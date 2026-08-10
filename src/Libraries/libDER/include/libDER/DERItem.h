/*
 * DERItem.h - the (pointer, length) pair every libDER entry point traffics in.
 */
#ifndef _DER_ITEM_H_
#define _DER_ITEM_H_

#include <libDER/libDER_config.h>

__BEGIN_DECLS

typedef struct {
    DERByte *data;
    DERSize  length;
} DERItem;

__END_DECLS

#endif /* _DER_ITEM_H_ */
