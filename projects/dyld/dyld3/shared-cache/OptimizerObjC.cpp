/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- 
 *
 * Copyright (c) 2014 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#include <dirent.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <assert.h>

#include "DyldSharedCache.h"
#include "Diagnostics.h"
#include "SharedCacheBuilder.h"
#include "FileAbstraction.hpp"
#include "MachOFileAbstraction.hpp"
#include "MachOLoaded.h"
#include "MachOAnalyzer.h"
#include "MachOAnalyzerSet.h"

#ifndef MH_HAS_OBJC
  #define MH_HAS_OBJC            0x40000000
#endif

// Scan a C++ or Swift length-mangled field.
static bool scanMangledField(const char *&string, const char *end, 
                             const char *&field, int& length)
{
    // Leading zero not allowed.
    if (*string == '0') return false;

    length = 0;
    field = string;
    while (field < end) {
        char c = *field;
        if (!isdigit(c)) break;
        field++;
        if (__builtin_smul_overflow(length, 10, &length)) return false;
        if (__builtin_sadd_overflow(length, c - '0', &length)) return false;
    }

    string = field + length;
    return length > 0  &&  string <= end;
}


// copySwiftDemangledName
// Returns the pretty form of the given Swift-mangled class or protocol name. 
// Returns nullptr if the string doesn't look like a mangled Swift name.
// The result must be freed with free().
static char *copySwiftDemangledName(const char *string, bool isProtocol = false)
{
    if (!string) return nullptr;

    // Swift mangling prefix.
    if (strncmp(string, isProtocol ? "_TtP" : "_TtC", 4) != 0) return nullptr;
    string += 4;

    const char *end = string + strlen(string);

    // Module name.
    const char *prefix;
    int prefixLength;
    if (string[0] == 's') {
        // "s" is the Swift module.
        prefix = "Swift";
        prefixLength = 5;
        string += 1;
    } else {
        if (! scanMangledField(string, end, prefix, prefixLength)) return nullptr;
    }

    // Class or protocol name.
    const char *suffix;
    int suffixLength;
    if (! scanMangledField(string, end, suffix, suffixLength)) return nullptr;

    if (isProtocol) {
        // Remainder must be "_".
        if (strcmp(string, "_") != 0) return nullptr;
    } else {
        // Remainder must be empty.
        if (string != end) return nullptr;
    }

    char *result;
    asprintf(&result, "%.*s.%.*s", prefixLength,prefix, suffixLength,suffix);
    return result;
}


class ContentAccessor {
public:
    ContentAccessor(const DyldSharedCache* cache, Diagnostics& diag)
        : _diagnostics(diag)
    {
        _cacheStart         = (uint8_t*)cache;
        _cacheUnslideAddr   = cache->unslidLoadAddress();
        _slide              = (uint64_t)cache - _cacheUnslideAddr;
     }

    // Converts from an on disk vmAddr to the real vmAddr
    // That is, for a chained fixup, decodes the chain, for a non-chained fixup, does nothing.
    uint64_t vmAddrForOnDiskVMAddr(uint64_t vmaddr) {
        return vmaddr;
    }

    void* contentForVMAddr(uint64_t vmaddr) {
        vmaddr = vmAddrForOnDiskVMAddr(vmaddr);
        if ( vmaddr != 0 ) {
            uint64_t offset = vmaddr - _cacheUnslideAddr;
            return _cacheStart + offset;
        } else
            return nullptr;
    }

    uint64_t vmAddrForContent(const void* content) {
        if ( content != nullptr )
            return _cacheUnslideAddr + ((uint8_t*)content - _cacheStart);
        else
            return 0;
    }

    Diagnostics& diagnostics() { return _diagnostics; }

private:
    Diagnostics&                    _diagnostics;
    uint64_t                        _slide;
    uint64_t                        _cacheUnslideAddr;
    uint8_t*                        _cacheStart;
};


// Access a section containing a list of pointers
template <typename P, typename T>
class PointerSection 
{
    typedef typename P::uint_t   pint_t;
public:
    PointerSection(ContentAccessor* cache, const macho_header<P>* mh,
                   const char* segname, const char* sectname)
        : _cache(cache),
          _section(mh->getSection(segname, sectname)),
          _base(_section ? (pint_t*)cache->contentForVMAddr(_section->addr()) : 0),
          _count(_section ? (pint_t)(_section->size() / sizeof(pint_t)) : 0) {
    }

    pint_t count() const { return _count; }

    pint_t getVMAddress(pint_t index) const {
        if ( index >= _count ) {
            _cache->diagnostics().error("index out of range in section %s", _section->sectname());
            return 0;
        }
        return (pint_t)P::getP(_base[index]);
    }

    pint_t getSectionVMAddress() const {
        return (pint_t)_section->addr();
    }

    T get(pint_t index) const {
        return (T)_cache->contentForVMAddr(getVMAddress(index));
    }

    void setVMAddress(pint_t index, pint_t value) {
        if ( index >= _count ) {
            _cache->diagnostics().error("index out of range in section %s", _section->sectname());
            return;
        }
        P::setP(_base[index], value);
    }

    void removeNulls() {
        pint_t shift = 0;
        for (pint_t i = 0; i < _count; i++) {
            pint_t value = _base[i];
            if (value) {
                _base[i-shift] = value;
            } else {
                shift++;
            }
        }
        _count -= shift;
        const_cast<macho_section<P>*>(_section)->set_size(_count * sizeof(pint_t));
    }

private:
    ContentAccessor* const         _cache;
    const macho_section<P>* const  _section;
    pint_t* const                  _base;
    pint_t const                   _count;
};


// Access a section containing an array of structures
template <typename P, typename T>
class ArraySection 
{
public:
    ArraySection(ContentAccessor* cache, const macho_header<P>* mh,
                 const char *segname, const char *sectname)
        : _cache(cache),
          _section(mh->getSection(segname, sectname)),
          _base(_section ? (T *)cache->contentForVMAddr(_section->addr()) : 0),
          _count(_section ? _section->size() / sizeof(T) : 0) {
    }

    uint64_t count() const { return _count; }

    T& get(uint64_t index) const { 
        if (index >= _count) {
            _cache->diagnostics().error("index out of range in section %s", _section->sectname());
        }
        return _base[index];
    }

private:
    ContentAccessor* const         _cache;
    const macho_section<P>* const  _section;
    T * const                      _base;
    uint64_t const                 _count;
};


#define SELOPT_WRITE
#include "objc-shared-cache.h"
#include "ObjC1Abstraction.hpp"
#include "ObjC2Abstraction.hpp"


namespace {



template <typename P>
class ObjCSelectorUniquer
{
public:
    typedef typename P::uint_t  pint_t;

    ObjCSelectorUniquer(ContentAccessor* cache) : _cache(cache) { }

    pint_t visit(pint_t oldValue)
    {
        _count++;
        const char *s = (const char *)_cache->contentForVMAddr(oldValue);
        oldValue = (pint_t)_cache->vmAddrForOnDiskVMAddr(oldValue);
        objc_opt::string_map::iterator element = 
            _selectorStrings.insert(objc_opt::string_map::value_type(s, oldValue)).first;
        return (pint_t)element->second;
    }

    void visitCoalescedStrings(const CacheBuilder::CacheCoalescedText& coalescedText) {
        const CacheBuilder::CacheCoalescedText::StringSection& methodNames = coalescedText.getSectionData("__objc_methname");
        for (const auto& stringAndOffset : methodNames.stringsToOffsets) {
            uint64_t vmAddr = methodNames.bufferVMAddr + stringAndOffset.second;
            _selectorStrings[stringAndOffset.first.data()] = vmAddr;
        }
    }

    objc_opt::string_map& strings() { 
        return _selectorStrings;
    }

    size_t count() const { return _count; }

private:
    objc_opt::string_map    _selectorStrings;
    ContentAccessor*        _cache;
    size_t                  _count = 0;
};


template <typename P>
class ClassListBuilder
{
private:
    objc_opt::string_map    _classNames;
    objc_opt::class_map     _classes;
    size_t                  _count = 0;
    HeaderInfoOptimizer<P, objc_header_info_ro_t<P>>& _hInfos;

public:

    ClassListBuilder(HeaderInfoOptimizer<P, objc_header_info_ro_t<P>>& hinfos) : _hInfos(hinfos) { }

    void visitClass(ContentAccessor* cache,
                    const macho_header<P>* header,
                    objc_class_t<P>* cls)
    {
        if (cls->isMetaClass(cache)) return;

        const char* name = cls->getName(cache);
        uint64_t name_vmaddr = cache->vmAddrForContent((void*)name);
        uint64_t cls_vmaddr = cache->vmAddrForContent(cls);
        uint64_t hinfo_vmaddr = cache->vmAddrForContent(_hInfos.hinfoForHeader(cache, header));
        _classNames.insert(objc_opt::string_map::value_type(name, name_vmaddr));
        _classes.insert(objc_opt::class_map::value_type(name, std::pair<uint64_t, uint64_t>(cls_vmaddr, hinfo_vmaddr)));
        _count++;
    }

    objc_opt::string_map& classNames() { 
        return _classNames;
    }

    objc_opt::class_map& classes() { 
        return _classes;
    }

    size_t count() const { return _count; }
};


/// Builds a map from (install name, class name, method name) to actual IMPs
template <typename P>
class IMPMapBuilder
{
private:
    typedef typename P::uint_t pint_t;

public:

    struct MapKey {
        std::string_view installName;
        std::string_view className;
        std::string_view methodName;
        bool isInstanceMethod;

        bool operator==(const MapKey& other) const {
            return isInstanceMethod == other.isInstanceMethod &&
                    installName == other.installName &&
                    className == other.className &&
                    methodName == other.methodName;
        }

        size_t hash() const {
            std::size_t seed = 0;
            seed ^= std::hash<std::string_view>()(installName) + 0x9e3779b9 + (seed<<6) + (seed>>2);
            seed ^= std::hash<std::string_view>()(className) + 0x9e3779b9 + (seed<<6) + (seed>>2);
            seed ^= std::hash<std::string_view>()(methodName) + 0x9e3779b9 + (seed<<6) + (seed>>2);
            seed ^= std::hash<bool>()(isInstanceMethod) + 0x9e3779b9 + (seed<<6) + (seed>>2);
            return seed;
        }
    };

    struct MapKeyHasher {
        size_t operator()(const MapKey& k) const {
            return k.hash();
        }
    };

    std::unordered_map<MapKey, pint_t, MapKeyHasher> impMap;
    bool relativeMethodListSelectorsAreDirect;

    IMPMapBuilder(bool relativeMethodListSelectorsAreDirect)
        : relativeMethodListSelectorsAreDirect(relativeMethodListSelectorsAreDirect) { }

    void visitClass(ContentAccessor* cache,
                    const macho_header<P>* header,
                    objc_class_t<P>* cls)
    {
        objc_method_list_t<P> *methodList = cls->getMethodList(cache);
        if (methodList == nullptr) return;

        const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)header;
        bool isInstanceMethod = !cls->isMetaClass(cache);
        const char* className = cls->getName(cache);
        const char* installName = ma->installName();

        for (uint32_t n = 0; n < methodList->getCount(); n++) {
            // do not clobber an existing entry if any, because categories win
            impMap.try_emplace(MapKey{
                .installName = installName,
                .className = className,
                .methodName = methodList->getStringName(cache, n, relativeMethodListSelectorsAreDirect),
                .isInstanceMethod = isInstanceMethod
            }, methodList->getImp(n, cache));
        }
    }

    void visit(ContentAccessor* cache, const macho_header<P>* header) {
        const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)header;

        // Method lists from categories
        PointerSection<P, objc_category_t<P> *>
            cats(cache, header, "__DATA", "__objc_catlist");
        for (pint_t i = 0; i < cats.count(); i++) {
            objc_category_t<P> *cat = cats.get(i);
            objc_class_t<P>* cls = cat->getClass(cache);
            if (cls == nullptr)
                continue;

            objc_method_list_t<P> *instanceMethods = cat->getInstanceMethods(cache);
            if (instanceMethods != nullptr) {
                for (uint32_t n = 0; n < instanceMethods->getCount(); n++) {
                    MapKey k {
                        .installName = ma->installName(),
                        .className = cls->getName(cache),
                        .methodName = instanceMethods->getStringName(cache, n, relativeMethodListSelectorsAreDirect),
                        .isInstanceMethod = true
                    };
                    //printf("Adding %s %s %s %d cat %s\n", k.installName.data(), k.className.data(), k.methodName.data(), k.isInstanceMethod, k.catName->data());
                    impMap[k] = instanceMethods->getImp(n, cache);
                }
            }
            objc_method_list_t<P> *classMethods = cat->getClassMethods(cache);
            if (classMethods != nullptr) {
                for (uint32_t n = 0; n < classMethods->getCount(); n++) {
                    MapKey k {
                        .installName = ma->installName(),
                        .className = cls->getName(cache),
                        .methodName = classMethods->getStringName(cache, n, relativeMethodListSelectorsAreDirect),
                        .isInstanceMethod = false
                    };
                    //printf("Adding %s %s %s %d cat %s\n", k.installName.data(), k.className.data(), k.methodName.data(), k.isInstanceMethod, k.catName->data());
                    impMap[k] = classMethods->getImp(n, cache);
                }
            }
        }
    }
};

// List of offsets in libobjc that the shared cache optimization needs to use.
template <typename T>
struct objc_opt_imp_caches_pointerlist_tt {
    T selectorStringVMAddrStart;
    T selectorStringVMAddrEnd;
    T inlinedSelectorsVMAddrStart;
    T inlinedSelectorsVMAddrEnd;
};

template <typename P>
class IMPCachesEmitter
{
    typedef typename P::uint_t pint_t;

private:
    Diagnostics&                 diag;
    const IMPMapBuilder<P>&      impMapBuilder;
    uint64_t                     selectorStringVMAddr;
    uint8_t*&                    readOnlyBuffer;
    size_t&                      readOnlyBufferSize;
    uint8_t*&                    readWriteBuffer;
    size_t&                      readWriteBufferSize;
    CacheBuilder::ASLR_Tracker& aslrTracker;

    std::map<std::string_view, const CacheBuilder::DylibInfo*> _dylibInfos;
    std::map<std::string_view, const macho_header<P>*> _dylibs;
    const std::vector<const IMPCaches::Selector*> inlinedSelectors;

    struct ImpCacheHeader {
        int32_t  fallback_class_offset;
        uint32_t cache_shift :  5;
        uint32_t cache_mask  : 11;
        uint32_t occupied    : 14;
        uint32_t has_inlines :  1;
        uint32_t bit_one     :  1;
    };

    struct ImpCacheEntry {
        uint32_t selOffset;
        uint32_t impOffset;
    };

public:

    static size_t sizeForImpCacheWithCount(int entries) {
        return sizeof(ImpCacheHeader) + entries * sizeof(ImpCacheEntry);
    }

    struct ImpCacheContents {
        struct bucket_t {
            uint32_t sel_offset = 0;
            uint64_t imp = 0;
        };
        std::vector<bucket_t>   buckets;
        uint64_t                occupiedBuckets = 0;
        bool hasInlines = false;

        uint64_t capacity() const
        {
            return buckets.size();
        }

        uint64_t occupied() const {
            return occupiedBuckets;
        }

        void incrementOccupied() {
            ++occupiedBuckets;
        }

        void insert(uint64_t slot, uint64_t selOffset, uint64_t imp) {
            bucket_t& b = buckets[slot];
            assert(b.imp == 0);

            if (!b.imp) incrementOccupied();
            assert((uint32_t)selOffset == selOffset);
            b.sel_offset = (uint32_t)selOffset;
            b.imp = imp;
        }

        void fillBuckets(const IMPCaches::ClassData* classData, bool metaclass, const IMPMapBuilder<P> & classRecorder) {
            const std::vector<IMPCaches::ClassData::Method> & methods = classData->methods;
            buckets.resize(classData->modulo());
            for (const IMPCaches::ClassData::Method& method : methods) {
                typename IMPMapBuilder<P>::MapKey k {
                    .installName = method.installName,
                    .className = method.className,
                    .methodName = method.selector->name,
                    .isInstanceMethod = !metaclass
                };

                pint_t imp = classRecorder.impMap.at(k);
                int slot = (method.selector->inProgressBucketIndex >> classData->shift) & classData->mask();
                insert(slot, method.selector->offset, imp);
                hasInlines |= (method.wasInlined && !method.fromFlattening);
            }
        }

        std::pair<uint64_t, uint64_t>
        write(ContentAccessor* cache,
              uint64_t cacheSelectorStringVMAddr, uint64_t clsVMAddr,
              uint8_t*& buf, size_t& bufSize, Diagnostics& diags) {
            constexpr bool log = false;
            uint64_t spaceRequired = sizeof(ImpCacheEntry) * capacity();

            if (spaceRequired > bufSize) {
                diags.error("Not enough space for imp cache");
                return { 0, 0 };
            }

            // Convert from addresses to offsets and write out
            ImpCacheEntry* offsetBuckets = (ImpCacheEntry*)buf;
            // printf("Buckets: 0x%08llx\n", cache->vmAddrForContent(offsetBuckets));
            for (uint64_t index = 0; index != buckets.size(); ++index) {
                bucket_t bucket = buckets[index];
                if (bucket.sel_offset == 0 && bucket.imp == 0) {
                    // Empty bucket
                    offsetBuckets[index].selOffset = 0xFFFFFFFF;
                    offsetBuckets[index].impOffset = 0;
                } else {
                    int64_t selOffset = (int64_t)bucket.sel_offset;
                    int64_t impOffset = clsVMAddr - bucket.imp;
                    assert((int32_t)impOffset == impOffset);
                    assert((int32_t)selOffset == selOffset);
                    offsetBuckets[index].selOffset = (int32_t)selOffset;
                    offsetBuckets[index].impOffset = (int32_t)impOffset;
                    if (log) {
                        diags.verbose("[IMP Caches] Coder[%lld]: %#08llx (sel: %#08x, imp %#08x) %s\n", index,
                           cache->vmAddrForOnDiskVMAddr(bucket.imp),
                           (int32_t)selOffset, (int32_t)impOffset,
                           (const char*)cache->contentForVMAddr(cacheSelectorStringVMAddr + bucket.sel_offset));
                    }
                }
            }

            buf += spaceRequired;
            bufSize -= spaceRequired;

            return { cache->vmAddrForContent(offsetBuckets), (uint64_t)buckets.size() };
        }
    };

    IMPCachesEmitter(Diagnostics& diags, const IMPMapBuilder<P>& builder, uint64_t selectorStringVMAddr, uint8_t*& roBuf, size_t& roBufSize, uint8_t* &rwBuf, size_t& rwBufSize, const std::vector<CacheBuilder::DylibInfo> & dylibInfos, const std::vector<const macho_header<P>*> & dylibs, CacheBuilder::ASLR_Tracker& tracker)
        : diag(diags), impMapBuilder(builder), selectorStringVMAddr(selectorStringVMAddr), readOnlyBuffer(roBuf), readOnlyBufferSize(roBufSize), readWriteBuffer(rwBuf), readWriteBufferSize(rwBufSize), aslrTracker(tracker) {
            for (const CacheBuilder::DylibInfo& d : dylibInfos) {
                _dylibInfos[d.dylibID] = &d;
            }
            for (const macho_header<P>* d : dylibs) {
                const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*) d;
                _dylibs[ma->installName()] = d;
            }
        }

    // Returns true if we should filter this class out from getting an imp cache
    bool filter(ContentAccessor* cache, const dyld3::MachOAnalyzer* ma, const objc_class_t<P>* cls) {
        const CacheBuilder::DylibInfo* d = _dylibInfos[ma->installName()];
        IMPCaches::ClassKey key {
            .name = cls->getName(cache),
            .metaclass = cls->isMetaClass(cache)
        };
        return (d->impCachesClassData.find(key) == d->impCachesClassData.end());
    }

    void visitClass(ContentAccessor* cache,
                    const macho_header<P>* header,
                    objc_class_t<P>* cls)
    {
        // If we ran out of space then don't try to optimize more
        if (diag.hasError())
            return;

        const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*) header;
        if (filter(cache, ma, cls)) {
            *cls->getVTableAddress() = 0;
            return;
        }

        const char* className = cls->getName(cache);

        if (cls->getVTable(cache) != 0) {
            diag.error("Class '%s' has non-zero vtable\n", className);
            return;
        }

        const CacheBuilder::DylibInfo* d = _dylibInfos[ma->installName()];
        IMPCaches::ClassKey key {
            .name = cls->getName(cache),
            .metaclass = cls->isMetaClass(cache)
        };
        IMPCaches::ClassData* data = (d->impCachesClassData.at(key)).get();
#if 0
        for (const objc_method_t<P>& method : methods) {
            printf("  0x%llx: 0x%llx (%s)\n", method.getImp(), method.getName(),
                   (const char*)cache->contentForVMAddr(method.getName()));
        }
#endif

        uint64_t clsVMAddr = cache->vmAddrForContent(cls);

        if (data->mask() > 0x7ff) {
            diag.verbose("Cache for class %s (%#08llx) is too large (mask: %#x)\n",
                         className, clsVMAddr, data->mask());
            return;
        }

        ImpCacheContents impCache;
        impCache.fillBuckets(data, cls->isMetaClass(cache), impMapBuilder);

        constexpr bool log = false;
        if (log) {
            printf("Writing cache for %sclass %s (%#08llx)\n", cls->isMetaClass(cache) ? "meta" : "", className, clsVMAddr);
        }

        struct ImpCacheHeader {
            int32_t  fallback_class_offset;
            uint32_t cache_shift :  5;
            uint32_t cache_mask  : 11;
            uint32_t occupied    : 14;
            uint32_t has_inlines :  1;
            uint32_t bit_one     :  1;
        };
        pint_t* vtableAddr = cls->getVTableAddress();

        // the alignment of ImpCaches to 16 bytes is only needed for arm64_32.
        ImpCacheHeader* cachePtr = (ImpCacheHeader*)align_buffer(readOnlyBuffer, sizeof(pint_t) == 4 ? 4 : 3);

        assert(readOnlyBufferSize > sizeof(ImpCacheHeader));

        uint64_t occupied = impCache.occupied();
        int64_t fallback_class_offset = *(cls->getSuperClassAddress()) - clsVMAddr;

        if (data->flatteningRootSuperclass) {
            // If we are a class being flattened (inheriting all the selectors of
            // its superclasses up to and including the flattening root), the fallback class
            // should be the first superclass which is not flattened.
            
            // Find the VMAddr of that superclass, given its segment index and offset
            // in the source dylib.
            const auto & superclass = *(data->flatteningRootSuperclass);
            const macho_header<P> * d = _dylibs[superclass.installName];
            __block uint64_t superclassVMAddr = 0;
            const dyld3::MachOAnalyzer *ma = (const dyld3::MachOAnalyzer *)d;
            ma->forEachSegment(^(const dyld3::MachOAnalyzer::SegmentInfo &info, bool &stop) {
                if (info.segIndex == superclass.segmentIndex) {
                    superclassVMAddr = info.vmAddr + superclass.segmentOffset;
                    stop = true;
                }
            });

            assert(superclassVMAddr > 0);
            fallback_class_offset = superclassVMAddr - clsVMAddr;
        }

        assert((int32_t)fallback_class_offset == fallback_class_offset);
        assert((uint32_t)occupied == occupied);

        *cachePtr = (ImpCacheHeader){
            .fallback_class_offset = (int32_t)fallback_class_offset,
            .cache_shift = (uint32_t)(data->shift + 7),
            .cache_mask = (uint32_t)data->mask(),
            .occupied = (uint32_t)occupied,
            .has_inlines = impCache.hasInlines,
            .bit_one = 1, // obj-c plays HORRENDOUS games here
        };

        // is this right?
        int64_t vmaddr = cache->vmAddrForContent(readOnlyBuffer);
        assert((pint_t)vmaddr == (uint64_t)vmaddr);
        *vtableAddr =  (pint_t)cache->vmAddrForContent(readOnlyBuffer);
        aslrTracker.add(vtableAddr);
        readOnlyBuffer += sizeof(ImpCacheHeader);
        readOnlyBufferSize -= sizeof(ImpCacheHeader);

        impCache.write(cache, selectorStringVMAddr, clsVMAddr, readOnlyBuffer, readOnlyBufferSize, diag);
    }

    void emitInlinedSelectors(const std::vector<const IMPCaches::Selector*> selectors) {
        // FIXME: this should be in constant memory
        for (const IMPCaches::Selector* s : selectors) {
            assert(readWriteBufferSize >= sizeof(pint_t));
            *(pint_t*)readWriteBuffer = (pint_t)(selectorStringVMAddr + s->offset);
            aslrTracker.add(readWriteBuffer);
            readWriteBuffer += sizeof(pint_t);
            readWriteBufferSize -= sizeof(pint_t);
        }
    }
};

template <typename P>
class ProtocolOptimizer
{
private:
    typedef typename P::uint_t pint_t;

    objc_opt::string_map                                _protocolNames;
    objc_opt::legacy_protocol_map                       _protocols;
    objc_opt::protocol_map                              _protocolsAndHeaders;
    size_t                                              _protocolCount;
    size_t                                              _protocolReferenceCount;
    Diagnostics&                                        _diagnostics;
    HeaderInfoOptimizer<P, objc_header_info_ro_t<P>>&   _hInfos;

    friend class ProtocolReferenceWalker<P, ProtocolOptimizer<P>>;

    pint_t visitProtocolReference(ContentAccessor* cache, pint_t oldValue)
    {
        objc_protocol_t<P>* proto = (objc_protocol_t<P>*)
            cache->contentForVMAddr(oldValue);
        pint_t newValue = (pint_t)_protocols[proto->getName(cache)];
        if (oldValue != newValue) _protocolReferenceCount++;
        return newValue;
    }

public:

    ProtocolOptimizer(Diagnostics& diag, HeaderInfoOptimizer<P, objc_header_info_ro_t<P>>& hinfos)
        : _protocolCount(0), _protocolReferenceCount(0), _diagnostics(diag), _hInfos(hinfos) {
    }

    void addProtocols(ContentAccessor* cache, const macho_header<P>* header)
    {
        PointerSection<P, objc_protocol_t<P> *>
            protocols(cache, header, "__DATA", "__objc_protolist");
        
        for (pint_t i = 0; i < protocols.count(); i++) {
            objc_protocol_t<P> *proto = protocols.get(i);

            const char* name = proto->getName(cache);
            if (_protocolNames.count(name) == 0) {
                if (proto->getSize() > sizeof(objc_protocol_t<P>)) {
                    _diagnostics.error("objc protocol is too big");
                    return;
                }
                uint64_t name_vmaddr = cache->vmAddrForContent((void*)name);
                uint64_t proto_vmaddr = cache->vmAddrForContent(proto);
                _protocolNames.insert(objc_opt::string_map::value_type(name, name_vmaddr));
                _protocols.insert(objc_opt::legacy_protocol_map::value_type(name, proto_vmaddr));
                _protocolCount++;
            }

            // Note down which header this protocol came from.  We'll fill in the proto_vmaddr here later
            // once we've chosen a single definition for the protocol with this name.
            uint64_t hinfo_vmaddr = cache->vmAddrForContent(_hInfos.hinfoForHeader(cache, header));
            _protocolsAndHeaders.insert(objc_opt::class_map::value_type(name, std::pair<uint64_t, uint64_t>(0, hinfo_vmaddr)));
        }
    }

    const char *writeProtocols(ContentAccessor* cache,
                               uint8_t *& rwdest, size_t& rwremaining,
                               uint8_t *& rodest, size_t& roremaining, 
                               CacheBuilder::ASLR_Tracker& aslrTracker,
                               pint_t protocolClassVMAddr,
                               const dyld3::MachOAnalyzerSet::PointerMetaData& PMD)
    {
        if (_protocolCount == 0) return NULL;

        if (protocolClassVMAddr == 0) {
            return "libobjc's Protocol class symbol not found (metadata not optimized)";
        }

        size_t rwrequired = _protocolCount * sizeof(objc_protocol_t<P>);
        if (rwremaining < rwrequired) {
            return "libobjc's read-write section is too small (metadata not optimized)";
        }

        for (auto iter = _protocols.begin(); iter != _protocols.end(); ++iter)
        {
            objc_protocol_t<P>* oldProto = (objc_protocol_t<P>*)
                cache->contentForVMAddr(iter->second);

            // Create a new protocol object.
            objc_protocol_t<P>* proto = (objc_protocol_t<P>*)rwdest;
            rwdest += sizeof(*proto);
            rwremaining -= sizeof(*proto);

            // Initialize it.
            uint32_t oldSize = oldProto->getSize();
            memcpy(proto, oldProto, oldSize);
            if (!proto->getIsaVMAddr()) {
                proto->setIsaVMAddr(protocolClassVMAddr);
            }

            // If the objc runtime signed the Protocol ISA, then we need to too
            if ( PMD.authenticated ) {
                aslrTracker.setAuthData(proto->getISALocation(), PMD.diversity, PMD.usesAddrDiversity, PMD.key);
            }

            if (oldSize < sizeof(*proto)) {
                // Protocol object is old. Populate new fields.
                proto->setSize(sizeof(objc_protocol_t<P>));
                // missing extendedMethodTypes is already nil
            }
            // Some protocol objects are big enough to have the 
            // demangledName field but don't initialize it.
            // Initialize it here if it is not already set.
            if (!proto->getDemangledName(cache)) {
                const char *roName = proto->getName(cache);
                char *demangledName = copySwiftDemangledName(roName, true);
                if (demangledName) {
                    size_t length = 1 + strlen(demangledName);
                    if (roremaining < length) {
                        return "libobjc's read-only section is too small (metadata not optimized)";
                    }

                    memmove(rodest, demangledName, length);
                    roName = (const char *)rodest;
                    rodest += length;
                    roremaining -= length;

                    free(demangledName);
                }
                proto->setDemangledName(cache, roName, _diagnostics);
            }
            proto->setFixedUp();
            proto->setIsCanonical();

            // Redirect the protocol table at our new object.
            iter->second = cache->vmAddrForContent(proto);

            // Add new rebase entries.
            proto->addPointers(cache, aslrTracker);
        }

        // Now that we've chosen the canonical protocols, set the duplicate headers to
        // point to their protocols.
        for (auto iter = _protocolsAndHeaders.begin(); iter != _protocolsAndHeaders.end(); ++iter) {
            iter->second.first = _protocols[iter->first];
        }
        
        return NULL;
    }

    void updateReferences(ContentAccessor* cache, const macho_header<P>* header)
    {
        ProtocolReferenceWalker<P, ProtocolOptimizer<P>> refs(*this);
        refs.walk(cache, header);
    }

    objc_opt::string_map& protocolNames() {
        return _protocolNames;
    }

    objc_opt::legacy_protocol_map& protocols() {
        return _protocols;
    }

    objc_opt::protocol_map& protocolsAndHeaders() {
        return _protocolsAndHeaders;
    }

    size_t protocolCount() const { return _protocolCount; }
    size_t protocolReferenceCount() const { return _protocolReferenceCount; }
};


static int percent(size_t num, size_t denom) {
    if (denom)
        return (int)(num / (double)denom * 100);
    else
        return 100;
}

template <typename P>
void addObjcSegments(Diagnostics& diag, DyldSharedCache* cache, const mach_header* libobjcMH,
                     uint8_t* objcReadOnlyBuffer, uint64_t objcReadOnlyBufferSizeAllocated,
                     uint8_t* objcReadWriteBuffer, uint64_t objcReadWriteBufferSizeAllocated,
                     uint64_t objcRwFileOffset)
{
    // validate there is enough free space to add the load commands
    const dyld3::MachOAnalyzer* libobjcMA = ((dyld3::MachOAnalyzer*)libobjcMH);
    uint32_t freeSpace = libobjcMA->loadCommandsFreeSpace();
    const uint32_t segSize = sizeof(macho_segment_command<P>);
    if ( freeSpace < 2*segSize ) {
        diag.warning("not enough space in libojbc.dylib to add load commands for objc optimization regions");
        return;
    }

    // find location of LINKEDIT LC_SEGMENT load command, we need to insert new segments before it
    __block uint8_t* linkeditSeg = nullptr;
    libobjcMA->forEachSegment(^(const dyld3::MachOFile::SegmentInfo& info, bool& stop) {
        if ( strcmp(info.segName, "__LINKEDIT") == 0 )
            linkeditSeg = (uint8_t*)libobjcMH + info.loadCommandOffset;
    });
    if ( linkeditSeg == nullptr ) {
        diag.warning("__LINKEDIT not found in libojbc.dylib");
        return;
    }

    // move load commands to make room to insert two new ones before LINKEDIT segment load command
    uint8_t* endOfLoadCommands = (uint8_t*)libobjcMH + sizeof(macho_header<P>) + libobjcMH->sizeofcmds;
    uint32_t remainingSize = (uint32_t)(endOfLoadCommands - linkeditSeg);
    memmove(linkeditSeg+2*segSize, linkeditSeg, remainingSize);

    // insert new segments
    macho_segment_command<P>* roSeg = (macho_segment_command<P>*)(linkeditSeg);
    macho_segment_command<P>* rwSeg = (macho_segment_command<P>*)(linkeditSeg+sizeof(macho_segment_command<P>));
    roSeg->set_cmd(macho_segment_command<P>::CMD);
    roSeg->set_cmdsize(segSize);
    roSeg->set_segname("__OBJC_RO");
    roSeg->set_vmaddr(cache->unslidLoadAddress() + objcReadOnlyBuffer - (uint8_t*)cache);
    roSeg->set_vmsize(objcReadOnlyBufferSizeAllocated);
    roSeg->set_fileoff(objcReadOnlyBuffer - (uint8_t*)cache);
    roSeg->set_filesize(objcReadOnlyBufferSizeAllocated);
    roSeg->set_maxprot(VM_PROT_READ);
    roSeg->set_initprot(VM_PROT_READ);
    roSeg->set_nsects(0);
    roSeg->set_flags(0);
    rwSeg->set_cmd(macho_segment_command<P>::CMD);
    rwSeg->set_cmdsize(segSize);
    rwSeg->set_segname("__OBJC_RW");
    rwSeg->set_vmaddr(cache->unslidLoadAddress() + objcReadWriteBuffer - (uint8_t*)cache);
    rwSeg->set_vmsize(objcReadWriteBufferSizeAllocated);
    rwSeg->set_fileoff(objcRwFileOffset);
    rwSeg->set_filesize(objcReadWriteBufferSizeAllocated);
    rwSeg->set_maxprot(VM_PROT_WRITE|VM_PROT_READ);
    rwSeg->set_initprot(VM_PROT_WRITE|VM_PROT_READ);
    rwSeg->set_nsects(0);
    rwSeg->set_flags(0);

    // update mach_header to account for new load commands
    macho_header<P>* mh = (macho_header<P>*)libobjcMH;
    mh->set_sizeofcmds(mh->sizeofcmds() + 2*segSize);
    mh->set_ncmds(mh->ncmds()+2);

    // fix up table at start of dyld cache that has pointer into install name for libobjc
    dyld_cache_image_info* images = (dyld_cache_image_info*)((uint8_t*)cache + cache->header.imagesOffset);
    uint64_t libobjcUnslidAddress = cache->unslidLoadAddress() + ((uint8_t*)libobjcMH - (uint8_t*)cache);
    for (uint32_t i=0; i < cache->header.imagesCount; ++i) {
        if ( images[i].address == libobjcUnslidAddress ) {
            images[i].pathFileOffset += (2*segSize);
            break;
        }
    }
}

template <typename P> static inline void emitIMPCaches(ContentAccessor& cacheAccessor,
                                         std::vector<CacheBuilder::DylibInfo> & allDylibs,
                                         std::vector<const macho_header<P>*> & sizeSortedDylibs,
                                         bool relativeMethodListSelectorsAreDirect,
                                         uint64_t selectorStringVMAddr,
                                         uint8_t* optROData, size_t& optRORemaining,
                                         uint8_t* optRWData, size_t& optRWRemaining,
                                         CacheBuilder::ASLR_Tracker& aslrTracker,
                                         const std::vector<const IMPCaches::Selector*> & inlinedSelectors,
                                         uint8_t* &inlinedSelectorsStart,
                                         uint8_t* &inlinedSelectorsEnd,
                                         Diagnostics& diag,
                                         TimeRecorder& timeRecorder) {
    diag.verbose("[IMP caches] computing IMP map\n");

    IMPMapBuilder<P> classRecorder(relativeMethodListSelectorsAreDirect);
    for (const macho_header<P>* mh : sizeSortedDylibs) {
        ClassWalker<P, IMPMapBuilder<P>> classWalker(classRecorder, ClassWalkerMode::ClassAndMetaclasses);
        classWalker.walk(&cacheAccessor, mh);
        classRecorder.visit(&cacheAccessor, mh);
    }

    timeRecorder.recordTime("compute IMP map");
    diag.verbose("[IMP caches] emitting IMP caches\n");

    IMPCachesEmitter<P> impCachesEmitter(diag, classRecorder, selectorStringVMAddr, optROData, optRORemaining, optRWData, optRWRemaining, allDylibs, sizeSortedDylibs, aslrTracker);
    ClassWalker<P, IMPCachesEmitter<P>> impEmitterClassWalker(impCachesEmitter, ClassWalkerMode::ClassAndMetaclasses);
    for (const macho_header<P>* mh : sizeSortedDylibs) {
        impEmitterClassWalker.walk(&cacheAccessor, mh);
        if (diag.hasError())
            return;
    }

    inlinedSelectorsStart = optRWData;
    impCachesEmitter.emitInlinedSelectors(inlinedSelectors);
    inlinedSelectorsEnd = optRWData;
}

template <typename P>
void doOptimizeObjC(DyldSharedCache* cache, bool forProduction, CacheBuilder::ASLR_Tracker& aslrTracker,
                    CacheBuilder::LOH_Tracker& lohTracker, const CacheBuilder::CacheCoalescedText& coalescedText,
                    const std::map<void*, std::string>& missingWeakImports, Diagnostics& diag,
                    uint8_t* objcReadOnlyBuffer, uint64_t objcReadOnlyBufferSizeUsed, uint64_t objcReadOnlyBufferSizeAllocated,
                    uint8_t* objcReadWriteBuffer, uint64_t objcReadWriteBufferSizeAllocated,
                    uint64_t objcRwFileOffset,
                    std::vector<CacheBuilder::DylibInfo> & allDylibs,
                    const std::vector<const IMPCaches::Selector*> & inlinedSelectors,
                    bool impCachesSuccess,
                    TimeRecorder& timeRecorder)
{
    typedef typename P::E           E;
    typedef typename P::uint_t      pint_t;

    diag.verbose("Optimizing objc metadata:\n");
    diag.verbose("  cache type is %s\n", forProduction ? "production" : "development");

    ContentAccessor cacheAccessor(cache, diag);

    size_t headerSize = P::round_up(sizeof(objc_opt::objc_opt_t));
    if (headerSize != sizeof(objc_opt::objc_opt_t)) {
        diag.warning("libobjc's optimization structure size is wrong (metadata not optimized)");
    }

    //
    // Find libobjc's empty sections and build list of images with objc metadata
    //
    __block const mach_header*      libobjcMH = nullptr;
    __block const macho_section<P> *optROSection = nullptr;
    __block const macho_section<P> *optPointerListSection = nullptr;
    __block const macho_section<P> *optImpCachesPointerSection = nullptr;
    __block std::vector<const macho_header<P>*> objcDylibs;
    cache->forEachImage(^(const mach_header* machHeader, const char* installName) {
        const macho_header<P>* mh = (const macho_header<P>*)machHeader;
        if ( strstr(installName, "/libobjc.") != nullptr ) {
            libobjcMH = (mach_header*)mh;
            optROSection = mh->getSection("__TEXT", "__objc_opt_ro");
            optPointerListSection = mh->getSection("__DATA", "__objc_opt_ptrs");
            if ( optPointerListSection == nullptr )
                optPointerListSection = mh->getSection("__AUTH", "__objc_opt_ptrs");
            optImpCachesPointerSection = mh->getSection("__DATA_CONST", "__objc_scoffs");
        }
        if ( mh->getSection("__DATA", "__objc_imageinfo") || mh->getSection("__OBJC", "__image_info") ) {
            objcDylibs.push_back(mh);
        }
        // log("installName %s at mhdr 0x%016lx", installName, (uintptr_t)cacheAccessor.vmAddrForContent((void*)mh));
    });
    if ( optROSection == nullptr ) {
        diag.warning("libobjc's read-only section missing (metadata not optimized)");
        return;
    }
    if ( optPointerListSection == nullptr ) {
        diag.warning("libobjc's pointer list section missing (metadata not optimized)");
        return;
    }
    if ( optImpCachesPointerSection == nullptr ) {
        diag.warning("libobjc's magical shared cache offsets list section missing (metadata not optimized)");
    }
    // point optROData into space allocated in dyld cache
    uint8_t* optROData = objcReadOnlyBuffer + objcReadOnlyBufferSizeUsed;
    size_t optRORemaining = objcReadOnlyBufferSizeAllocated - objcReadOnlyBufferSizeUsed;
    *((uint32_t*)optROData) = objc_opt::VERSION;
    if ( optROData == nullptr ) {
        diag.warning("libobjc's read-only section has bad content");
        return;
    }

    uint8_t* optRWData = objcReadWriteBuffer;
    size_t optRWRemaining = objcReadWriteBufferSizeAllocated;
    if (optRORemaining < headerSize) {
        diag.warning("libobjc's read-only section is too small (metadata not optimized)");
        return;
    }
    objc_opt::objc_opt_t* optROHeader = (objc_opt::objc_opt_t *)optROData;
    optROData += headerSize;
    optRORemaining -= headerSize;
    if (E::get32(optROHeader->version) != objc_opt::VERSION) {
        diag.warning("libobjc's read-only section version is unrecognized (metadata not optimized)");
        return;
    }

    if (optPointerListSection->size() < sizeof(objc_opt::objc_opt_pointerlist_tt<pint_t>)) {
        diag.warning("libobjc's pointer list section is too small (metadata not optimized)");
        return;
    }
    const objc_opt::objc_opt_pointerlist_tt<pint_t> *optPointerList = (const objc_opt::objc_opt_pointerlist_tt<pint_t> *)cacheAccessor.contentForVMAddr(optPointerListSection->addr());

    // Write nothing to optROHeader until everything else is written.
    // If something fails below, libobjc will not use the section.


    //
    // Make copy of objcList and sort that list.
    //
    std::vector<const macho_header<P>*> addressSortedDylibs = objcDylibs;
    std::sort(addressSortedDylibs.begin(), addressSortedDylibs.end(), [](const macho_header<P>* lmh, const macho_header<P>* rmh) -> bool {
        return lmh < rmh;
    });

    //
    // Build HeaderInfo list in cache
    //
    // First the RO header info
    // log("writing out %d RO dylibs at offset %d", (uint32_t)objcDylibs.size(), (uint32_t)(optROSection->size() - optRORemaining));
    uint64_t hinfoROVMAddr = cacheAccessor.vmAddrForContent(optROData);
    HeaderInfoOptimizer<P, objc_header_info_ro_t<P>> hinfoROOptimizer;
    const char* err = hinfoROOptimizer.init((uint32_t)objcDylibs.size(), optROData, optRORemaining);
    if (err) {
        diag.warning("%s", err);
        return;
    }
    else {
        for (const macho_header<P>* mh : addressSortedDylibs) {
            hinfoROOptimizer.update(&cacheAccessor, mh, aslrTracker);
        }
    }

    // Then the RW header info
    // log("writing out %d RW dylibs at offset %d", (uint32_t)objcDylibs.size(), (uint32_t)(optRWSection->size() - optRWRemaining));
    uint64_t hinfoRWVMAddr = cacheAccessor.vmAddrForContent(optRWData);
    HeaderInfoOptimizer<P, objc_header_info_rw_t<P>> hinfoRWOptimizer;
    err = hinfoRWOptimizer.init((uint32_t)objcDylibs.size(), optRWData, optRWRemaining);
    if (err) {
        diag.warning("%s", err);
        return;
    }
    else {
        for (const macho_header<P>* mh : addressSortedDylibs) {
            hinfoRWOptimizer.update(&cacheAccessor, mh, aslrTracker);
        }
    }

    //
    // Update selector references and build selector list
    //
    // This is SAFE: if we run out of room for the selector table, 
    // the modified binaries are still usable.
    //
    // Heuristic: choose selectors from libraries with more selector cstring data first.
    // This tries to localize selector cstring memory.
    //
    ObjCSelectorUniquer<P> uniq(&cacheAccessor);
    std::vector<const macho_header<P>*> sizeSortedDylibs = objcDylibs;
    std::sort(sizeSortedDylibs.begin(), sizeSortedDylibs.end(),  [](const macho_header<P>* lmh, const macho_header<P>* rmh) -> bool {
        // Sort a select few heavy hitters first.
        auto getPriority = [](const char* installName) -> int {
            if (!strcmp(installName, "/usr/lib/libobjc.A.dylib"))
                return 0;
            if (!strcmp(installName, "/System/Library/Frameworks/Foundation.framework/Versions/C/Foundation") ||
                !strcmp(installName, "/System/Library/Frameworks/Foundation.framework/Foundation"))
                return 1;
            if (!strcmp(installName, "/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation") ||
                !strcmp(installName, "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation"))
                return 2;
            // Note we don't sort iOSMac UIKitCore early as we want iOSMac after macOS.
            if (!strcmp(installName, "/System/Library/PrivateFrameworks/UIKitCore.framework/UIKitCore"))
                return 3;
            if (!strcmp(installName, "/System/Library/Frameworks/AppKit.framework/Versions/C/AppKit"))
                return 4;
            if (!strcmp(installName, "/System/Library/Frameworks/CFNetwork.framework/Versions/A/CFNetwork") ||
                !strcmp(installName, "/System/Library/Frameworks/CFNetwork.framework/CFNetwork"))
                return 5;
            return INT_MAX;
        };

        // Sort by priority first
        int priorityA = getPriority(((const dyld3::MachOFile*)lmh)->installName());
        int priorityB = getPriority(((const dyld3::MachOFile*)rmh)->installName());
        if (priorityA != priorityB)
            return priorityA < priorityB;

        // Sort mac before iOSMac
        bool isIOSMacA = strncmp(((const dyld3::MachOFile*)lmh)->installName(), "/System/iOSSupport/", 19) == 0;
        bool isIOSMacB = strncmp(((const dyld3::MachOFile*)rmh)->installName(), "/System/iOSSupport/", 19) == 0;
        if (isIOSMacA != isIOSMacB)
            return !isIOSMacA;
        
        const macho_section<P>* lSection = lmh->getSection("__TEXT", "__objc_methname");
        const macho_section<P>* rSection = rmh->getSection("__TEXT", "__objc_methname");
        uint64_t lSelectorSize = (lSection ? lSection->size() : 0);
        uint64_t rSelectorSize = (rSection ? rSection->size() : 0);
        return lSelectorSize > rSelectorSize;
    });

    auto alignPointer = [](uint8_t* ptr) -> uint8_t* {
        return (uint8_t*)(((uintptr_t)ptr + 0x7) & ~0x7);
    };

    // Relative method lists names are initially an offset to a selector reference.
    // Eventually we'll update them to offsets directly to the selector string.
    bool relativeMethodListSelectorsAreDirect = false;

    SelectorOptimizer<P, ObjCSelectorUniquer<P> > selOptimizer(uniq, relativeMethodListSelectorsAreDirect);
    selOptimizer.visitCoalescedStrings(coalescedText);
    for (const macho_header<P>* mh : sizeSortedDylibs) {
        LegacySelectorUpdater<P, ObjCSelectorUniquer<P>>::update(&cacheAccessor, mh, uniq);
        selOptimizer.optimize(&cacheAccessor, mh);
    }

    diag.verbose("  uniqued  %6lu selectors\n", uniq.strings().size());
    diag.verbose("  updated  %6lu selector references\n", uniq.count());

    uint64_t seloptVMAddr = cacheAccessor.vmAddrForContent(optROData);
    objc_opt::objc_selopt_t *selopt = new(optROData) objc_opt::objc_selopt_t;
    err = selopt->write(seloptVMAddr, optRORemaining, uniq.strings());
    if (err) {
        diag.warning("%s", err);
        return;
    }
    optROData += selopt->size();
    optROData = alignPointer(optROData);
    optRORemaining -= selopt->size();
    uint32_t seloptCapacity = selopt->capacity;
    uint32_t seloptOccupied = selopt->occupied;
    selopt->byteswap(E::little_endian), selopt = nullptr;

    diag.verbose("  selector table occupancy %u/%u (%u%%)\n",
                    seloptOccupied, seloptCapacity,
                    (unsigned)(seloptOccupied/(double)seloptCapacity*100));


    // 
    // Detect classes that have missing weak-import superclasses.
    // 
    // Production shared caches don't support roots so we can set this and know
    // there will definitely not be missing weak superclasses at runtime.
    // Development shared caches can set this bit as the objc runtime only trusts
    // this bit if there are no roots at runtime.
    // 
    // This is SAFE: the binaries themselves are unmodified.
    WeakClassDetector<P> weakopt;
    bool noMissingWeakSuperclasses = weakopt.noMissingWeakSuperclasses(&cacheAccessor,
                                                                       missingWeakImports,
                                                                       sizeSortedDylibs);

    if (forProduction) {
        // Shared cache does not currently support unbound weak references.
        // Here we assert that there are none. If support is added later then
        // this assertion needs to be removed and this path needs to be tested.
        // FIXME: The internal cache also isn't going to notice that an on-disk
        // dylib could resolve a weak bind from the shared cache.  Should we just
        // error on all caches, regardless of dev/customer?
        if (!noMissingWeakSuperclasses) {
            diag.error("Some Objective-C class has a superclass that is "
                       "weak-import and missing from the cache.");
        }
    }


    //
    // Build class table.
    //
    // This is SAFE: the binaries themselves are unmodified.
    ClassListBuilder<P> classes(hinfoROOptimizer);
    ClassWalker<P, ClassListBuilder<P>> classWalker(classes);
    for (const macho_header<P>* mh : sizeSortedDylibs) {
        classWalker.walk(&cacheAccessor, mh);
    }

    diag.verbose("  recorded % 6ld classes\n", classes.classNames().size());

    uint64_t clsoptVMAddr = cacheAccessor.vmAddrForContent(optROData);
    objc_opt::objc_clsopt_t *clsopt = new(optROData) objc_opt::objc_clsopt_t;
    err = clsopt->write(clsoptVMAddr, optRORemaining, 
                        classes.classNames(), classes.classes(), false);
    if (err) {
        diag.warning("%s", err);
        return;
    }
    optROData += clsopt->size();
    optROData = alignPointer(optROData);
    optRORemaining -= clsopt->size();
    size_t duplicateCount = clsopt->duplicateCount();
    uint32_t clsoptCapacity = clsopt->capacity;
    uint32_t clsoptOccupied = clsopt->occupied;
    clsopt->byteswap(E::little_endian);
    clsopt = nullptr;

    diag.verbose("  found    % 6ld duplicate classes\n",
               duplicateCount);
    diag.verbose("  class table occupancy %u/%u (%u%%)\n",
               clsoptOccupied, clsoptCapacity, 
               (unsigned)(clsoptOccupied/(double)clsoptCapacity*100));


    //
    // Sort method lists.
    //
    // This is SAFE: modified binaries are still usable as unsorted lists.
    // This must be done AFTER uniquing selectors.
    MethodListSorter<P> methodSorter(relativeMethodListSelectorsAreDirect);
    for (const macho_header<P>* mh : sizeSortedDylibs) {
        methodSorter.optimize(&cacheAccessor, mh);
    }

    diag.verbose("  sorted   % 6ld method lists\n", methodSorter.optimized());


    // Unique protocols and build protocol table.

    // This is SAFE: no protocol references are updated yet
    // This must be done AFTER updating method lists.

    ProtocolOptimizer<P> protocolOptimizer(diag, hinfoROOptimizer);
    for (const macho_header<P>* mh : sizeSortedDylibs) {
        protocolOptimizer.addProtocols(&cacheAccessor, mh);
    }

    diag.verbose("  uniqued  % 6ld protocols\n",
               protocolOptimizer.protocolCount());

    pint_t protocolClassVMAddr = (pint_t)P::getP(optPointerList->protocolClass);

    // Get the pointer metadata from the magic protocolClassVMAddr symbol
    // We'll transfer it over to the ISA on all the objc protocols when we set their ISAs
    dyld3::MachOAnalyzerSet::PointerMetaData protocolClassPMD;
    uint16_t    protocolClassAuthDiversity  = 0;
    bool        protocolClassAuthIsAddr     = false;
    uint8_t     protocolClassAuthKey        = 0;
    if ( aslrTracker.hasAuthData((void*)&optPointerList->protocolClass, &protocolClassAuthDiversity, &protocolClassAuthIsAddr, &protocolClassAuthKey) ) {
        protocolClassPMD.diversity           = protocolClassAuthDiversity;
        protocolClassPMD.high8               = 0;
        protocolClassPMD.authenticated       = 1;
        protocolClassPMD.key                 = protocolClassAuthKey;
        protocolClassPMD.usesAddrDiversity   = protocolClassAuthIsAddr;
    }

    err = protocolOptimizer.writeProtocols(&cacheAccessor,
                                           optRWData, optRWRemaining,
                                           optROData, optRORemaining,
                                           aslrTracker, protocolClassVMAddr,
                                           protocolClassPMD);
    if (err) {
        diag.warning("%s", err);
        return;
    }

    // Align the buffer again.  The new protocols may have added an odd number of name characters
    optROData = alignPointer(optROData);

    // New protocol table which tracks loaded images.
    uint64_t protocoloptVMAddr = cacheAccessor.vmAddrForContent(optROData);
    objc_opt::objc_protocolopt2_t *protocolopt = new (optROData) objc_opt::objc_protocolopt2_t;
    err = protocolopt->write(protocoloptVMAddr, optRORemaining,
                             protocolOptimizer.protocolNames(),
                             protocolOptimizer.protocolsAndHeaders(), false);
    if (err) {
        diag.warning("%s", err);
        return;
    }
    optROData += protocolopt->size();
    optROData = alignPointer(optROData);
    optRORemaining -= protocolopt->size();
    uint32_t protocoloptCapacity = protocolopt->capacity;
    uint32_t protocoloptOccupied = protocolopt->occupied;
    protocolopt->byteswap(E::little_endian), protocolopt = NULL;

    diag.verbose("  protocol table occupancy %u/%u (%u%%)\n",
                 protocoloptOccupied, protocoloptCapacity,
                 (unsigned)(protocoloptOccupied/(double)protocoloptCapacity*100));


    // Redirect protocol references to the uniqued protocols.

    // This is SAFE: the new protocol objects are still usable as-is.
    for (const macho_header<P>* mh : sizeSortedDylibs) {
        protocolOptimizer.updateReferences(&cacheAccessor, mh);
    }

    diag.verbose("  updated  % 6ld protocol references\n", protocolOptimizer.protocolReferenceCount());


    //
    // Repair ivar offsets.
    //
    // This is SAFE: the runtime always validates ivar offsets at runtime.
    IvarOffsetOptimizer<P> ivarOffsetOptimizer;
    for (const macho_header<P>* mh : sizeSortedDylibs) {
        ivarOffsetOptimizer.optimize(&cacheAccessor, mh);
    }
    
    diag.verbose("  updated  % 6ld ivar offsets\n", ivarOffsetOptimizer.optimized());

    //
    // Build imp caches
    //
    // Objc has a magic section of imp cache base pointers.  We need these to
    // offset everything else from
    const CacheBuilder::CacheCoalescedText::StringSection& methodNames = coalescedText.getSectionData("__objc_methname");
    uint64_t selectorStringVMAddr = methodNames.bufferVMAddr;
    uint64_t selectorStringVMSize = methodNames.bufferSize;
    uint64_t impCachesVMSize = 0; // We'll calculate this later

    uint64_t optRODataRemainingBeforeImpCaches = optRORemaining;

    timeRecorder.pushTimedSection();
    
    uint8_t* inlinedSelectorsStart = optRWData;
    uint8_t* inlinedSelectorsEnd = optRWData;
    
    if (impCachesSuccess) {
        emitIMPCaches<P>(cacheAccessor, allDylibs, sizeSortedDylibs, relativeMethodListSelectorsAreDirect,
                         selectorStringVMAddr, optROData, optRORemaining, optRWData, optRWRemaining,
                         aslrTracker, inlinedSelectors, inlinedSelectorsStart, inlinedSelectorsEnd, diag, timeRecorder);
    }

    uint8_t* alignedROData = alignPointer(optROData);
    optRORemaining -= (alignedROData - optROData);
    optROData = alignedROData;

    impCachesVMSize = optRODataRemainingBeforeImpCaches - optRORemaining;
    timeRecorder.recordTime("emit IMP caches");
    timeRecorder.popTimedSection();

    diag.verbose("[IMP Caches] Imp caches size: %'lld bytes\n\n", impCachesVMSize);

    // Update the pointers in the pointer list section
    if (optImpCachesPointerSection) {
        if (optImpCachesPointerSection->size() < sizeof(objc_opt::objc_opt_pointerlist_tt<pint_t>)) {
            diag.warning("libobjc's pointer list section is too small (metadata not optimized)");
            return;
        }
        auto *impCachePointers = (objc_opt_imp_caches_pointerlist_tt<pint_t> *)cacheAccessor.contentForVMAddr(optImpCachesPointerSection->addr());
        impCachePointers->selectorStringVMAddrStart = (pint_t)selectorStringVMAddr;
        impCachePointers->selectorStringVMAddrEnd   = (pint_t)(selectorStringVMAddr + selectorStringVMSize);
        impCachePointers->inlinedSelectorsVMAddrStart = (pint_t)cacheAccessor.vmAddrForContent(inlinedSelectorsStart);
        impCachePointers->inlinedSelectorsVMAddrEnd = (pint_t)cacheAccessor.vmAddrForContent(inlinedSelectorsEnd);

        aslrTracker.add(&impCachePointers->selectorStringVMAddrStart);
        aslrTracker.add(&impCachePointers->selectorStringVMAddrEnd);
        aslrTracker.add(&impCachePointers->inlinedSelectorsVMAddrStart);
        aslrTracker.add(&impCachePointers->inlinedSelectorsVMAddrEnd);        
    }

    // Collect flags.
    uint32_t headerFlags = 0;
    if (forProduction) {
        headerFlags |= objc_opt::IsProduction;
    }
    if (noMissingWeakSuperclasses) {
        headerFlags |= objc_opt::NoMissingWeakSuperclasses;
    }


    // Success. Mark dylibs as optimized.
    for (const macho_header<P>* mh : sizeSortedDylibs) {
        const macho_section<P>* imageInfoSection = mh->getSection("__DATA", "__objc_imageinfo");
        if (!imageInfoSection) {
            imageInfoSection = mh->getSection("__OBJC", "__image_info");
        }
        if (imageInfoSection) {
            objc_image_info<P>* info = (objc_image_info<P>*)cacheAccessor.contentForVMAddr(imageInfoSection->addr());
            info->setOptimizedByDyld();
        }
    }


    // Success. Update __objc_opt_ro section in libobjc.dylib to contain offsets to generated optimization structures
    objc_opt::objc_opt_t* libROHeader = (objc_opt::objc_opt_t *)cacheAccessor.contentForVMAddr(optROSection->addr());
    E::set32(libROHeader->flags, headerFlags);
    E::set32(libROHeader->selopt_offset, (uint32_t)(seloptVMAddr - optROSection->addr()));
    E::set32(libROHeader->clsopt_offset, (uint32_t)(clsoptVMAddr - optROSection->addr()));
    E::set32(libROHeader->unused_protocolopt_offset, 0);
    E::set32(libROHeader->headeropt_ro_offset, (uint32_t)(hinfoROVMAddr - optROSection->addr()));
    E::set32(libROHeader->headeropt_rw_offset, (uint32_t)(hinfoRWVMAddr - optROSection->addr()));
    E::set32(libROHeader->protocolopt_offset, (uint32_t)(protocoloptVMAddr - optROSection->addr()));

    // Log statistics.
    size_t roSize = objcReadOnlyBufferSizeAllocated - optRORemaining;
    size_t rwSize = objcReadWriteBufferSizeAllocated - optRWRemaining;
    diag.verbose("  %lu/%llu bytes (%d%%) used in shared cache read-only optimization region\n",
                  roSize, objcReadOnlyBufferSizeAllocated, percent(roSize, objcReadOnlyBufferSizeAllocated));
    diag.verbose("  %lu/%llu bytes (%d%%) used in shared cache read/write optimization region\n",
                  rwSize, objcReadWriteBufferSizeAllocated, percent(rwSize, objcReadWriteBufferSizeAllocated));
    diag.verbose("  wrote objc metadata optimization version %d\n", objc_opt::VERSION);

    // Add segments to libobjc.dylib that cover cache builder allocated r/o and r/w regions
    addObjcSegments<P>(diag, cache, libobjcMH, objcReadOnlyBuffer, objcReadOnlyBufferSizeAllocated, objcReadWriteBuffer, objcReadWriteBufferSizeAllocated, objcRwFileOffset);


    // Now that objc has uniqued the selector references, we can apply the LOHs so that ADRP/LDR -> ADRP/ADD
    {
        const bool logSelectors = false;
        uint64_t lohADRPCount = 0;
        uint64_t lohLDRCount = 0;

        for (auto& targetAndInstructions : lohTracker) {
            uint64_t targetVMAddr = targetAndInstructions.first;
            if (!selOptimizer.isSelectorRefAddress((pint_t)targetVMAddr))
                continue;

            std::set<void*>& instructions = targetAndInstructions.second;
            // We do 2 passes over the instructions.  The first to validate them and the second
            // to actually update them.
            for (unsigned pass = 0; pass != 2; ++pass) {
                uint32_t adrpCount = 0;
                uint32_t ldrCount = 0;
                for (void* instructionAddress : instructions) {
                    uint32_t& instruction = *(uint32_t*)instructionAddress;
                    uint64_t instructionVMAddr = cacheAccessor.vmAddrForContent(&instruction);
                    uint64_t selRefContent = *(uint64_t*)cacheAccessor.contentForVMAddr(targetVMAddr);
                    const char* selectorString = (const char*)cacheAccessor.contentForVMAddr(selRefContent);
                    uint64_t selectorStringVMAddr = cacheAccessor.vmAddrForContent(selectorString);

                    if ( (instruction & 0x9F000000) == 0x90000000 ) {
                        // ADRP
                        int64_t pageDistance = ((selectorStringVMAddr & ~0xFFF) - (instructionVMAddr & ~0xFFF));
                        int64_t newPage21 = pageDistance >> 12;

                        if (pass == 0) {
                            if ( (newPage21 > 2097151) || (newPage21 < -2097151) ) {
                                if (logSelectors)
                                    fprintf(stderr, "Out of bounds ADRP selector reference target\n");
                                instructions.clear();
                                break;
                            }
                            ++adrpCount;
                        }

                        if (pass == 1) {
                            instruction = (instruction & 0x9F00001F) | ((newPage21 << 29) & 0x60000000) | ((newPage21 << 3) & 0x00FFFFE0);
                            ++lohADRPCount;
                        }
                        continue;
                    }

                    if ( (instruction & 0x3B000000) == 0x39000000 ) {
                        // LDR/STR.  STR shouldn't be possible as this is a selref!
                        if (pass == 0) {
                            if ( (instruction & 0xC0C00000) != 0xC0400000 ) {
                                // Not a load, or dest reg isn't xN, or uses sign extension
                                if (logSelectors)
                                    fprintf(stderr, "Bad LDR for selector reference optimisation\n");
                                instructions.clear();
                                break;
                            }
                            if ( (instruction & 0x04000000) != 0 ) {
                                // Loading a float
                                if (logSelectors)
                                    fprintf(stderr, "Bad LDR for selector reference optimisation\n");
                                instructions.clear();
                                break;
                            }
                            ++ldrCount;
                        }

                        if (pass == 1) {
                            uint32_t ldrDestReg = (instruction & 0x1F);
                            uint32_t ldrBaseReg = ((instruction >> 5) & 0x1F);

                            // Convert the LDR to an ADD
                            instruction = 0x91000000;
                            instruction |= ldrDestReg;
                            instruction |= ldrBaseReg << 5;
                            instruction |= (selectorStringVMAddr & 0xFFF) << 10;

                            ++lohLDRCount;
                        }
                        continue;
                    }

                    if ( (instruction & 0xFFC00000) == 0x91000000 ) {
                        // ADD imm12
                        // We don't support ADDs.
                        if (logSelectors)
                            fprintf(stderr, "Bad ADD for selector reference optimisation\n");
                        instructions.clear();
                        break;
                    }

                    if (logSelectors)
                        fprintf(stderr, "Unknown instruction for selref optimisation\n");
                    instructions.clear();
                    break;
                }
                if (pass == 0) {
                    // If we didn't see at least one ADRP/LDR in pass one then don't optimize this location
                    if ((adrpCount == 0) || (ldrCount == 0)) {
                        instructions.clear();
                        break;
                    }
                }
            }
        }

        diag.verbose("  Optimized %lld ADRP LOHs\n", lohADRPCount);
        diag.verbose("  Optimized %lld LDR LOHs\n", lohLDRCount);
    }
}


} // anon namespace

size_t IMPCaches::sizeForImpCacheWithCount(int count) {
    // The architecture should not be relevant here as it's all offsets and fixed int sizes.
    // It was just the most logical place to host this function in.

    size_t size64 = IMPCachesEmitter<Pointer64<LittleEndian>>::sizeForImpCacheWithCount(count);
    size_t size32 = IMPCachesEmitter<Pointer32<LittleEndian>>::sizeForImpCacheWithCount(count);
    assert(size64 == size32);

    return size64;
}

void SharedCacheBuilder::optimizeObjC(bool impCachesSuccess, const std::vector<const IMPCaches::Selector*> & inlinedSelectors)
{
    if ( _archLayout->is64 )
        doOptimizeObjC<Pointer64<LittleEndian>>((DyldSharedCache*)_readExecuteRegion.buffer,
            _options.optimizeStubs,
            _aslrTracker, _lohTracker,
            _coalescedText,
            _missingWeakImports, _diagnostics,
            _objcReadOnlyBuffer,
            _objcReadOnlyBufferSizeUsed,
            _objcReadOnlyBufferSizeAllocated,
            _objcReadWriteBuffer, _objcReadWriteBufferSizeAllocated,
            _objcReadWriteFileOffset, _sortedDylibs, inlinedSelectors, impCachesSuccess, _timeRecorder);
    else
        doOptimizeObjC<Pointer32<LittleEndian>>((DyldSharedCache*)_readExecuteRegion.buffer,
            _options.optimizeStubs,
            _aslrTracker, _lohTracker,
            _coalescedText,
            _missingWeakImports, _diagnostics,
            _objcReadOnlyBuffer,
            _objcReadOnlyBufferSizeUsed,
            _objcReadOnlyBufferSizeAllocated,
            _objcReadWriteBuffer, _objcReadWriteBufferSizeAllocated,
            _objcReadWriteFileOffset, _sortedDylibs, inlinedSelectors, impCachesSuccess, _timeRecorder);
}

static uint32_t hashTableSize(uint32_t maxElements, uint32_t perElementData)
{
    uint32_t elementsWithPadding = maxElements*11/10; // if close to power of 2, perfect hash may fail, so don't get within 10% of that
    uint32_t powTwoCapacity = 1 << (32 - __builtin_clz(elementsWithPadding - 1));
    uint32_t headerSize = 4*(8+256);
    return headerSize + powTwoCapacity/2 + powTwoCapacity + powTwoCapacity*perElementData;
}

// The goal here is to allocate space in the dyld shared cache (while it is being laid out) that will contain
// the objc structures that previously were in the __objc_opt_ro section.
uint32_t SharedCacheBuilder::computeReadOnlyObjC(uint32_t selRefCount, uint32_t classDefCount, uint32_t protocolDefCount)
{
    return 0xA000 + hashTableSize(selRefCount, 5) + hashTableSize(classDefCount, 12) + hashTableSize(protocolDefCount, 8);
}

// Space to replace the __objc_opt_rw section.
uint32_t SharedCacheBuilder::computeReadWriteObjC(uint32_t imageCount, uint32_t protocolDefCount)
{
    uint8_t pointerSize = _archLayout->is64 ? 8 : 4;
    return 8*imageCount
         + protocolDefCount*12*pointerSize
         + (int)_impCachesBuilder->inlinedSelectors.size() * pointerSize;
}
