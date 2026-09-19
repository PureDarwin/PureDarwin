// Hyper-V sockets as an AF_VSOCK transport. Each connection is its own VMBus
// channel offered by the host. Framing follows FreeBSD's BSD-licensed hv_sock
#ifndef PD_HV_SOCK_H
#define PD_HV_SOCK_H

#include "PDHyperV.h"

#define VMBUS_CHANMSG_CHRESCIND     2
#define VMBUS_CHANMSG_CHCLOSE       7
#define VMBUS_CHANMSG_CHFREE        13
#define VMBUS_CHANMSG_TL_CONN       21
#define VMBUS_CHANMSG_TL_RESULT     23
#define VMBUS_CHAN_TLNPI_PROVIDER_OFFER 0x2000

class PDHyperV;

void pd_hvsock_init(PDHyperV *bus);
// Called on the bus work loop. The heavy lifting happens on thread calls
void pd_hvsock_offer(const struct vmbus_chanmsg_choffer *offer);
void pd_hvsock_rescind(uint32_t chanid);
void pd_hvsock_tl_result(const uint8_t *data, uint32_t len);

#endif
