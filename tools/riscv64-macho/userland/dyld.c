// dyld-shaped: an entry, data pointers to code and data that need rebasing when it slides
static int table_data[4] = { 1, 2, 3, 4 };
static int helper(int x) { return x * 2; }
int (*const fn_table[2])(int) = { helper, helper };
int *data_ptr = table_data;
const char *msg = "dyld";
int _dyld_start_c(void) { return fn_table[1](*data_ptr) + msg[0]; }
