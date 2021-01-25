#!/usr/bin/env ruby

require 'yaml'

$availCmd = ARGV[0]

def versionString(vers)
    uvers = ""
    if vers =~ /^(\d+)$/
        uvers = "#{$1}_0"
    elsif vers =~ /^(\d+).(\d+)$/
        uvers = "#{$1}_#{$2}"
    elsif vers =~ /^(\d+).(\d+).(\d+)$/
        if $3 == 0
            uvers = "#{$1}_#{$2}"
        else
            uvers = "#{$1}_#{$2}_#{$3}"
        end
    end
    uvers
end

def versionHex(vers)
    major = 0;
    minor = 0;
    revision = 0;

    if vers =~ /^(\d+)$/
        major = $1.to_i;
    elsif vers =~ /^(\d+).(\d+)$/
        major = $1.to_i;
        minor = $2.to_i;
    elsif vers =~ /^(\d+).(\d+).(\d+)$/
        major = $1.to_i;
        minor = $2.to_i;
        revision = $3.to_i;
    end
    "0x00#{major.to_s(16).rjust(2, '0')}#{minor.to_s(16).rjust(2, '0')}#{revision.to_s(16).rjust(2, '0')}"
end

def expandVersions(prefix, arg)
    versionList = `#{$availCmd} #{arg}`.gsub(/\s+/m, ' ').strip.split(" ")
    versionList.each { |version|
        puts "#define #{prefix}#{versionString(version)}".ljust(48, ' ') + versionHex(version)
    }
end

def printPlatformVersion(name, platform, version)
    puts "#define #{name}".ljust(56, ' ') + "({ (dyld_build_version_t){#{platform}, #{version}}; })"
end

def expandPlatformVersions(prefix, define_prefix, platform, arg)
    versionList = `#{$availCmd} #{arg}`.gsub(/\s+/m, ' ').strip.split(" ")
    versionList.each { |version|
	printPlatformVersion("dyld_platform_version_#{prefix}_#{versionString(version)}", platform, "#{define_prefix}#{versionString(version)}")
    }
end

$minorLookupTable = {
    "late_winter" => "0115",
    "spring" => "0301",
    "late_spring" => "0415",
    "summer" => "0601",
    "late_summer" => "0715",
    "fall" => "0901",
    "autumn" => "0902", # autumn has to come after fall because of ios 13.0 and 13.1
    "late_fall" => "1015",
    "winter" => "1201"
}

def setNameToVersion(setName)
    if setName =~ /^(.*)\_(\d{4})$/
        if not $minorLookupTable.key?($1)
            abort("Unknown platform set substring \"#{$1}\"")
        end
        "0x00#{$2.to_i.to_s(16)}#{$minorLookupTable[$1]}"
    else
        abort("Could not parse set identiifer \"#{setName}\"")
    end
end

def expandVersionSets()
    versionSets = YAML.load(`#{$availCmd} --sets`)
    versionSets.map { |set, value|
	puts "// dyld_#{set}_os_versions => " + value.sort.to_h.map { | platform, version | "#{platform} #{version}"}.join(" / ")
	printPlatformVersion("dyld_#{set}_os_versions", "0xffffffff", setNameToVersion(set))
	puts
    }
end

expandVersions("DYLD_MACOSX_VERSION_", "--macosx")
puts
expandVersions("DYLD_IOS_VERSION_", "--ios")
puts
expandVersions("DYLD_WATCHOS_VERSION_", "--watchos")
puts
expandVersions("DYLD_TVOS_VERSION_", "--appletvos")
puts
expandVersions("DYLD_BRIDGEOS_VERSION_", "--bridgeos")
puts
expandVersionSets()
puts
expandPlatformVersions("macOS", "DYLD_MACOSX_VERSION_", "PLATFORM_MACOS", "--macosx")
puts
expandPlatformVersions("iOS", "DYLD_IOS_VERSION_", "PLATFORM_IOS", "--ios")
puts
expandPlatformVersions("watchOS", "DYLD_WATCHOS_VERSION_", "PLATFORM_WATCHOS", "--watchos")
puts
expandPlatformVersions("tvOS", "DYLD_TVOS_VERSION_", "PLATFORM_TVOS", "--appletvos")
puts
expandPlatformVersions("bridgeOS", "DYLD_BRIDGEOS_VERSION_", "PLATFORM_BRIDGEOS", "--bridgeos")
puts
