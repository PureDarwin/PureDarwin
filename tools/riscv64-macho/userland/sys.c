// stand-in libSystem: just the entry points a c++ dylib and its client reference
typedef unsigned long size_t;
int puts(const char *s) { return 0; }
int printf(const char *f, ...) { return 0; }
void _tlv_bootstrap(void) {}
void *__cxa_allocate_exception(size_t n) { return 0; }
void __cxa_throw(void *e, void *t, void (*d)(void *)) { for (;;); }
void *__cxa_begin_catch(void *e) { return e; }
void __cxa_end_catch(void) {}
int __gxx_personality_v0(void) { return 0; }
void _Unwind_Resume(void *e) { for (;;); }
void __cxa_free_exception(void *e) {}
void *_Znwm(size_t n) { return 0; }
void _ZdlPv(void *p) {}
void _ZdlPvm(void *p, size_t n) {}
void __cxa_pure_virtual(void) {}
void *_ZTVN10__cxxabiv117__class_type_infoE[4];
void *_ZTVN10__cxxabiv120__si_class_type_infoE[4];
void _exit(int c) { for (;;); }
void *__dynamic_cast(const void *p, const void *s, const void *d, long o) { return (void *)p; }
// the objc entry points compiled objc code calls
void *objc_msgSend(void *self, void *sel, ...) { return self; }
void *objc_opt_self(void *c) { return c; }
void *_objc_empty_cache;
