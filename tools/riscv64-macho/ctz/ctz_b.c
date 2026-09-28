// both de Bruijn ctz tables, in the other order
int ctz32_b(unsigned x) { return __builtin_ctz(x); }
int ctz64_b(unsigned long long x) { return __builtin_ctzll(x); }
