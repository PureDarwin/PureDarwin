#ifndef corecrypto_ccsrp_gp_h
#define corecrypto_ccsrp_gp_h

#include <corecrypto/ccdh.h>

ccdh_const_gp_t ccsrp_gp_rfc5054_1024(void);
ccdh_const_gp_t ccsrp_gp_rfc5054_2048(void);
ccdh_const_gp_t ccsrp_gp_rfc5054_3072(void);
ccdh_const_gp_t ccsrp_gp_rfc5054_4096(void);
ccdh_const_gp_t ccsrp_gp_rfc5054_8192(void);

#endif
