/*
 * os_state: the sysdiagnose "state dump" facility. A process registers a block
 * that libsystem serialises on demand when a state dump is collected.
 *
 * PureDarwin has no state-dump collector, so os_state_add_handler() registers
 * nothing and the block is simply never invoked - the documented behaviour when
 * no collector exists, and exactly what SCDynamicStore expects (it ignores the
 * return value). The types below match the real ABI so the handler block in
 * SCDOpen.c compiles unmodified.
 */

#ifndef _PUREDARWIN_OS_STATE_PRIVATE_H_
#define _PUREDARWIN_OS_STATE_PRIVATE_H_

#include <stdint.h>
#include <stddef.h>
#include <dispatch/dispatch.h>

__BEGIN_DECLS

#define OS_STATE_DATA_TITLE_SIZE 64

/* osd_type values */
#define OS_STATE_DATA_SERIALIZED_NSCF_OBJECT    1
#define OS_STATE_DATA_PROTOCOL_BUFFER           2
#define OS_STATE_DATA_CUSTOM                    3

typedef struct os_state_data_decoder_s {
	char osdd_library[64];
	char osdd_type[64];
} os_state_data_decoder_t;

typedef struct os_state_data_s {
	uint32_t                 osd_type;
	uint32_t                 osd_data_size;
	os_state_data_decoder_t  osd_decoder;
	char                     osd_title[OS_STATE_DATA_TITLE_SIZE];
	uint8_t                  osd_data[];
} *os_state_data_t;

#define OS_STATE_DATA_SIZE_NEEDED(size) \
	(sizeof(struct os_state_data_s) + (size))

/* Upper bound on a single state-dump payload, as enforced by the collector. */
#define MAX_STATEDUMP_SIZE (1024 * 1024)

typedef struct os_state_hints_s {
	uint32_t osh_flags;
	char     osh_requestor[64];
} *os_state_hints_t;

typedef uint64_t os_state_handle_t;

typedef os_state_data_t (^os_state_block_t)(os_state_hints_t hints);

static inline os_state_handle_t
os_state_add_handler(dispatch_queue_t queue, os_state_block_t block)
{
	(void)queue;
	(void)block;
	return 0;
}

static inline void
os_state_remove_handler(os_state_handle_t handle)
{
	(void)handle;
}

__END_DECLS

#endif /* _PUREDARWIN_OS_STATE_PRIVATE_H_ */
