#include <mach/kmod.h>

extern kern_return_t _start(kmod_info_t *ki, void *data);
extern kern_return_t _stop(kmod_info_t *ki, void *data);
@KEXT_MAIN_FUNCTION_DECL@
@KEXT_ANTIMAIN_FUNCTION_DECL@

__attribute__((visibility("default"))) KMOD_EXPLICIT_DECL(@KEXT_IDENTIFIER@, "@KEXT_VERSION@", _start, _stop)
__private_extern__ kmod_start_func_t *_realmain = @KEXT_MAIN_FUNCTION@;
__private_extern__ kmod_stop_func_t *_antimain = @KEXT_ANTIMAIN_FUNCTION@;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
