# Platform DSL
# This describes all our platforms, and versions
# It is used to autogenerate defines for availability markup
# By generating everything form a common source we guarantee consistency

# platform parameters (positional)
# 1: Platform short name (used for command line arguments to availability.pl)
# 2: Stylized name (used for platform aware dyld version strings)
# 3: Macro name (used by default in macro expansions)
# Optionals:
# cmd_aliases: comma separated list os additional aliases for use with availability.pl
# dyld_version_define_name: an override for the name that appears in legacy (non-platform) dyld version numbers. Defaults to macro name
# availability_aliases: comma separated set of alias that can be used in API_AVAILABLE() markup functions instead of the platform name
# min_max_define_name: an override for the name that appears _VERSION_MIN_REQUIRED/_VERSION_MAX_ALLOWED macros. Defaults to macro name
# -short_version_numbers: Boolean to enable shorter form version numbers
# availability_deprecation_define_name: an override for the name that appears __API_TO_BE_DEPRECATED_ macros. Defaults to macro name
# platform_define_name: an override for the name that appears PLATFORM_ macros. Defaults to macro name
# version_define_name: an override for the name that appears version defines (__MAC_10_1, etc). Defaults to macro name
# ios_implicit_min: Sets an implicit ios min value for iOS derived platforms
# bleached: Boolean to mark a platform as bleached from the pulic SDKs

platform    macos               macOS 		MACOS			cmd_aliases=macosx,osx		dyld_version_define_name=MACOSX	min_max_define_name=MAC_OS_X	availability_aliases=macosx -short_version_numbers availability_deprecation_define_name=MACOS version_define_name=MAC -supports_legacy_environment_defines
variant     macos               macOSApplicationExtension       MACOSAPPLICATIONEXTENSION
version     macos               10.0
version     macos               10.1
version     macos               10.2
version     macos               10.3
version     macos               10.4
version     macos               10.5
version     macos               10.6
version     macos               10.7
platform    ios                 iOS         IPHONE		dyld_version_define_name=IOS	min_max_define_name=IPHONE_OS platform_define_name=IOS availability_deprecation_define_name=IOS -supports_legacy_environment_defines
variant     ios                 iOSApplicationExtension IOSAPPLICATIONEXTENSION
variant     ios                 macCatalyst   MACCATALYST
variant     ios                 macCatalystApplicationExtension MACCATALYSTAPPLICATIONEXTENSION
version     ios                 2.0
version     ios                 2.1
version     ios                 2.2
version     ios                 3.0
version     ios                 3.1
version     ios                 3.2
version     ios                 4.0
version     ios                 4.1
version     ios                 4.2
version     ios                 4.3
version     ios                 5.0
set         fall_2011           2011.9.1	0.0
version     ios                 5.1
version     macos               10.8
version     ios                 6.0
set         fall_2012           2012.9.1	0.0
version     macos               10.9
version     ios                 6.1
version     ios                 7.0
set         fall_2013           2013.9.1	0.0
version     macos               10.10
version     ios                 7.1
version     ios                 8.0
set         fall_2014           2014.9.1	0.0
platform    watchos             watchOS     WATCHOS		ios_implicit_min=9.0	min_max_define_name=WATCH_OS -supports_legacy_environment_defines
variant     watchos             watchOSApplicationExtension     WATCHOSAPPLICATIONEXTENSION
version     watchos             1.0
version     macos               10.10.2
version     macos               10.10.3
version     macos               10.11
version     ios                 8.1
version     ios                 8.2
version     ios                 8.3
version     ios                 8.4
version     ios                 9.0
platform    tvos                tvOS        TVOS		cmd_aliases=appletvos	ios_implicit_min=9.0 min_max_define_name=TV_OS -supports_legacy_environment_defines
variant     tvos                tvOSApplicationExtension        TVOSAPPLICATIONEXTENSION 
version     tvos                9.0
version     watchos             2.0
set         fall_2015           2015.9.1	0.0
version     macos               10.11.2
version     macos               10.11.3
version     macos               10.11.4
version     macos               10.12
version     ios                 9.1
version     ios                 9.2
version     ios                 9.3
version     ios                 10.0
version     watchos             2.1
version     watchos             2.2
version     tvos                9.1
version     tvos                9.2
version     tvos                10.0
version     watchos             3.0
set         fall_2016           2016.9.1	0.0
version     macos               10.12.1
version     macos               10.12.2
version     macos               10.12.4
version     ios                 10.1
version     ios                 10.2
version     ios                 10.3
version     tvos                10.0.1
version     tvos                10.1
version     tvos                10.2
version     watchos             3.1
version     watchos             3.1.1
version     watchos             3.2
version     macos               10.13
version     ios                 11.0
version     tvos                11.0
version     watchos             4.0
platform    bridgeos            bridgeOS	BRIDGEOS	ios_implicit_min=11.0 min_max_define_name=BRIDGE_OS -bleached
version     bridgeos            2.0
set         fall_2017           2017.9.1	0.0
version     macos               10.13.1
version     macos               10.13.2
version     ios                 11.1
version     ios                 11.2
version     tvos                11.1
version     tvos                11.2
version     watchos             4.1
version     watchos             4.2
set         winter_2017         2017.18.1	0.0
version     macos               10.13.4
version     ios                 11.3
version     tvos                11.3
version     watchos             4.3
set         spring_2018         2018.3.1	0.0
version     ios                 11.4
version     tvos                11.4
version     macos               10.14
version     ios                 12.0
version     tvos                12.0
version     watchos             5.0
version     bridgeos            3.0
set         fall_2018           2018.9.1	0.0
version     macos               10.14.1
version     ios                 12.1
version     tvos                12.1
version     watchos             5.1
version     bridgeos            3.1
set         late_fall_2018      2018.16.21	0.0
version     macos               10.14.4
version     ios                 12.2
version     tvos                12.2
version     watchos             5.2
version     bridgeos            3.4
set         spring_2019         2019.3.1	0.0
version     ios                 12.3
version     tvos                12.3
version     macos               10.14.5
set         summer_2019         2019.6.1	0.0
version     macos               10.14.6
version     ios                 12.4
version     tvos                12.4
version     watchos             5.3
set         late_summer_2019    2019.7.21	0.0
version     macos               10.15
version     ios                 13.0
version     tvos                13.0
version     watchos             6.0
version     bridgeos            4.0
platform    driverkit           driverkit    DRIVERKIT
version     driverkit           19.0
set         fall_2019           2019.9.1	0.0
version     ios                 13.1
set         autumn_2019         2019.9.2	0.0
version     macos               10.15.1
version     ios                 13.2
version     tvos                13.2
version     watchos             6.1
version     bridgeos            4.1
set         late_fall_2019      2019.16.21	0.0
version     ios                 13.3
version     tvos                13.3
set         winter_2019         2019.18.1	0.0
version     macos               10.15.4
version     ios                 13.4
version     tvos                13.4
version     watchos             6.2
set         spring_2020         2020.3.1	0.0
version     ios                 13.5
set         late_spring_2020    2020.4.21	0.0
version     ios                 13.6
set         summer_2020         2020.6.1	0.0
version     ios                 13.7
set         late_summer_2020    2020.7.21	0.0
version     macos               10.16
version     ios                 14.0
version     tvos                14.0
version     watchos             7.0
version     bridgeos            5.0
version     driverkit           20.0
set         fall_2020           2020.9.1	0.0
version     ios                 14.1
version     tvos                14.1
version     watchos             7.1
version     ios                 14.2
version     tvos                14.2
set         late_fall_2020      2020.16.21	0.0
version     macos               11.0
version     ios                 14.3
version     tvos                14.3
version     watchos             7.2
version     bridgeos            5.1
version     macos               11.1
set         winter_2020         2020.18.1	0.0
version     ios                 14.5
version     tvos                14.5
version     watchos             7.3
version     watchos             7.4
version     bridgeos            5.3
version     macos               11.3
set         spring_2021         2021.3.1	0.0
version     macos               11.4
version     macos               11.5
version     macos               11.6
version     ios                 14.6
version     ios                 14.7
version     ios                 14.8
version     tvos                14.6
version     tvos                14.7
version     watchos             7.5
version     watchos             7.6
version     macos               12.0
version     ios                 15.0
version     tvos                15.0
version     watchos             8.0
version     bridgeos            6.0
version     driverkit           21.0
set         fall_2021           2021.9.1    0.0
version     ios                 15.1
version     tvos                15.1
version     watchos             8.1
set         late_fall_2021      2021.16.21  0.0
version     macos               12.1
version     ios                 15.2
version     tvos                15.2
version     watchos             8.3
version     bridgeos            6.2
set         winter_2021         2021.18.01  0.0
version     macos               12.2
version     ios                 15.3
version     tvos                15.3
version     watchos             8.4
set         late_winter_2021    2021.18.21  0.0
version     macos               12.3
version     ios                 15.4
version     tvos                15.4
version     watchos             8.5
version     bridgeos            6.4
set         spring_2022         2022.3.1    0.0
version     macos               12.4
version     ios                 15.5
version     tvos                15.5
version     watchos             8.6
version     bridgeos            6.5
set         late_spring_2022    2022.4.15   0.0
version     macos               12.5
version     ios                 15.6
version     tvos                15.6
version     watchos             8.7
version     bridgeos            6.6
set         summer_2022         2022.6.1    0.0
# Security updates that shipped after final normal SU
version     macos               12.6
version     macos               12.7
version     watchos             8.8
version     ios                 15.7
version     ios                 15.8
version     macos               13.0
version     ios                 16.0
version     tvos                16.0
version     watchos             9.0
version     bridgeos            7.0
version     driverkit           22.0
set         fall_2022           2022.9.1    0.0
version     macos               13.1
version     ios                 16.1
version     tvos                16.1
version     watchos             9.1
version     bridgeos            7.1
set         late_fall_2022      2022.16.21  0.0
version     watchos             9.2
version     tvos                16.2
version     ios                 16.2
set         2022_SU_C           2022.33.0   0.0
version     macos               13.2
version     watchos             9.3
version     tvos                16.3
version     ios                 16.3
version     bridgeos            7.2
set         2022_SU_D           2022.34.0   0.0
version     driverkit           22.4
version     macos               13.3
version     ios                 16.4
version     tvos                16.4
version     watchos             9.4
version     bridgeos            7.3
set         2022_SU_E           2022.35.0   22.4.0
version     driverkit           22.5
version     macos               13.4
version     ios                 16.5
version     tvos                16.5
version     watchos             9.5
version     bridgeos            7.4
set         2022_SU_F           2022.36.0   22.5.0
version     driverkit           22.6
version     macos               13.5
version     ios                 16.6
version     tvos                16.6
version     watchos             9.6
version     bridgeos            7.6
version     macos               13.6
version     ios                 16.7
version     macos               13.7
set         2022_SU_G           2022.37.0   22.6.0
version     macos               14.0
version     ios                 17.0
version     tvos                17.0
version     watchos             10.0
version     bridgeos            8.0
version     driverkit           23.0
set         fall_2023           2023.9.1    23.0.0
platform    visionos            visionOS    VISIONOS      min_max_define_name=VISION_OS   cmd_aliases=xros ios_implicit_min=17.1
variant     visionos            visionOSApplicationExtension    VISIONOSAPPLICATIONEXTENSION
version     visionos            1.0
version     macos               14.1
version     ios                 17.1
version     tvos                17.1
version     watchos             10.1
version     bridgeos            8.1
version     driverkit           23.1
set         2023_SU_B           2023.12.1    23.1.0
version     macos               14.2
version     ios                 17.2
version     tvos                17.2
version     watchos             10.2
version     bridgeos            8.2
version     driverkit           23.2
set         2023_SU_C           2023.13.1    23.2.0
version     macos               14.3
version     ios                 17.3
version     tvos                17.3
version     watchos             10.3
version     bridgeos            8.3
version     driverkit           23.3
set         2023_SU_D           2023.14.1    23.3.0

# PureDarwin extension. Apple's last published AvailabilityVersions drop is
# 157.2 (macOS 14.3); everything below is ours so the table can express the
# version this system actually reports. Only macOS is extended - PureDarwin
# has no other platform - and the seed-update sets Apple uses between releases
# are omitted, since nothing outside their build system names them.
version     macos               14.4
version     macos               14.5
version     macos               14.6
version     macos               14.7
version     macos               15.0
set         fall_2024           2024.9.1    24.0.0
version     macos               15.1
version     macos               15.2
version     macos               15.3
version     macos               15.4
version     macos               15.5
version     macos               15.6
version     macos               26.0
set         fall_2025           2025.9.1    25.0.0
version     macos               26.1
version     macos               26.2
version     macos               26.3
version     macos               26.4
version     macos               26.5
