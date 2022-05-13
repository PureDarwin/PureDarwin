#include <tapi/tapi.h>

// Note: The functions below are bad and don't function. Don't use them.

namespace tapi
{

bool APIVersion::isAtLeast(unsigned major, unsigned minor) noexcept { return false; }

unsigned APIVersion::getMajor() noexcept { return 0; }
unsigned APIVersion::getMinor() noexcept { return 0; }

std::string Version::getFullVersionAsString() noexcept
{
    return "Apple TAPI version 0.0.0 (0)";
}

std::string Version::getAsString() noexcept
{
    return "0.0.0";
}

LinkerInterfaceFile LinkerInterfaceFile::d;

LinkerInterfaceFile *LinkerInterfaceFile::create(const std::string &path, cpu_type_t cpuType, cpu_subtype_t cpuSubType, ParsingFlags flags, PackedVersion32 minOSVersion, std::string &errorMessage) noexcept { return &d; }

LinkerInterfaceFile *LinkerInterfaceFile::getInlinedFramework(const std::string &installName, cpu_type_t cpuType, cpu_subtype_t cpuSubType, ParsingFlags flags, PackedVersion32 minOSVersion, std::string &errorMessage) const noexcept { return &d; }

bool LinkerInterfaceFile::isInstallNameVersionSpecific() const noexcept { return false; }

bool LinkerInterfaceFile::hasReexportedLibraries() const noexcept { return false; }

bool LinkerInterfaceFile::hasWeakDefinedExports() const noexcept { return false; }

PackedVersion32 LinkerInterfaceFile::getCurrentVersion() const noexcept { return PackedVersion32(0); }

unsigned LinkerInterfaceFile::getSwiftVersion() const noexcept { return false; }

const std::string &LinkerInterfaceFile::getParentFrameworkName() const noexcept { return ""; }

const std::string &LinkerInterfaceFile::getInstallName() const noexcept { return ""; }

bool LinkerInterfaceFile::isApplicationExtensionSafe() const noexcept { return false; }

bool LinkerInterfaceFile::shouldPreferTextBasedStubFile(const std::string &path) noexcept { return false; }

bool LinkerInterfaceFile::areEquivalent(const std::string &tbdPath, const std::string &dylibPath) noexcept { return false; }

bool LinkerInterfaceFile::isSupported(const std::string &path, const uint8_t *data, size_t size) noexcept { return false; }

bool LinkerInterfaceFile::hasAllowableClients() const noexcept { return false; }

const std::vector<std::string> &LinkerInterfaceFile::allowableClients() const noexcept { return this->a; }

const std::vector<uint32_t> &LinkerInterfaceFile::getPlatformSet() const noexcept { return this->b; }

Platform LinkerInterfaceFile::getPlatform() const noexcept { return Platform::OSX; }

const std::vector<std::string> &LinkerInterfaceFile::reexportedLibraries() const noexcept { return this->a; }

const std::vector<std::string> &LinkerInterfaceFile::ignoreExports() const noexcept { return this->a; }

bool LinkerInterfaceFile::hasTwoLevelNamespace() const noexcept { return false; }

const std::vector<Symbol> &LinkerInterfaceFile::undefineds() const noexcept { return this->c; }

PackedVersion32 LinkerInterfaceFile::getCompatibilityVersion() const noexcept { return PackedVersion32(0); }

const std::vector<Symbol> &LinkerInterfaceFile::exports() const noexcept { return this->c; }

const std::vector<std::string> &LinkerInterfaceFile::inlinedFrameworkNames() const noexcept { return this->a; }

}
