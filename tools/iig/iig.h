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
