// both de Bruijn ctz tables, one copy per object
int ctz64_a(unsigned long long x) { return __builtin_ctzll(x); }
int ctz32_a(unsigned x) { return __builtin_ctz(x); }
