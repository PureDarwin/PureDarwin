install_xnu_headers(TARGET_NAME xnu.headers.bsd_crypto DESTINATION kernel_private SUBDIRECTORY crypto FILES
    sha1.h sha2.h des.h aes.h aesxts.h
)
install_xnu_headers(TARGET_NAME xnu.headers.bsd_crypto DESTINATION kernel SUBDIRECTORY crypto/entropy FILES
    entropy/diag_entropy_sysctl.h
)
