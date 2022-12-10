
// Hack to force a section on __TEXT as otherwise its given the vmSize by forEachSegment
__attribute__((used, section("__TEXT, __const")))
int packHack = 0;

extern int foo();

int bar() {
	return foo();
}