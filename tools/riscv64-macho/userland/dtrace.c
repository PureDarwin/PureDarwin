// usdt probes as dtrace -h headers emit them, ld64 patches the calls and builds the dof
extern void probe_fire(long) __asm("___dtrace_probe$pdtest$fire$v1$6c6f6e67");
extern int probe_fire_enabled(void) __asm("___dtrace_isenabled$pdtest$fire$v1");
extern void probe_stub_for_linkage(void) __asm("___dtrace_typedefs$pdtest$v2");
int fire(long x) { if (probe_fire_enabled()) probe_fire(x); return (int)x; }
