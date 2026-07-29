/*
 * os_transaction: libdispatch SPI telling the system a process has work in
 * flight and should not be suspended or jetsammed. PureDarwin's libdispatch
 * does not implement it, and no libdarwin source references anything from this
 * header - internal.h simply includes it - so declare the opaque type alone.
 */

#ifndef _PUREDARWIN_OS_TRANSACTION_PRIVATE_H_
#define _PUREDARWIN_OS_TRANSACTION_PRIVATE_H_

typedef struct os_transaction_s *os_transaction_t;

#endif /* _PUREDARWIN_OS_TRANSACTION_PRIVATE_H_ */
