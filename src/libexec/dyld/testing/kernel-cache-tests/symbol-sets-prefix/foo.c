
extern int symbol_from_xnu0();
extern int symbol_from_xnu1();
extern int symbol_from_xnu2();
extern int symbol_from_xnu3();

int foo() {
	return symbol_from_xnu0() + symbol_from_xnu1() + symbol_from_xnu2() + symbol_from_xnu3();
}
