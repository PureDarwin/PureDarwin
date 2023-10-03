#ifndef CC_PRIVATE_CCEC_EXTRA_H
#define CC_PRIVATE_CCEC_EXTRA_H

#define ccec_cp_ccn_size(x) ((8 * ccec_cp_n(x)) + 3)
#define ccec_cp_size(x) (sizeof(struct cczp) + ccec_cp_ccn_size(x))

#define ccec_cp_p(x)  (x.prime->ccn)
#define ccec_cp_pr(x) (x.prime->ccn + 1 * ccec_cp_n(x))
#define ccec_cp_a(x)  (x.prime->ccn + 2 * ccec_cp_n(x) + 1)
#define ccec_cp_b(x)  (x.prime->ccn + 3 * ccec_cp_n(x) + 1)
#define ccec_cp_x(x)  (x.prime->ccn + 4 * ccec_cp_n(x) + 1)
#define ccec_cp_y(x)  (x.prime->ccn + 5 * ccec_cp_n(x) + 1)
#define ccec_cp_o(x)  (x.prime->ccn + 6 * ccec_cp_n(x) + 1)
#define ccec_cp_or(x) (x.prime->ccn + 7 * ccec_cp_n(x) + 1)
#define ccec_cp_h(x)  (x.prime->ccn + 8 * ccec_cp_n(x) + 2)

#endif // CC_PRIVATE_CCEC_EXTRA_H
