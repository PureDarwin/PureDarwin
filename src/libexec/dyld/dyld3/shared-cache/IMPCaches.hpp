//
//  IMPCaches.hpp
//  dyld_shared_cache_builder
//
//  Created by Thomas Deniau on 18/12/2019.
//

#ifndef IMPCaches_hpp
#define IMPCaches_hpp

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <map>
#include <string>
#include <random>

#define DEBUG_SLOTS 1

template <typename P> class objc_method_t;

namespace IMPCaches {
class Selector;
class Constraint;

class ClassData {
private:
#if DEBUG_SLOTS
    std::vector<const Selector*> slots;
#else
    std::vector<bool> slots;
#endif

    // Scratchpad for constraintForMethod
    std::vector<int> allowedValues;

    void resetSlots();

public:
    bool isMetaclass;

    // True most of the time, but can also be false in 2 cases:
    // * We have entries for classes for which we don't want to generate IMP caches
    //   when they are superclasses of "interesting" (= for which we want an IMP cache)
    //   classes, so that we can properly attach non-cross-image categories to them and
    //   reference the right IMP for child classes which are actually interesting
    // * We can also have failed to generate a cache for this class, or for a class
    //   in its flattening superclass hierarchy
    bool shouldGenerateImpCache;

    // Class has duplicates or is a child of a class with duplicates.
    bool isPartOfDuplicateSet = false;

    // This is set when we drop a class because a class in its flattening superclasss
    // hierarchy was dropped. In that case, we won't try to flip its shouldGenerateImpCache
    // value back to true when restoring a snapshot. (We could keep track of all the
    // dependencies but it would be very messy and the reward is only a few classes here and there).
    bool droppedBecauseFlatteningSuperclassWasDropped = false;

    uint8_t backtracks = 0;

    // For debug purposes
    const char* name;

    struct Method {
        const char* installName = nullptr;
        const char* className = nullptr;
        const char* categoryName = nullptr;
        Selector* selector = nullptr;
        bool wasInlined = false;
        bool fromFlattening = false;
    };

    std::vector<Method> methods;

    std::string description() const;

    int neededBits;

    void didFinishAddingMethods();

    // Not const because this uses the slots as a scratchpad
    Constraint constraintForMethod(const Selector* m);

    bool operator<(const ClassData& r) const {
        if (isMetaclass != r.isMetaclass) return isMetaclass;
        return strcmp(name, r.name) < 0;
    }

    struct PlacementAttempt {
        int numberOfBitsToSet;
        int shift;
        int neededBits;

        PlacementAttempt(int _numberOfBitsToSet, int _shift, int _neededBits) : numberOfBitsToSet(_numberOfBitsToSet), shift(_shift), neededBits(_neededBits) {

        }

        std::string description() const;

        friend bool operator<(const PlacementAttempt& l, const PlacementAttempt& r)
        {
            return  std::tie(l.neededBits, l.numberOfBitsToSet, l.shift)
            < std::tie(r.neededBits, r.numberOfBitsToSet, r.shift);
        }

        int mask() const {
            return (1 << neededBits) - 1;
        }

        struct PreviousMethodAddress {
            int address;
            int fixedBitsMask;
        };

        struct PreviousState {
            int neededBits;
            int shift;
            int mask;
            std::unordered_map<Selector*, PreviousMethodAddress> methods;
        };

        struct Result {
            bool success;
            PreviousState previousState;
        };
    };

    // Possibilities we go through in the greedy backtracking algorithm findShiftsAndMask()
    // Each class has a set (attempts()) of shift-mask possibilities, ordered, and we go through them
    // sequentially.
    PlacementAttempt attemptForShift(int shift, int neededBits) const;
    std::vector<PlacementAttempt> attempts() const;

    int shift;

    inline int modulo() const {
        return 1 << neededBits;
    }

    inline int mask() const {
        return modulo() - 1;
    }

    // Attempt to place the class with the shift/mask in the attempt argument.
    typename PlacementAttempt::Result applyAttempt(const PlacementAttempt& attempt, std::minstd_rand & randomNumberGenerator);

    // Reassign the addresses as they were before we produced resultToBacktrackFrom
    void backtrack(typename PlacementAttempt::Result& resultToBacktrackFrom);

    // Did we have to grow the size of the hash table to one more bit when attempting to place it?
    bool hadToIncreaseSize() const;

    // Not const because this stomps over the slots array for checking
    bool checkConsistency();

    // Size in bytes needed for this hash table in the shared cache.
    size_t sizeInSharedCache() const;

    // Used to store the location of a flattening root's superclass, so that
    // we can compute its final vmAddr once we are in the ObjC optimizer.
    struct ClassLocator {
        const char *installName;
        unsigned segmentIndex;
        unsigned segmentOffset;
        bool operator==(const ClassLocator & other);
    };

    std::string_view flatteningRootName;
    std::set<std::string_view> flattenedSuperclasses;
    std::optional<ClassLocator> flatteningRootSuperclass;

    // For debug purposes
    bool operator==(const ClassData &other);
};

/// A unique selector. Has a name, a list of classes it's used in, and an address (which is actually an offset from
/// the beginning of the selectors section). Due to how the placement algorithm work, it also has a current
/// partial address and the corresponding bitmask of fixed bits. The algorithm adds bits to the partial address
/// as it makes progress and updates the mask accordingly
class Selector {
public:
    // For debug purposes
    const char* name;
    
    /// Classes the selector is used in
    std::vector<IMPCaches::ClassData*> classes;
    
    /// Which bits of address are already set
    int fixedBitsMask;
    
    /// Current 128-byte bucket index for this selector. Only the bits in fixedBitsMask are actually frozen.
    int inProgressBucketIndex;
    
    /// Full offset of the selector, including its low 7 bits (so, not the bucket's index ; inProgressBucketIndex (assuming all bits are setr by now) << 7 + some low bits)
    int offset; // including low bits

    /// Number of bits that you would need to freeze if you were to use this selector with this shift and mask.
    int numberOfBitsToSet(int shift, int mask) const {
        int fixedBits = (fixedBitsMask >> shift) & mask;
        return __builtin_popcount(mask) - __builtin_popcount(fixedBits);
    }

    int numberOfSetBits() const {
        return __builtin_popcount(fixedBitsMask);
    }

    unsigned int size() const {
        return (unsigned int)strlen(name) + 1;
    }

    // For debug purposes
    bool operator==(const Selector &other) const {
        return (strcmp(name, other.name) == 0)
            && (inProgressBucketIndex == other.inProgressBucketIndex)
            && (fixedBitsMask == other.fixedBitsMask);
    }

    bool operator!=(const Selector &other) const {
        return !(*this == other);
    }

};

class AddressSpace;
std::ostream& operator<<(std::ostream& o, const AddressSpace& c);

struct Hole {
    // [startAddress, endAddress[
    int startAddress;
    int endAddress;
    int size() const {
        return endAddress - startAddress;
    }

    // All our intervals are non-overlapping
    bool operator<(const Hole& other) const {
        auto a = std::make_tuple(size(), startAddress);
        auto b = std::make_tuple(other.size(), other.startAddress);
        return a < b;
    }
};

/// Represents the holes left by the selector placement algorithm, to be filled later with other selectors we did not target.
class HoleMap {
public:
    
    /// Returns the position at which we should place a string of size `size`.
    int addStringOfSize(unsigned size);
    
    /// Total size of all the holes
    unsigned long totalHoleSize() const;
    
    // Empty the hole map.
    void clear();

    HoleMap();

private:
    friend class AddressSpace;
    friend std::ostream& operator<<  (std::ostream& o, const HoleMap& m);

    int endAddress = 0;
    std::set<IMPCaches::Hole> holes;
};

// A selector that is known to be at offset 0, to let objc easily compute
// the offset of a selector given the SEL.
constexpr std::string_view magicSelector = "\xf0\x9f\xa4\xaf";

/// This is used to place the selectors in 128-byte buckets.
/// The "indices" below are the indices of the 128-byte bucket. To get an actual selector offset from this,
/// we shift the index to the left by 7 bits, and assign low bits depending on the length of each selector (this happens
/// in @see computeLowBits()).
/// The goal of this class is to validate that selectors can actually be placed in the buckets without overflowing
/// the 128-byte total length limit (based on the length of each individual selector)
class AddressSpace {
public:
    int sizeAtIndex(int idx) const;
    int sizeAvailableAfterIndex(int idx) const;
    bool canPlaceMethodAtIndex(const Selector* method, int idx) const;
    void placeMethodAtIndex(Selector* method, int idx);
    
    // If we decided to drop any classes, remove the selectors that were only present in them
    void removeUninterestingSelectors();
    
    // Once all selectors are placed in their 128-byte buckets,
    // actually assign the low 7 bits for each, and make a map of the
    // holes so that we can fill them with other selectors later.
    void computeLowBits(HoleMap& selectorsHoleMap) const;

    std::string description() const;

    static const int maximumIndex = (1 << 17) - 1;
    static constexpr int bagSizeShift = 7;

    friend std::ostream& operator<<  (std::ostream& o, const AddressSpace& c);
private:
    inline int bagSizeAtIndex(int idx) const {
        static constexpr int bagSize = 1 << bagSizeShift;
        static constexpr int bag0Size = bagSize - (magicSelector.length() + 1);
        return idx ? bagSize : bag0Size;
    }
    bool canPlaceWithoutFillingOverflowCellAtIndex(int idx) const;
    std::unordered_map<int, std::vector<Selector*>> methodsByIndex;
    std::unordered_map<int, int> sizes;
};

/// Represents a constraint on some of the bits of an address
/// It stores a set of allowed values for a given range of bits (shift and mask)
class Constraint {
public:
    int mask;
    int shift;
    std::unordered_set<int> allowedValues;

    Constraint intersecting(const Constraint& other) const;
    friend std::ostream& operator << (std::ostream& o, const Constraint& c);

    bool operator==(const Constraint& other) const {
        return mask == other.mask &&
                shift == other.shift &&
                allowedValues == other.allowedValues;
    }

    struct Hasher {
        size_t operator()(const IMPCaches::Constraint& c) const {
            return c.shift << 24 | c.mask << 16 | c.allowedValues.size() << 8 | *c.allowedValues.begin();
        }
    };
};

/// Merges several Constraints together to generate a simplified constraint equivalent to the original set of constraints
class ConstraintSet {
    std::unordered_set<Constraint, Constraint::Hasher> constraints;

public:
    std::optional<Constraint> mergedConstraint;

    bool add(const Constraint& c);
    void clear();
};

class SelectorMap {
public:
    using UnderlyingMap = std::map<std::string_view, std::unique_ptr<IMPCaches::Selector>>;
    UnderlyingMap map;
    SelectorMap();
};

// Implemented in OptimizerObjC
size_t sizeForImpCacheWithCount(int entries);

struct ClassKey {
    std::string_view name;
    bool metaclass;

    size_t hash() const {
        std::size_t seed = 0;
        seed ^= std::hash<std::string_view>()(name) + 0x9e3779b9 + (seed<<6) + (seed>>2);
        seed ^= std::hash<bool>()(metaclass) + 0x9e3779b9 + (seed<<6) + (seed>>2);
        return seed;
    }

    bool operator==(const ClassKey &other) const {
        return (name == other.name) && (metaclass == other.metaclass);
    }
};

struct ClassKeyHasher {
    size_t operator()(const ClassKey& k) const {
        return k.hash();
    }
};

}


#endif /* IMPCaches_hpp */
