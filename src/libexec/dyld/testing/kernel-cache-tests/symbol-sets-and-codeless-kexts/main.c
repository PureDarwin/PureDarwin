
// This will be re-exported from a symbol set in bar with an alias
int symbol_from_xnu() {
	return 0;
}

// This will be re-exported from a symbol set in bar without an alias
int symbol_from_xnu_no_alias() {
	return 0;
}

int _start() {
	return 0;
}