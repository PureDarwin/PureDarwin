#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include <clang-c/Index.h>

using namespace std;

inline bool strequal(const char *s1, const char *s2) {
    return strcmp(s1, s2) == 0;
}

inline bool strnequal(const char *s1, const char *s2, size_t n) {
    return strncmp(s1, s2, n) == 0;
}

inline void fwrite(FILE *file, CXString string) {
    const char *str = clang_getCString(string);
    fwrite(str, sizeof(char), strlen(str), file);
}

inline void fwrite(FILE *file, const char *string) {
    fwrite(string, sizeof(char), strlen(string), file);
}

template<typename CharType>
inline void fwrite(FILE *file, basic_string<CharType> string) {
    fwrite(string.c_str(), sizeof(CharType), string.length, file);
}

#define assertion_failure(msg) __assert_rtn(__func__, __FILE__, __LINE__, msg)
