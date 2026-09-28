// a thread local read on both sides of 4KB, the descriptor address may be shared
_Thread_local int t;
int far_tlv(void) { int a = t; __asm__ volatile(".fill 1100, 4, 0x00000013"); return a + t; }
