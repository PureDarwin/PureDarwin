# This project just exposes headers in the architecture/ directory that don't live elsewhere.
add_library(architecture_headers INTERFACE)

target_include_directories(architecture_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Install headers
install(DIRECTORY include/architecture DESTINATION usr/include COMPONENT DeveloperTools)

install(DIRECTORY include/architecture DESTINATION System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders COMPONENT DeveloperInternal)
