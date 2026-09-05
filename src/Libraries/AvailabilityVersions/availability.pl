#!/usr/bin/env python3

# This file ends in ".pl" because it used to be a perl script, and updating all of
# the client projects and OS mastering rules is not worth the effort.

import os
import re
import sys
import zlib
import shutil
import filecmp
import argparse
import tempfile

from ast import literal_eval

MIN_PYTHON = (3, 7) #Required for ordered dictionaries as default
if sys.version_info < MIN_PYTHON:
    sys.exit("Python %s.%s or later is required.\n" % MIN_PYTHON)

avVersion = "Local"

# The build script will embed the DSL content here, otherwise we build it at runtime
dslContent = None
# @@INSERT_DSL_CONTENT()@@

class VersionSetDSL:
    def __init__(self, data):   self.parsedDSL = self.Parser(data)
    def sets(self):             return self.parsedDSL.version_sets
    def platforms(self):        return self.parsedDSL.platforms

    class Version:
        vers = ""
        def __init__(self, vers):
            self.vers = vers
            match = re.match(r"^(\d+)(?:.(\d+))?(?:.(\d+))?$", self.vers)
            assert match  is not None
            if (match.group(1)):
                self.major = int(match.group(1))
            else:
                print(f"Invalid version number (missing major)")
                exit(-1)
            if (match.group(2)):
                self.minor = int(match.group(2))
            else:
                print(f"Invalid version number (missing minor)")
                exit(-1)
            if (match.group(3)):
                self.revision = int(match.group(3))
            else:
                self.revision = 0
        def hex(self):
            return f"0x00{self.major :02x}{self.minor :02x}{self.revision :02x}"
        def decimal(self, macosFormat):
            if macosFormat and self.major == 10 and self.minor <= 9: result = f"{self.major}{self.minor :01}{self.revision :01}"
            else: result = f"{self.major}{self.minor :02}{self.revision :02}"
            return result.rjust(6)
        def symbol(self):
            if self.revision != 0: return f"{self.major}_{self.minor}_{self.revision}"
            return f"{self.major}_{self.minor}"
        def __eq__(self, other):
            return (self.major == other.major) and (self.minor == other.minor) and (self.revision == other.revision)
        def __lt__(self, other):
            if self.major > other.major: return False
            if self.minor > other.minor: return False
            if self.revision > other.revision: return False
            return True
        def string(self): return self.vers
        def __str__(self): return self.vers
    class Variant:
        def __init__(self, name, macroName, **optionals):
            self.name                                   = name
            self.availability_aliases                   = []
            self.availability_deprecation_define_name   = macroName
            if "availability_aliases" in optionals:
                self.availability_aliases = optionals["availability_aliases"].split(',')
    class Platform:
        def __init__(self, name, stylizedName, macroName, **optionals):
            # Default values
            self.name                                   = name
            self.stylized_name                          = stylizedName
            self.availability_deprecation_define_name   = macroName
            self.min_max_define_name                    = macroName
            self.availability_define_prefix             = f"__{macroName}_"
            self.platform_define                        = f"PLATFORM_{macroName}"
            self.dyld_version_define_name               = f"DYLD_{macroName}_VERSION_"
            self.versions                               = []
            self.variants                               = []
            self.cmd_aliases                            = []
            self.availability_aliases                   = []
            self.ios_compatility_version                = None
            self.versioned                              = not optionals.get("unversioned", False)
            self.short_version_numbers                  = optionals.get("short_version_numbers", False)
            self.bleached                               = optionals.get("bleached", False)
            self.target_os_name                         = optionals.get("target_os_name", name)
            self.supports_legacy_environment_defines    = optionals.get("supports_legacy_environment_defines", False)
            self.midVersionAlias                        = False
            if "platform_define_name" in optionals:
                self.platform_define = f"PLATFORM_{optionals['platform_define_name']}"
            if "cmd_aliases" in optionals:
                self.cmd_aliases = optionals["cmd_aliases"].split(',')
            if "availability_aliases" in optionals:
                self.availability_aliases = optionals["availability_aliases"].split(',')
            if "dyld_version_define_name" in optionals:
                self.dyld_version_define_name = f'DYLD_{optionals["dyld_version_define_name"]}_VERSION_'
            if "min_max_define_name" in optionals:
                self.min_max_define_name = optionals["min_max_define_name"]
            if "ios_implicit_min" in optionals:
                self.ios_compatility_version = VersionSetDSL.Version(optionals["ios_implicit_min"])
            if "availability_deprecation_define_name" in optionals:
                self.availability_deprecation_define_name = optionals["availability_deprecation_define_name"]
            if "version_define_name" in optionals:
                self.availability_define_prefix = f"__{optionals['version_define_name']}_"
        def add_version(self, version):
            if (len(self.versions) > 0) and (self.versions[-1] > version):
                print(f"Out of order version {version} for platform {self.name}")
                exit(-1)
            self.versions.append(version)
        def add_variant(self, variant): return self.variants.append(variant);
    class Parser:
        platforms       = {}
        version_sets    = []
        def __init__(self, data):
            for line in data.splitlines():
                line = line.strip().split('#',1)[0]
                if not line:
                    continue
                parts = line.split()
                args = []
                kwargs = {}
                for dsl_arg in parts:
                    if '=' in dsl_arg:
                        k, v = dsl_arg.split('=', 1)
                        kwargs[k] = v
                    elif dsl_arg[0] == "-":
                        kwargs[dsl_arg[1:]] = True
                    else:
                        args.append(dsl_arg)
                getattr(self, parts[0])(*args[1:], **kwargs)
        def set(self, name, version, uversion):
            platforms = {}
            for (platformName, platform) in self.platforms.items():
                if platform.versioned:
                    if platform.midVersionAlias:
                        platforms[platformName] = platform.versions[-2]
                    else:
                        platforms[platformName] = platform.versions[-1]
            version_set                 = {}
            version_set["name"]         = name
            version_set["version"]      = VersionSetDSL.Version(version)
            version_set["platforms"]    = platforms
            self.version_sets.append(version_set)
            # TODO add error checking for version decrease
        def version(self, platform, version, **optionals):
            if platform in self.platforms:
                self.platforms[platform].midVersionAlias = False
                version = VersionSetDSL.Version(version)
                if "alias_version" in optionals:
                    self.platforms[platform].midVersionAlias = True
                    aliasVersion = VersionSetDSL.Version(optionals["alias_version"])
                    if aliasVersion > version:
                        self.platforms[platform].add_version(version)
                        self.platforms[platform].add_version(aliasVersion)
                    else:
                        self.platforms[platform].add_version(aliasVersion)
                        self.platforms[platform].add_version(version)
                else:
                    self.platforms[platform].add_version(version)
            else:
                print(f"Unknown platform \"{platform}\"")
                exit(-1)
            # TODO add error checking for version decrease
        def platform(self, name, stylizedName, macroName, **optionals):
            self.platforms[name] = VersionSetDSL.Platform(name, stylizedName, macroName, **optionals)
        def variant(self, platform, name, macroName, **optionals):
            # print(f"PLATFORM {platform}\nNAME {name}\n)
            if platform in self.platforms:
                self.platforms[platform].add_variant(VersionSetDSL.Variant(name, macroName, **optionals))
            else:
                print(f"Unknown platform \"{platform}\"")
                exit(-1)

if not dslContent:
    uversion = os.getenv('RC_DARWIN_KERNEL_VERSION')
    dslContent = ""
    with open(f"{os.path.abspath(os.path.dirname(sys.argv[0]))}/availability.dsl", 'r') as f:
        for line in f.readlines():
            dslContent += line
            # We do this here instead of in the DSL parser because filtering the output in the DSL parser would 
            # require storing the parsed DSL structures or being able to regenerate the DSL. Both are doable, but
            # not worth the effort at this time
            parts = line.split()
            if uversion and parts and parts[0] == "set" and parts[3] == uversion:
                break
versions = VersionSetDSL(dslContent)

def print_sets():
    print("---")
    for set in versions.sets():
        print(f'{set["name"]}:')
        for os, osVersion in sorted(set["platforms"].items()):
            print(f' {os}: "{osVersion.string()}"')

def print_versions(platform):
    print(" ".join([version.string() for version in versions.platforms()[platform].versions]))

class Preprocessor:
    def __init__(self, inputFile, outputFile):
        bufferedOutput = ""
        with tempfile.NamedTemporaryFile('w') as tmp:
            with open(inputFile, 'r') as input:
                for line in input.readlines():
                    match = re.match(r"^(?:#|//)\s+@@(.*)\((.*)\)@@", line)
                    if match:
                        args = []
                        optionals = {}
                        if match.group(2):
                            for arg in match.group(2).split(','):
                                if '=' in arg:
                                    k, v = arg.split('=', 1)
                                    optionals[k] = v
                                else:
                                    args.append(arg)
                            getattr(self, match.group(1))(tmp, *args, **optionals)
                        else: getattr(self, match.group(1))(tmp)
                    else: tmp.write(line)
            tmp.flush()
            shutil.copymode(inputFile, tmp.name)
            if (not os.path.exists(outputFile)) or (not filecmp.cmp(tmp.name, outputFile)):
                shutil.copy(tmp.name, outputFile)
    def INSERT_DSL_CONTENT(self, output):
        output.write("dslContent = \"\"\"\n")
        output.write(dslContent)
        output.write("\"\"\"\n")
    def VERSION_MAP(self, output):
        sets = []
        for set in versions.sets():
            set_string = ", ".join(sorted({".{} = {}".format(os,osVersion.hex()) for (os,osVersion) in set["platforms"].items()}))
            sets.append("\t{{ .set = {}, {} }}".format(set["version"].hex(), set_string))
        platform_string = "\n".join(["    uint32_t {} = 0;".format(name) for name in versions.platforms().keys()])  
        output.write("""
#include <set>
#include <array>
namespace dyld3 {{
struct __attribute__((visibility("hidden"))) VersionSetEntry {{
    uint32_t set = 0;
{}
    bool operator<(const uint32_t& other) const {{
        return set < other;
    }}
}};

static const std::array<VersionSetEntry, {}> sVersionMap = {{{{
{}
}}}};
}};
""".format(platform_string, len(sets), ",\n".join(sets)))
    def DYLD_HEADER_VERSIONS(self, output):
        for (name,platform) in versions.platforms().items():
            for version in platform.versions:
                output.write(f"#define {platform.dyld_version_define_name + version.symbol() : <48}{version.hex()}\n");
            output.write("\n")
        for set in versions.sets():
            set_string = " / ".join(sorted({"{} {}".format(os,osVersion.string()) for(os,osVersion) in set["platforms"].items()}))
            output.write("// dyld_{}_os_versions => {}\n".format(set["name"], set_string))
            output.write("#define dyld_{}_os_versions".format(set["name"]).ljust(56, ' '))
            output.write("({{ (dyld_build_version_t){{0xffffffff, {}}}; }})\n\n".format(set["version"].hex()))
        for (name,platform) in versions.platforms().items():
            for version in platform.versions:
                output.write("#define dyld_platform_version_{}_{}".format(platform.stylized_name, version.symbol()).ljust(56, ' '))
                output.write("({{ (dyld_build_version_t){{{}, {}{}}}; }})\n".format(platform.platform_define, platform.dyld_version_define_name, version.symbol()))
            output.write("\n")
            # self.AVAILABILITY_VERSION_DEFINES_PLATFORM(output, platform)
    def ALIAS_VERSION_MACROS(self, output, platformString, newName, oldName, **optionals):
        minVersion =  literal_eval(optionals.get("minVersion", "0x00000000"))
        maxVersion =  literal_eval(optionals.get("maxVersion", "0xFFFFFFFF"))
        platform = versions.platforms()[platformString];
        for version in platform.versions:
            if literal_eval(version.hex()) < minVersion: continue
            if literal_eval(version.hex()) >= maxVersion: continue
            output.write(f'#define {newName + version.symbol() : <48} {oldName + version.symbol()}\n')
    def AVAILABILITY_DEFINES(self, output):
        for platformString in versions.platforms():
            platform = versions.platforms()[platformString];
            if platform.bleached:
                output.write(f"#ifndef __APPLE_BLEACH_SDK__\n")
            output.write(f"#ifndef __API_TO_BE_DEPRECATED_{platform.availability_deprecation_define_name}\n")
            output.write(f"#define __API_TO_BE_DEPRECATED_{platform.availability_deprecation_define_name} 100000\n")
            output.write(f"#endif\n");
            for variant in platform.variants:
                output.write(f"#ifndef __API_TO_BE_DEPRECATED_{variant.availability_deprecation_define_name}\n")
                output.write(f"#define __API_TO_BE_DEPRECATED_{variant.availability_deprecation_define_name} 100000\n")
                output.write(f"#endif\n");
            if platform.bleached:
                output.write(f"#endif /* __APPLE_BLEACH_SDK__ */\n")
            output.write(f"\n");
    def AVAILABILITY_VERSION_DEFINES(self, output):
        for platformString in versions.platforms():
            short = platform = versions.platforms()[platformString].short_version_numbers
            platform = versions.platforms()[platformString];
            for version in platform.versions:
                output.write(f"#define {platform.availability_define_prefix + version.symbol() : <48}{version.decimal(short)}\n")
            output.write(f"/* {platform.availability_define_prefix}_NA is not defined to a value but is used as a token by macros to indicate that the API is unavailable */\n\n")
    def AVAILABILITY_MIN_MAX_DEFINES(self, output):
        for platformString in versions.platforms():
            platform = versions.platforms()[platformString];
            if not platform.versioned:
                continue
            if platform.bleached:
                output.write(f"#ifndef __APPLE_BLEACH_SDK__\n")
            output.write(f"#ifndef __{platform.min_max_define_name}_VERSION_MIN_REQUIRED\n")
            output.write(f"    #if defined(__has_builtin)\n")
            output.write(f"        #if __has_builtin(__is_target_os)\n")
            output.write(f"            #if __is_target_os({platform.target_os_name})\n")
            output.write(f"                #define __{platform.min_max_define_name}_VERSION_MIN_REQUIRED __ENVIRONMENT_OS_VERSION_MIN_REQUIRED__\n")
            output.write(f"                #define __{platform.min_max_define_name}_VERSION_MAX_ALLOWED {platform.availability_define_prefix + platform.versions[-1].symbol()}\n")
            if platform.ios_compatility_version:
                output.write(f"                /* for compatibility with existing code.  New code should use platform specific checks */\n")
                output.write(f"                #define __IPHONE_OS_VERSION_MIN_REQUIRED __IPHONE_{platform.ios_compatility_version.symbol()}\n")
            output.write(f"            #endif\n")
            if platform.supports_legacy_environment_defines:
                output.write(f"        #elif  __ENVIRONMENT_{platform.min_max_define_name}_VERSION_MIN_REQUIRED__ \n")
                output.write(f"            #define __{platform.min_max_define_name}_VERSION_MIN_REQUIRED __ENVIRONMENT_{platform.min_max_define_name}_VERSION_MIN_REQUIRED__\n")
                output.write(f"            #define __{platform.min_max_define_name}_VERSION_MAX_ALLOWED {platform.availability_define_prefix + platform.versions[-1].symbol()}\n")
                if platform.ios_compatility_version:
                    output.write(f"            /* for compatibility with existing code.  New code should use platform specific checks */\n")
                    output.write(f"            #define __IPHONE_OS_VERSION_MIN_REQUIRED __IPHONE_{platform.ios_compatility_version.symbol()}\n")
            output.write(f"        #endif /* __has_builtin(__is_target_os) */\n")
            output.write(f"    #endif /* defined(__has_builtin) */\n")
            output.write(f"#endif /* __{platform.min_max_define_name}_VERSION_MIN_REQUIRED */\n")
            if platform.bleached:
                output.write(f"#endif /* __APPLE_BLEACH_SDK__ */\n")
            output.write("\n")
    def AVAILABILITY_PLATFORM_DEFINES(self, output):
        def writeDefines(displayName, realName, hasVersions):
            if hasVersions:
                output.write(f"   #define __API_AVAILABLE_PLATFORM_{displayName}(x) {realName},introduced=x\n")
                output.write(f"   #define __API_DEPRECATED_PLATFORM_{displayName}(x,y) {realName},introduced=x,deprecated=y\n")
                output.write(f"   #define __API_OBSOLETED_PLATFORM_{displayName}(x,y,z) {realName},introduced=x,deprecated=y,obsoleted=z\n")
            output.write(f"   #define __API_UNAVAILABLE_PLATFORM_{displayName} {realName},unavailable\n")
        output.write(f"#if defined(__has_feature) && defined(__has_attribute)\n")
        output.write(f" #if __has_attribute(availability)\n")
        for platformString in versions.platforms():
            platform = versions.platforms()[platformString];
            if platform.bleached:
                output.write(f"#ifndef __APPLE_BLEACH_SDK__\n")
            writeDefines(platformString, platformString, platform.versioned)
            for alias in platform.availability_aliases:
                writeDefines(alias, platformString, platform.versioned)
            for variant in platform.variants:
                writeDefines(variant.name, variant.name, platform.versioned)
                for alias in variant.availability_aliases:
                    writeDefines(alias, variant.name, platform.versioned)
            if platform.bleached:
                output.write(f"#endif /* __APPLE_BLEACH_SDK__ */\n")
        output.write(f" #endif /* __has_attribute(availability) */\n")
        output.write(f"#endif /* defined(__has_feature) && defined(__has_attribute) */\n")
    def AVAILABILITY_MACRO_IMPL(self, output, prefix, dispatcher, **optionals):
        av_version_hash = zlib.adler32(avVersion.encode()) # This does not need to cryptographically secure as it is meant to detect accidental failures
        count = len(versions.platforms())
        for platformString in versions.platforms():
            platform = versions.platforms()[platformString]
            count = count + len(platform.variants)
        platformList    = []
        argList         = []
        argPrefix       = ""
        if "args" in optionals:
            argList     = optionals["args"].split(",")
            argPrefix   = f"{optionals['args']},"
        for i in range(0, count):
            platformList.append(f"arg{i}")
            output.write(f"    #define {prefix}{i}({argPrefix}{','.join(platformList)}) {' '.join([f'{dispatcher}({argPrefix}{x})' for x in platformList])}\n")   
        output.write(f"    #define {prefix}_GET_MACRO_{av_version_hash}({','.join([f'_{x}' for x in range(0, count+len(argList))])},NAME,...) NAME\n");
    def AVAILABILITY_MACRO_INTERFACE(self, output, name, macroName, **optionals):
        av_version_hash = zlib.adler32(avVersion.encode()) # This does not need to cryptographically secure as it is meant to detect accidental failures
        scoped_availablity = False
        if "scoped_availablity" in optionals and optionals["scoped_availablity"] == "TRUE":
            scoped_availablity=True
        count = len(versions.platforms())
        for platformString in versions.platforms():
            platform = versions.platforms()[platformString]
            count = count + len(platform.variants)
        argList = ','.join([f'{macroName}{x}' for x in reversed(range(0, count))])
        if "argCount" in optionals:
            argList = argList + (",0" * int(optionals["argCount"]))
        if scoped_availablity:
            output.write(f'    #define {name}_BEGIN(...) _Pragma("clang attribute push") {macroName}_GET_MACRO_{av_version_hash}(__VA_ARGS__,{argList},0)(__VA_ARGS__)\n')
            output.write(f'    #define {name}_END _Pragma("clang attribute pop")\n')
        else:
            output.write(f"    #define {name}(...) {macroName}_GET_MACRO_{av_version_hash}(__VA_ARGS__,{argList},0)(__VA_ARGS__)\n")
    def AVAILABILITY_VERSION_CHECK(self, output, filename):
        av_version_string = avVersion
        av_version_hash = zlib.adler32(avVersion.encode()) # This does not need to cryptographically secure as it is meant to detect accidental failures
        output.write(f"#ifndef __AVAILABILITY_VERSIONS_VERSION_HASH\n")
        output.write(f"#define __AVAILABILITY_VERSIONS_VERSION_HASH {av_version_hash}U\n")
        output.write(f'#define __AVAILABILITY_VERSIONS_VERSION_STRING "{av_version_string}"\n')
        output.write(f'#define __AVAILABILITY_FILE "{filename}"\n')
        output.write(f"#elif __AVAILABILITY_VERSIONS_VERSION_HASH != {av_version_hash}U\n")
        output.write(f'#pragma GCC error "Already found AvailabilityVersions version " __AVAILABILITY_FILE " from " __AVAILABILITY_VERSIONS_VERSION_STRING ", which is incompatible with {filename} from {av_version_string}. Mixing and matching Availability from different SDKs is not supported"\n')
        output.write(f"#endif /* __AVAILABILITY_VERSIONS_VERSION_HASH */\n")

parser = argparse.ArgumentParser()
group = parser.add_mutually_exclusive_group()
for (name, platform) in versions.platforms().items():
    group.add_argument("--{}".format(name), default=False, action='store_true', help="Prints all SDK versions defined for {}".format(name))
    for alias in platform.cmd_aliases:
        group.add_argument("--{}".format(alias), dest=name, default=False, action='store_true', help="Alias for --{}".format(name))
group.add_argument("--sets",                                default=False, action='store_true', help='Prints all YAML for all defined version sets')
group.add_argument("--preprocess",                          nargs=2,                            help=argparse.SUPPRESS)
parser.add_argument("--av_version",                         nargs=1,                            help='Version string to embed in preprocessed headers')

args = parser.parse_args()
if args.av_version:
    avVersion = args.av_version


if args.sets:                                   print_sets();
elif args.preprocess:                           Preprocessor(args.preprocess[0], args.preprocess[1]);
else:
    for platform in versions.platforms().keys():
        if getattr(args, platform, None):
            print_versions(platform)
            parser.exit(0)
    parser.print_help()
    parser.exit(2)

