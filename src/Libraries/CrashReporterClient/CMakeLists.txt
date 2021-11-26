add_darwin_static_library(CrashReporterClient)

target_sources(CrashReporterClient PRIVATE CrashReporterClient.c)

# For stdint.h
target_link_libraries(CrashReporterClient PUBLIC xnu_kernel_headers)

target_include_directories(CrashReporterClient PUBLIC include)

# I believe this is where this should be placed. The configure-dyld-archives.sh script looks for it here at least.
install(TARGETS CrashReporterClient DESTINATION usr/local/lib COMPONENT DeveloperInternal)

install(FILES include/CrashReporterClient.h DESTINATION usr/local/include COMPONENT DeveloperInternal)
