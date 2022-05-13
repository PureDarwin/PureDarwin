// This is a stub header. Currently, TAPI sources have not been released for any
//   llvm source version in the last two years. This header and one c++ stub file
//   provide a method to stub out the TAPI dependency in ld64.
// Many of these things have been pulled out of the public TAPI code.

#ifndef TAPI_H
#define TAPI_H

#include <mach-o/loader.h>
#include <string>
#include <vector>

// We have to pretend to be TAPI 1.3 or ld64 will error on a macro.
#define TAPI_API_VERSION_MAJOR  1
#define TAPI_API_VERSION_MINOR  3
#define TAPI_VERSION_PATCH      0

#define TAPI_PUBLIC __attribute__((visibility ("default")))

namespace tapi
{

enum class Platform : unsigned {
    Unknown = 0,
    OSX = 1,
    iOS = 2,
    watchOS = 3,
    tvOS = 4,
    bridgeOS = 5
};

enum ParsingFlags : unsigned {
  None = 0,
  ExactCpuSubType = 1U << 0,
  DisallowWeakImports = 1U << 1,
};

inline ParsingFlags operator|(ParsingFlags lhs, ParsingFlags rhs) noexcept
{
    return static_cast<ParsingFlags>(static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs));
}

inline ParsingFlags operator|=(ParsingFlags &lhs, ParsingFlags rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

class TAPI_PUBLIC APIVersion
{
public:
    static bool isAtLeast(unsigned major, unsigned minor = 0) noexcept;

    static unsigned getMajor() noexcept;
    static unsigned getMinor() noexcept;
};

class TAPI_PUBLIC Version
{
public:
    static std::string getFullVersionAsString() noexcept;

    static std::string getAsString() noexcept;
};

class TAPI_PUBLIC PackedVersion32
{
private:
    uint32_t _version;

public:
    PackedVersion32(uint32_t rawVersion) : _version(rawVersion) {}

    bool operator<(const PackedVersion32 &rhs) const { return _version < rhs._version; }

    bool operator<=(const PackedVersion32 &rhs) const { return _version <= rhs._version; }

    bool operator==(const PackedVersion32 &rhs) const { return _version == rhs._version; }

    bool operator!=(const PackedVersion32 &rhs) const { return _version != rhs._version; }

    operator unsigned() const { return _version; }
};

class TAPI_PUBLIC Symbol
{
private:
    std::string _name;

public:
    inline const std::string &getName() const noexcept { return _name; }

    inline bool isThreadLocalValue() const noexcept { return false; }
    inline bool isWeakDefined() const noexcept { return false; }
};

class TAPI_PUBLIC LinkerInterfaceFile
{
private:
    std::vector<std::string> a;
    std::vector<uint32_t> b;
    std::vector<Symbol> c;

    static LinkerInterfaceFile d;

public:
    static LinkerInterfaceFile *create(const std::string &path, cpu_type_t cpuType, cpu_subtype_t cpuSubType, ParsingFlags flags, PackedVersion32 minOSVersion, std::string &errorMessage) noexcept;

    static bool shouldPreferTextBasedStubFile(const std::string &path) noexcept;

    static bool areEquivalent(const std::string &tbdPath, const std::string &dylibPath) noexcept;

    static bool isSupported(const std::string &path, const uint8_t *data, size_t size) noexcept;

    bool isInstallNameVersionSpecific() const noexcept;

    bool hasReexportedLibraries() const noexcept;

    bool hasWeakDefinedExports() const noexcept;

    PackedVersion32 getCurrentVersion() const noexcept;

    PackedVersion32 getCompatibilityVersion() const noexcept;

    unsigned getSwiftVersion() const noexcept;

    const std::string &getParentFrameworkName() const noexcept;

    const std::string &getInstallName() const noexcept;

    bool isApplicationExtensionSafe() const noexcept;

    bool hasAllowableClients() const noexcept;

    const std::vector<std::string> &allowableClients() const noexcept;

    const std::vector<uint32_t> &getPlatformSet() const noexcept;

    Platform getPlatform() const noexcept;

    const std::vector<std::string> &reexportedLibraries() const noexcept;

    const std::vector<std::string> &ignoreExports() const noexcept;

    bool hasTwoLevelNamespace() const noexcept;

    const std::vector<Symbol> &undefineds() const noexcept;

    const std::vector<Symbol> &exports() const noexcept;

    LinkerInterfaceFile *getInlinedFramework(const std::string &installName, cpu_type_t cpuType, cpu_subtype_t cpuSubType, ParsingFlags flags, PackedVersion32 minOSVersion, std::string &errorMessage) const noexcept;

    const std::vector<std::string> &inlinedFrameworkNames() const noexcept;
};

}

#endif // TAPI_H
