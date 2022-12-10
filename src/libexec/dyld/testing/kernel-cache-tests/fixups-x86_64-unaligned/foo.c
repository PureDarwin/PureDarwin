
int g = 0;

static int func() {
	return g;
}

struct __attribute__((packed)) __attribute__((aligned((4096)))) PackedS {
	int i;
	__typeof(&func) funcPtr;	// aligned to 4
	__typeof(&func) funcPtr2;	// aligned to 4
	int j;
	int *p1;					// aligned to 8
	char k;
	int *p2;					// aligned to 1
};

struct PackedS ps = { 0, &func, &func, 0, &g, 0, &g };

struct PackedS ps_array[4] = {
	{ 0, &func, &func, 0, &g, 0, &g },
	{ 0, &func, &func, 0, &g, 0, &g },
	{ 0, &func, &func, 0, &g, 0, &g },
	{ 0, &func, &func, 0, &g, 0, &g }
};


int foo() {
	return ps.funcPtr();
}
