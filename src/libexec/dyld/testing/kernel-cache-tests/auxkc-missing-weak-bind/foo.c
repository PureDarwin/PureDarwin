
__attribute__((weak))
extern int weakValue;

extern int gOSKextUnresolved;

int bar() {
	// Missing weak import test
	if ( &weakValue != &gOSKextUnresolved )
		return 0;
	return weakValue;
}