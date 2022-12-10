
// This will be re-exported from a symbol set in bar with an alias
int symbol_from_xnu() __asm("__ZN15OSMetaClassBase8DispatchE5IORPC");
int symbol_from_xnu() {
	return 0;
}

int _start() {
	return 0;
}