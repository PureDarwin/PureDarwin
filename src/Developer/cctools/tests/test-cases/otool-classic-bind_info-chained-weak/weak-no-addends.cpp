
#include <errno.h>
#include <stdlib.h>

__attribute__((weak))
int a = 0;

const int arraySize = 3;

__attribute__((weak))
int b[arraySize] = { 0 };

int* bPtr = &b[0];
int* bPtr2 = &b[0];

__attribute__((weak, visibility(("hidden"))))
int c = 0;

//int* freePtr = ((int*)&free) + 0;
int* errnoPtr = ((int*)&errno) + 0;

__attribute__((weak))
void* operator new(size_t size) {
	return malloc(size);
}

int main() {
	delete(new int());
	return a + *bPtr + c + *bPtr2 + *errnoPtr;
}