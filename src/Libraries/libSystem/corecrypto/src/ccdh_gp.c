#include <corecrypto/ccdh_gp.h>
#include <corecrypto/ccstubs.h>

static const ccdh_const_gp_t stub_gp = {
    .gp = NULL
};

ccdh_const_gp_t ccdh_gp_rfc5114_MODP_1024_160() {
	CC_STUB(stub_gp)
}

ccdh_const_gp_t ccdh_gp_rfc5114_MODP_2048_224() {
	CC_STUB(stub_gp);
}

ccdh_const_gp_t ccdh_gp_rfc5114_MODP_2048_256() {
	CC_STUB(stub_gp);
}

ccdh_const_gp_t ccdh_gp_rfc3526group05() {
	CC_STUB(stub_gp);
}

ccdh_const_gp_t ccdh_gp_rfc3526group14() {
	CC_STUB(stub_gp);
}

ccdh_const_gp_t ccdh_gp_rfc3526group15() {
	CC_STUB(stub_gp);
}

ccdh_const_gp_t ccdh_gp_rfc3526group16() {
	CC_STUB(stub_gp);
}

ccdh_const_gp_t ccdh_gp_rfc3526group17() {
	CC_STUB(stub_gp);
}

ccdh_const_gp_t ccdh_gp_rfc3526group18() {
	CC_STUB(stub_gp);
}

ccdh_const_gp_t ccsrp_gp_rfc5054_1024() {
	CC_STUB(stub_gp);
}

ccdh_const_gp_t ccsrp_gp_rfc5054_2048() {
	CC_STUB(stub_gp);
}

ccdh_const_gp_t ccsrp_gp_rfc5054_3072() {
	CC_STUB(stub_gp);
}

ccdh_const_gp_t ccsrp_gp_rfc5054_4096() {
	CC_STUB(stub_gp);
}

ccdh_const_gp_t ccsrp_gp_rfc5054_8192() {
	CC_STUB(stub_gp);
}

ccdh_const_gp_t ccdh_gp_rfc2409group02() {
	CC_STUB(stub_gp);
}
