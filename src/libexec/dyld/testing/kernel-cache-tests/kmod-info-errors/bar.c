
#include "kmod.h"

int startKext() {
	return 0;
}
int endKext() {
	return 0;
}

KMOD_EXPLICIT_DECL(com.apple.bar, "1.0.0", startKext, endKext)

int foo() {
	return 0;
}
