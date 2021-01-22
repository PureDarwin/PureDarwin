add_library(libctf STATIC)
target_sources(libctf PRIVATE
    libctf/ctf_create.c
    libctf/ctf_decl.c
    libctf/ctf_error.c
    libctf/ctf_hash.c
    libctf/ctf_labels.c
    libctf/ctf_lib.c
    libctf/ctf_lookup.c
    libctf/ctf_open.c
    libctf/ctf_subr.c
    libctf/ctf_util.c

    libctf/ctf_impl.h
    libctf/libctf.h
)
set_property(TARGET libctf PROPERTY PREFIX "")
target_include_directories(libctf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
