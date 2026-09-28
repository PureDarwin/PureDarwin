// global merge on mach-o may combine statics into a local symbol, never export one
int ext_a = 1, ext_b = 2;
static int st_c = 3, st_d = 4;
int merged_use(void) { return ext_a + ext_b + st_c++ + st_d++; }
