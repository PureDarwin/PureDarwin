
extern int symbol_from_bar();
extern int symbol_from_xnu_no_alias();

int foo() {
	return symbol_from_bar() + symbol_from_xnu_no_alias();
}
