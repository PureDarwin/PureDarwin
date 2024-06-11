#include "util.h"
#include <x86_64/efibind.h>
#include <lib.h>

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

int strlen(char *string) {
    size_t len = 0;

    while (*string != '\0') {
        string++;
        len++;
    }

    return len;
}

extern char *strcpy(char *destination, const char *source) {
    if (*source == '\0') {
        *destination = '\0';
        return destination;
    }

    do {
        *destination = *source;
        destination++;
        source++;
    } while (*source != '\0');

    return destination;
}

void bzero(void *s, size_t n) {
    if (n == 0) return;

    char *ptr = (char *)s;
    for (size_t i = 0; i < n; i++) {
        ptr[i] = 0;
    }
}

void *malloc(size_t size) {
    return AllocatePool(size);
}

void *calloc(size_t count, size_t size) {
    return AllocateZeroPool(count * size);
}

void free(void *ptr) {
    return FreePool(ptr);
}
