#pragma D depends_on module mach_kernel
#pragma D depends_on library darwin.d
#pragma D depends_on library regs_arm64.d

inline user_addr_t swifterror = curpsinfo->pr_dmodel == PR_MODEL_LP64 ? uregs[R_X21] : uregs[R_R8];
inline user_addr_t swiftself = curpsinfo->pr_dmodel == PR_MODEL_LP64 ? uregs[R_X20] : uregs[R_R10];
