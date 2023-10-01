# CMake 3.27 insists on passing an mmacosx-version-min flag for every C/C++
# file compiled on the macOS platform. We use -target to specify the version
# of macOS we're targeting. With modern clang, this results in a warning on
# every C file. Don't do this.
add_compile_options(-Wno-overriding-t-option)

add_definitions(-DTARGET_OS_MACOSX=1)

add_subdirectory(Kernel)
add_subdirectory(Libraries)
