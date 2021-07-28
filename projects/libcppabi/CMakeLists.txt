add_darwin_shared_library(libcppabi NO_STANDARD_LIBRARIES INSTALL_NAME_DIR /usr/lib)
set_property(TARGET libcppabi PROPERTY PREFIX lib)
set_property(TARGET libcppabi PROPERTY OUTPUT_NAME c++abi)
target_sources(libcppabi PRIVATE
    src/abort_message.c
    src/cxa_apple.cxx
    src/cxa_demangle.cpp
    src/cxa_guard.cxx
    src/cxa_pure_virtual.cxx
    src/del_op.cc
    src/del_opnt.cc
    src/del_opvnt.cc
    src/eh_alloc.cc
    src/eh_arm.cc
    src/eh_aux_runtime.cc
    src/eh_call.cc
    src/eh_catch.cc
    src/eh_exception.cc
    src/eh_globals.cc
    src/eh_personality.cc
    src/eh_term_handler.cc
    src/eh_terminate.cc
    src/eh_type.cc
    src/eh_unex_handler.cc
    src/new_handler.cc
    src/new_op.cc
    src/new_opnt.cc
    src/new_opvnt.cc
    src/pure.cc
    src/tinfo.cc
    src/tinfo2.cc
    src/vec.cc
    src/vterminate.cc
)
target_link_libraries(libcppabi PRIVATE Libc_headers)
# target_link_libraries(libc++abi PRIVATE libSystem)


#add_firstpass_library(libcppabi)
