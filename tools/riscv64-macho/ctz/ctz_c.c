// both de Bruijn ctz tables, one copy per object
int ctz64_c(unsigned long long x) { return __builtin_ctzll(x); }
int ctz32_c(unsigned x) { return __builtin_ctz(x); }
