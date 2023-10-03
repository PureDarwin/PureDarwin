#ifndef _CCDH_GP_H_
#define _CCDH_GP_H_

#include <corecrypto/ccdh.h>

// Various function that we will need stubs for later
ccdh_const_gp_t ccdh_gp_rfc5114_MODP_1024_160();
ccdh_const_gp_t ccdh_gp_rfc5114_MODP_2048_224();
ccdh_const_gp_t ccdh_gp_rfc5114_MODP_2048_256();
ccdh_const_gp_t ccdh_gp_rfc3526group05();
ccdh_const_gp_t ccdh_gp_rfc3526group14();
ccdh_const_gp_t ccdh_gp_rfc3526group15();
ccdh_const_gp_t ccdh_gp_rfc3526group16();
ccdh_const_gp_t ccdh_gp_rfc3526group17();
ccdh_const_gp_t ccdh_gp_rfc3526group18();
ccdh_const_gp_t ccsrp_gp_rfc5054_1024();
ccdh_const_gp_t ccsrp_gp_rfc5054_2048();
ccdh_const_gp_t ccsrp_gp_rfc5054_3072();
ccdh_const_gp_t ccsrp_gp_rfc5054_4096();
ccdh_const_gp_t ccsrp_gp_rfc5054_8192();
ccdh_const_gp_t ccdh_gp_rfc2409group02();

#endif
