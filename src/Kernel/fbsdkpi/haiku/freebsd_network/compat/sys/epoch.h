/*
 * Copyright 2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_EPOCH_H_
#define _FBSD_COMPAT_SYS_EPOCH_H_


struct epoch_tracker {
	int dummy;
};

#define NET_EPOCH_ENTER(et)
#define NET_EPOCH_EXIT(et)
#define NET_EPOCH_ASSERT()


#endif	/* _FBSD_COMPAT_SYS_EPOCH_H_ */
