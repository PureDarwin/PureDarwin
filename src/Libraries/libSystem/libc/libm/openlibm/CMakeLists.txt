# openlibm (PureDarwin/Libm fork), vendored directly (BSD/FreeBSD/OpenBSD-derived,
# see LICENSE.md) - replaces the hand-rolled pd_libm_*.c files that used to
# live in the parent libm/ directory. Built as portable C only: the amd64/
# directory's optimized .S routines are skipped (fenv.c, which is plain C,
# is kept - it's the only thing there providing real fegetenv/fesetenv/
# feclearexcept/... instead of stubs) so this needs no CMake ASM setup.
add_darwin_static_library(openlibm_static)

target_sources(openlibm_static PRIVATE
    $<$<BOOL:${PUREDARWIN_ARM64}>:src/pd_arm64.c>
    # src/ - main portable C implementation (see src/Make.files)
    src/common.c
    src/e_acos.c src/e_acosf.c src/e_acosh.c src/e_acoshf.c
    src/e_asin.c src/e_asinf.c
    src/e_atan2.c src/e_atan2f.c src/e_atanh.c src/e_atanhf.c
    src/e_cosh.c src/e_coshf.c
    src/e_exp.c src/e_expf.c
    src/e_fmod.c src/e_fmodf.c
    src/e_hypot.c src/e_hypotf.c
    src/e_j0.c src/e_j0f.c src/e_j1.c src/e_j1f.c src/e_jn.c src/e_jnf.c
    src/e_lgamma.c src/e_lgamma_r.c src/e_lgammaf.c src/e_lgammaf_r.c
    src/e_log.c src/e_log10.c src/e_log10f.c src/e_log2.c src/e_log2f.c src/e_logf.c
    src/e_pow.c src/e_powf.c
    src/e_remainder.c src/e_remainderf.c
    src/e_rem_pio2.c src/e_rem_pio2f.c
    src/e_sinh.c src/e_sinhf.c
    src/e_sqrt.c src/e_sqrtf.c
    src/k_cos.c src/k_exp.c src/k_expf.c src/k_rem_pio2.c src/k_sin.c src/k_tan.c
    src/k_cosf.c src/k_sinf.c src/k_tanf.c
    src/s_asinh.c src/s_asinhf.c
    src/s_atan.c src/s_atanf.c
    src/s_carg.c src/s_cargf.c
    src/s_cbrt.c src/s_cbrtf.c
    src/s_ceil.c src/s_ceilf.c
    src/s_copysign.c src/s_copysignf.c
    src/s_cos.c src/s_cosf.c
    src/s_csqrt.c src/s_csqrtf.c
    src/s_erf.c src/s_erff.c
    src/s_exp2.c src/s_exp2f.c
    src/s_expm1.c src/s_expm1f.c
    src/s_fabs.c src/s_fabsf.c
    src/s_fdim.c
    src/s_floor.c src/s_floorf.c
    src/s_fma.c src/s_fmaf.c
    src/s_fmax.c src/s_fmaxf.c src/s_fmin.c src/s_fminf.c
    src/s_fpclassify.c
    src/s_frexp.c src/s_frexpf.c
    src/s_ilogb.c src/s_ilogbf.c
    src/s_isinf.c src/s_isfinite.c src/s_isnormal.c src/s_isnan.c
    src/s_llrint.c src/s_llrintf.c src/s_llround.c src/s_llroundf.c
    src/s_log1p.c src/s_log1pf.c
    src/s_logb.c src/s_logbf.c
    src/s_lrint.c src/s_lrintf.c
    src/s_lround.c src/s_lroundf.c
    src/s_modf.c src/s_modff.c
    src/s_nearbyint.c
    src/s_nextafter.c src/s_nextafterf.c
    src/s_nexttowardf.c
    src/s_remquo.c src/s_remquof.c
    src/s_rint.c src/s_rintf.c
    src/s_round.c src/s_roundf.c
    src/s_scalbln.c src/s_scalbn.c src/s_scalbnf.c src/s_ldexpf.c
    src/s_signbit.c src/s_signgam.c
    src/s_sin.c src/s_sincos.c src/s_sinf.c src/s_sincosf.c
    src/s_tan.c src/s_tanf.c src/s_tanh.c src/s_tanhf.c src/s_tgammaf.c
    src/s_trunc.c src/s_truncf.c
    src/s_cpow.c src/s_cpowf.c
    src/w_cabs.c src/w_cabsf.c
    src/s_nan.c
    # src/ - long double C99 additions (i387/amd64/aarch64 only; we're amd64)
    src/s_copysignl.c src/s_fabsl.c src/s_llrintl.c src/s_lrintl.c src/s_modfl.c
    src/e_acosl.c src/e_asinl.c src/e_atan2l.c src/e_fmodl.c
    src/s_fmaxl.c src/s_fminl.c src/s_ilogbl.c
    src/e_hypotl.c src/e_lgammal.c src/e_remainderl.c src/e_sqrtl.c
    src/s_atanl.c src/s_ceill.c src/s_cosl.c src/s_cprojl.c
    src/s_csqrtl.c src/s_floorl.c src/s_fmal.c
    src/s_frexpl.c src/s_logbl.c src/s_nexttoward.c
    src/s_remquol.c src/s_roundl.c src/s_lroundl.c src/s_llroundl.c
    src/s_cpowl.c src/s_cargl.c
    src/s_sinl.c src/s_sincosl.c src/s_tanl.c src/s_truncl.c src/w_cabsl.c
    src/s_nextafterl.c src/s_rintl.c src/s_scalbnl.c src/polevll.c
    src/s_casinl.c src/s_ctanl.c
    src/s_cimagl.c src/s_conjl.c src/s_creall.c src/s_cacoshl.c src/s_catanhl.c src/s_casinhl.c
    src/s_catanl.c src/s_csinl.c src/s_cacosl.c src/s_cexpl.c src/s_csinhl.c src/s_ccoshl.c
    src/s_clogl.c src/s_ctanhl.c src/s_ccosl.c src/s_cbrtl.c
    # src/ - C99 complex functions
    src/s_ccosh.c src/s_ccoshf.c src/s_cexp.c src/s_cexpf.c
    src/s_cimag.c src/s_cimagf.c
    src/s_conj.c src/s_conjf.c
    src/s_cproj.c src/s_cprojf.c src/s_creal.c src/s_crealf.c
    src/s_csinh.c src/s_csinhf.c src/s_ctanh.c src/s_ctanhf.c
    src/s_cacos.c src/s_cacosf.c
    src/s_cacosh.c src/s_cacoshf.c
    src/s_casin.c src/s_casinf.c src/s_casinh.c src/s_casinhf.c
    src/s_catan.c src/s_catanf.c src/s_catanh.c src/s_catanhf.c
    src/s_clog.c src/s_clogf.c
    # bsdsrc/ (see bsdsrc/Make.files)
    bsdsrc/b_exp.c bsdsrc/b_log.c bsdsrc/b_tgamma.c
    # ld80/ - long double 80-bit extended precision (i387/amd64) (see ld80/Make.files)
    ld80/invtrig.c
    ld80/e_acoshl.c ld80/e_powl.c ld80/k_tanl.c ld80/s_exp2l.c
    ld80/e_atanhl.c ld80/e_lgammal_r.c ld80/e_sinhl.c ld80/s_asinhl.c ld80/s_expm1l.c
    ld80/e_coshl.c ld80/e_log10l.c ld80/e_tgammal.c
    ld80/e_expl.c ld80/e_log2l.c ld80/k_cosl.c ld80/s_log1pl.c ld80/s_tanhl.c
    ld80/e_logl.c ld80/k_sinl.c ld80/s_erfl.c
    ld80/s_nanl.c
    # amd64/ - fenv.c only (real fegetenv/fesetenv/feclearexcept/...); the
    # optimized .S routines in this directory are intentionally not built.
    amd64/fenv.c
)

set(_openlibm_long_double_sources
    src/s_copysignl.c src/s_fabsl.c src/s_llrintl.c src/s_lrintl.c src/s_modfl.c
    src/e_acosl.c src/e_asinl.c src/e_atan2l.c src/e_fmodl.c
    src/s_fmaxl.c src/s_fminl.c src/s_ilogbl.c
    src/e_hypotl.c src/e_lgammal.c src/e_remainderl.c src/e_sqrtl.c
    src/s_atanl.c src/s_ceill.c src/s_cosl.c src/s_cprojl.c
    src/s_csqrtl.c src/s_floorl.c src/s_fmal.c
    src/s_frexpl.c src/s_logbl.c src/s_nexttoward.c
    src/s_remquol.c src/s_roundl.c src/s_lroundl.c src/s_llroundl.c
    src/s_cpowl.c src/s_cargl.c
    src/s_sinl.c src/s_sincosl.c src/s_tanl.c src/s_truncl.c src/w_cabsl.c
    src/s_nextafterl.c src/s_rintl.c src/s_scalbnl.c src/polevll.c
    src/s_casinl.c src/s_ctanl.c
    src/s_cimagl.c src/s_conjl.c src/s_creall.c src/s_cacoshl.c src/s_catanhl.c src/s_casinhl.c
    src/s_catanl.c src/s_csinl.c src/s_cacosl.c src/s_cexpl.c src/s_csinhl.c src/s_ccoshl.c
    src/s_clogl.c src/s_ctanhl.c src/s_ccosl.c src/s_cbrtl.c
    ld80/invtrig.c
    ld80/e_acoshl.c ld80/e_powl.c ld80/k_tanl.c ld80/s_exp2l.c
    ld80/e_atanhl.c ld80/e_lgammal_r.c ld80/e_sinhl.c ld80/s_asinhl.c ld80/s_expm1l.c
    ld80/e_coshl.c ld80/e_log10l.c ld80/e_tgammal.c
    ld80/e_expl.c ld80/e_log2l.c ld80/k_cosl.c ld80/s_log1pl.c ld80/s_tanhl.c
    ld80/e_logl.c ld80/k_sinl.c ld80/s_erfl.c ld80/s_nanl.c)
if(PUREDARWIN_ARCH STREQUAL "arm64")
    set_source_files_properties(${_openlibm_long_double_sources}
        PROPERTIES HEADER_FILE_ONLY TRUE)
    # The ARM fenv implementation is provided by the public compiler/runtime
    # ABI; openlibm has no separate ARM source file in this tree.
    set_source_files_properties(amd64/fenv.c PROPERTIES HEADER_FILE_ONLY TRUE)
endif()

set_target_properties(openlibm_static PROPERTIES EXCLUDE_FROM_ALL TRUE)

get_target_property(_openlibm_copts openlibm_static COMPILE_OPTIONS)
if(_openlibm_copts)
    list(REMOVE_ITEM _openlibm_copts -nostdinc)
    set_target_properties(openlibm_static PROPERTIES COMPILE_OPTIONS "${_openlibm_copts}")
endif()

target_compile_options(openlibm_static PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -include ${CMAKE_CURRENT_SOURCE_DIR}/src/cdefs-compat.h
    -Wno-implicit-function-declaration)
target_compile_definitions(openlibm_static PRIVATE
    __PUREDARWIN__=1
    __BSD_VISIBLE=1
    _FORTIFY_SOURCE=0)
target_include_directories(openlibm_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/amd64
    ${CMAKE_CURRENT_SOURCE_DIR}/ld80
)
add_dependencies(openlibm_static libc_features)
