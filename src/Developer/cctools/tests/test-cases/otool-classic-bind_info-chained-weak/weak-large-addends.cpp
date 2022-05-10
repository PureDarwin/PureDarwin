
#include <errno.h>
#include <stdlib.h>

__attribute__((weak))
int a = 0;

const int arraySize = 1000000;

__attribute__((weak))
int b[arraySize] = { 0 };

int* bPtr = &b[1];
int* bPtr2 = &b[arraySize - 1];

__attribute__((weak, visibility(("hidden"))))
int c = 0;

//int* freePtr = ((int*)&free) + 1;
int* errnoPtr = ((int*)&errno) + 1;

__attribute__((weak))
void* operator new(size_t size) {
	return malloc(size);
}

int main() {
	delete(new int());
	return a + *bPtr + c + *bPtr2 + *errnoPtr + errno;
}