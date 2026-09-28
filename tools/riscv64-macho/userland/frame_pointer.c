// darwin keeps frame records in non-leaf functions, like arm64 darwin
int f(int x);
int leaf(int x) { return x * 3 + 1; }
int nonleaf(int x) { return f(x) + 1; }
