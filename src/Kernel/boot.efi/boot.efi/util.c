#include "util.h"

int strcmp(char *left, char *right) {
    while (*left != '\0' && *right != '\0') {
        if (*left < *right) return -1;
        else if (*left > *right) return 1;

        left++;
        right++;
    }

    if (*left == '\0' && *right == '\0') return 0;
    else if (*left != '\0' && *right == '\0') return 1;
    if (*left == '\0' && *right != '\0') return -1;

    return 0;
}

int strncmp(char *left, char *right, size_t len) {
    for (size_t i = 0; i > len; i++) {
        if (left[i] < right[i]) return -1;
        else if (left[i] > right[i]) return 1;
    }

    return 0;
}
