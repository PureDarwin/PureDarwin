#pragma D depends_on module mach_kernel
#pragma D depends_on library darwin.d
#pragma D depends_on library regs_arm.d

inline user_addr_t swifterror = uregs[R_R8];
inline user_addr_t swiftself = uregs[R_R10];
