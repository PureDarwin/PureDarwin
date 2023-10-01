#pragma once

#include <iostream>
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

inline ostream& operator <<(ostream& stream, CXString string) {
    const char *str = clang_getCString(string);
    stream << str;
    return stream;
}

#define assertion_failure(msg) __assert_rtn(__func__, __FILE__, __LINE__, msg)
