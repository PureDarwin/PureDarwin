//
//  IMPCachesBuilder.hpp
//  dyld
//
//  Created by Thomas Deniau on 20/01/2020.
//

#pragma once

#include "CacheBuilder.h"
#include "IMPCaches.hpp"
#include <random>

namespace IMPCaches {

class IMPCachesBuilder {
public:
    SelectorMap selectors;

    /// Parses source dylibs to figure out which methods end up in which caches
    bool parseDylibs(Diagnostics& diag);

    /// Builds a map of the class hierarchy across all dylibs. This is especially used to resolve
    /// cross-dylib dependencies for superclasses and categories.
    void buildClassesMap(Diagnostics& diag);

    /// The entry point of the algorithm
    void buildPerfectHashes(HoleMap& holeMap, Diagnostics& diag);

    /// Regenerate the hole map if we needed to evict dylibs.
    void computeLowBits(HoleMap& holeMap);
    
    // Total size the hash tables will need.
    size_t totalIMPCachesSize() const;

    void clear() {
        // FIXME: Implement this
    }

    /** Classes for which we want to generate IMP caches according to the input JSON config laid down by OrderFiles
     * The value is the index of the class in the json file (they are ordered by decreasing order of importance)
     * This isn't just a vector because we also need to test for membership quickly */
    std::unordered_map<std::string_view, int> neededClasses;
    std::unordered_map<std::string_view, int> neededMetaclasses;

    // Classes for which we don't generate IMP caches, but which we need to track
    // to attach categories to them and find the right implementation for
    // inlined selectors
    std::unordered_set<std::string_view> trackedClasses;
    std::unordered_set<std::string_view> trackedMetaclasses;

    // List of classes with the same name that appear in different images.
    // We should not try to play with fire and try to support duplicated
    // classes in IMP caches.

    using ClassSet = std::unordered_set<ClassKey, ClassKeyHasher>;
    ClassSet duplicateClasses;

    /// Selectors which we want to inline into child classes' caches.
    std::unordered_set<std::string_view> selectorsToInline;
    std::vector<const Selector*> inlinedSelectors;

    // Class hierarchies to flatten:
    // In every class, include every selector including
    // the ones from superclasses up to the flattening root.
    // This lets us enable constant caches for some of the classes which are not leaves.
    // We avoid the pyramid of doom by making sure selectors from superclasses are
    // included in child caches, up until some flattening root, and msgSend will
    // fallback to the superclass of the flattening root if it can't find the selector
    // it expects.
    std::unordered_set<std::string_view> metaclassHierarchiesToFlatten;
    std::unordered_set<std::string_view> classHierarchiesToFlatten;

    /// All the dylibs the algorith, works on.
    std::vector<CacheBuilder::DylibInfo> & dylibs;

    IMPCachesBuilder(std::vector<CacheBuilder::DylibInfo>& allDylibs, const dyld3::json::Node& optimizerConfiguration, Diagnostics& diag, TimeRecorder& timeRecorder, const dyld3::closure::FileSystem& fileSystem);

    struct ObjCClass {

        const dyld3::MachOAnalyzer* superclassMA = nullptr;
        const uint8_t*      metaClass           = nullptr;
        const uint8_t*      superclass          = nullptr;
        uint64_t            methodListVMaddr    = 0;
        const char*         className           = nullptr;
        bool                isRootClass         = false;
        bool                isMetaClass         = false;

        const IMPCaches::ClassData::ClassLocator superclassLocator() const {
            uint64_t loadAddress = superclassMA->preferredLoadAddress();
            __block std::optional<unsigned> segmentIndex;
            __block std::optional<unsigned> segmentOffset;
            uint64_t superclassVMAddr = (superclass - (const uint8_t*)superclassMA) + loadAddress;
            superclassMA->forEachSegment(^(const dyld3::MachOAnalyzer::SegmentInfo &info, bool &stop) {
                if (info.vmAddr <= superclassVMAddr && (info.vmAddr + info.vmSize) > superclassVMAddr) {
                    segmentIndex = info.segIndex;
                    segmentOffset = (unsigned)(superclassVMAddr - info.vmAddr);
                    stop = true;
                    return;
                }
            });
            assert(segmentIndex.has_value() && segmentOffset.has_value());
            return IMPCaches::ClassData::ClassLocator {
                .installName = superclassMA->installName(),
                .segmentIndex = *segmentIndex,
                .segmentOffset = *segmentOffset,
            };
        }
    };
    struct ObjCCategory {
        const dyld3::MachOAnalyzer* classMA = nullptr;
        const uint8_t* cls = nullptr;
    };

private:

    /// Is this a class for which we want to generate an IMP cache?
    bool isClassInteresting(const ObjCClass& theClass) const;

    /// Is this a class for which we want to generate an IMP cache, or on which we want to track method attachment by categories?
    bool isClassInterestingOrTracked(const ObjCClass& theClass) const;

    /// Adds a method to a given class's cache.
    void addMethod(IMPCaches::ClassData* classDataPtr, const char* methodName, const char* installName, const char* className, const char* catName, bool inlined, bool fromFlattening);

    /// Inline a method from a parent's cache to a child's cache.
    void inlineMethodIfNeeded(IMPCaches::ClassData* classToInlineIn, const char* classToInlineFrom, const char* catToInlineFrom, const char* installNameToInlineFrom, const char* name, std::set<Selector*> & seenSelectors, bool isFlattening);

    /// Map from location (address in the mmapped source dylib) to class info built by buildClassesMap()
    std::unordered_map<const uint8_t*, ObjCClass> objcClasses;

    /// Map from location (address in the mmapped source dylib) to category info built by buildClassesMap()
    std::unordered_map<const uint8_t*, ObjCCategory> objcCategories;

    /// Address space where the selectors are going to live. Used to determine at which address we'll actually layout each selector.
    IMPCaches::AddressSpace addressSpace;

    /// Find a shift and a mask for each class. Returns the number of classes that we could not place.
    int findShiftsAndMasks();
    
    /// Shuffles selectors around to satisfy size constraints.  Returns the number of classes that we could not place.
    int solveGivenShiftsAndMasks();

    /// Determine classes we need to track category attachment on (for later inlining)
    void buildTrackedClasses(CacheBuilder::DylibInfo& dylib, const dyld3::MachOAnalyzer* ma);

    /// Parses the method lists in the source dylibs to determine what will end up in which IMP cache.
    void populateMethodLists(CacheBuilder::DylibInfo& dylib, const dyld3::MachOAnalyzer* ma, int* duplicateClassCount);

    /// Go through categories and add the methods from the category to the corresponding class's IMP cache
    void attachCategories(CacheBuilder::DylibInfo& dylib, const dyld3::MachOAnalyzer* ma);

    // Inline some selectors (driven by the OrderFiles) from parent caches into child caches.
    void inlineSelectors(CacheBuilder::DylibInfo& dylib, std::unordered_map<std::string_view, CacheBuilder::DylibInfo*> & dylibsByInstallName, const dyld3::MachOAnalyzer* ma);

    void fillAllClasses(std::vector<IMPCaches::ClassData*> & allClasses);
    void fillAllMethods(std::vector<IMPCaches::Selector*> & allMethods);
    void removeUninterestingClasses();

    struct TargetClassFindingResult {
        bool success;
        const dyld3::MachOLoaded* foundInDylib;
        const uint8_t* location;
    };

    struct BindTarget {
        std::string                 symbolName      = "";
        const dyld3::MachOAnalyzer* targetDylib     = nullptr;
        bool                        isWeakImport    = false;
    };

    struct DylibAndDeps {
        const dyld3::MachOAnalyzer*         ma = nullptr;
        __block std::vector<std::string>    dependentLibraries;
    };

    TargetClassFindingResult findTargetClass(const dyld3::MachOAnalyzer* ma, uint64_t targetClassVMAddr, uint64_t targetClassPointerVMAddr, const char* logContext, const std::unordered_map<uint64_t, uint64_t> & bindLocations, std::vector<BindTarget> & bindTargets, std::unordered_map<std::string, DylibAndDeps> &dylibMap);

    const std::string * nameAndIsMetaclassPairFromNode(const dyld3::json::Node & node, bool* metaclass);

    Diagnostics& _diagnostics;
    TimeRecorder& _timeRecorder;
    const dyld3::closure::FileSystem& _fileSystem;
};

}
