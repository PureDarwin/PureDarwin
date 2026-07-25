/*
This file is part of Darling.

Copyright (C) 2017 Darling developers

Darling is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Darling is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Darling.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "xpc/xpc.h"
#include "xpc_internal.h"

OS_OBJECT_OBJC_CLASS_DECL(xpc_object);

/* The only key for XPC_ERROR_* dictionaries */
#define _XPC_ERROR_KEY_DESCRIPTION_STR "XPCErrorDescription"
const char *const _xpc_error_key_description = _XPC_ERROR_KEY_DESCRIPTION_STR;

/*
* XPC_ERROR_* constants are declared to be of type `struct _xpc_dictionary_s`
* in Apple's <xpc/connection.h>, yet we want `struct _xpc_dictionary_s *` to
* be interchangable with `struct xpc_object *`, because both types get passed
* as opaque pointers of type `xpc_object_t` to various XPC functions.
* Thankfully, `struct _xpc_dictionary_s` isn't used anywhere else, so we can
* make it a simple wrapper.
*/
struct _xpc_dictionary_s {
	struct xpc_object inner;
};

/*
* We cannot initialize these structures using any function because they are
* declared as `const` in Apple's <xpc/connection.h>
* See <sys/queue.h> for TAILQ_ENTRY & TAILQ_HEAD structure details.
*/

/* XPC_ERROR_CONNECTION_INTERRUPTED */

const struct _xpc_dictionary_s _xpc_error_connection_interrupted;

static const struct xpc_object _xpc_error_connection_interrupted_val = {
	.header = {
		.isa = &OS_xpc_object_class,
		.ref_cnt = _OS_OBJECT_GLOBAL_REFCNT,
		.xref_cnt = _OS_OBJECT_GLOBAL_REFCNT
	},
	.xo_xpc_type = XPC_TYPE_STRING,
	.xo_size = 22,		/* strlen("Connection interrupted") */
	.xo_u = {
		.str = "Connection interrupted"
	}
};

static const struct xpc_dict_pair _xpc_error_connection_interrupted_pair = {
	.key = _XPC_ERROR_KEY_DESCRIPTION_STR,
	.value = &_xpc_error_connection_interrupted_val,
	.xo_link = {
		.tqe_next = NULL,
		.tqe_prev = &_xpc_error_connection_interrupted.inner.xo_u.dict.tqh_first
	}
};

const struct _xpc_dictionary_s _xpc_error_connection_interrupted = {
	.inner = {
		.header = {
			.isa = &OS_xpc_object_class,
			.ref_cnt = _OS_OBJECT_GLOBAL_REFCNT,
			.xref_cnt = _OS_OBJECT_GLOBAL_REFCNT
		},
		.xo_xpc_type = XPC_TYPE_DICTIONARY,
		.xo_size = 1,
		.xo_u = {
			.dict = {
				.tqh_first = &_xpc_error_connection_interrupted_pair,
				.tqh_last = &_xpc_error_connection_interrupted_pair.xo_link.tqe_next
			}
		}
	}
};

/* XPC_ERROR_CONNECTION_INVALID */

const struct _xpc_dictionary_s _xpc_error_connection_invalid;

static const struct xpc_object _xpc_error_connection_invalid_val = {
	.header = {
		.isa = &OS_xpc_object_class,
		.ref_cnt = _OS_OBJECT_GLOBAL_REFCNT,
		.xref_cnt = _OS_OBJECT_GLOBAL_REFCNT
	},
	.xo_xpc_type = XPC_TYPE_STRING,
	.xo_size = 18,		/* strlen("Connection invalid") */
	.xo_u = {
		.str = "Connection invalid"
	}
};

static const struct xpc_dict_pair _xpc_error_connection_invalid_pair = {
	.key = _XPC_ERROR_KEY_DESCRIPTION_STR,
	.value = &_xpc_error_connection_invalid_val,
	.xo_link = {
		.tqe_next = NULL,
		.tqe_prev = &_xpc_error_connection_invalid.inner.xo_u.dict.tqh_first
	}
};

const struct _xpc_dictionary_s _xpc_error_connection_invalid = {
	.inner = {
		.header = {
			.isa = &OS_xpc_object_class,
			.ref_cnt = _OS_OBJECT_GLOBAL_REFCNT,
			.xref_cnt = _OS_OBJECT_GLOBAL_REFCNT
		},
		.xo_xpc_type = XPC_TYPE_DICTIONARY,
		.xo_size = 1,
		.xo_u = {
			.dict = {
				.tqh_first = &_xpc_error_connection_invalid_pair,
				.tqh_last = &_xpc_error_connection_invalid_pair.xo_link.tqe_next
			}
		}
	}
};

/* XPC_ERROR_TERMINATION_IMMINENT */

const struct _xpc_dictionary_s _xpc_error_termination_imminent;

static const struct xpc_object _xpc_error_termination_imminent_val = {
	.header = {
		.isa = &OS_xpc_object_class,
		.ref_cnt = _OS_OBJECT_GLOBAL_REFCNT,
		.xref_cnt = _OS_OBJECT_GLOBAL_REFCNT
	},
	.xo_xpc_type = XPC_TYPE_STRING,
	.xo_size = 20,		/* strlen("Termination imminent") */
	.xo_u = {
		.str = "Termination imminent"
	}
};

static const struct xpc_dict_pair _xpc_error_termination_imminent_pair = {
	.key = _XPC_ERROR_KEY_DESCRIPTION_STR,
	.value = &_xpc_error_termination_imminent_val,
	.xo_link = {
		.tqe_next = NULL,
		.tqe_prev = &_xpc_error_termination_imminent.inner.xo_u.dict.tqh_first
	}
};

const struct _xpc_dictionary_s _xpc_error_termination_imminent = {
	.inner = {
		.header = {
			.isa = &OS_xpc_object_class,
			.ref_cnt = _OS_OBJECT_GLOBAL_REFCNT,
			.xref_cnt = _OS_OBJECT_GLOBAL_REFCNT
		},
		.xo_xpc_type = XPC_TYPE_DICTIONARY,
		.xo_size = 1,
		.xo_u = {
			.dict = {
				.tqh_first = &_xpc_error_termination_imminent_pair,
				.tqh_last = &_xpc_error_termination_imminent_pair.xo_link.tqe_next
			}
		}
	}
};
