#add_darwin_shared_library(libsystem_malloc NO_STANDARD_LIBRARIES INSTALL_NAME_DIR /usr/lib/system)


add_library(libmalloc_headers INTERFACE)
target_include_directories(libmalloc_headers INTERFACE include)
