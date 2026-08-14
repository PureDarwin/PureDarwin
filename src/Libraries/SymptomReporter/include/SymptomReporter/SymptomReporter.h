#ifndef _SYMPTOM_REPORTER_H_
#define _SYMPTOM_REPORTER_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

typedef struct symptom_framework *symptom_framework_t;
typedef struct symptom *symptom_t;
typedef uint32_t symptom_ident_t;

/*
 * Register a reporter. `id` is the caller's numeric reporter id and `text_id`
 * its reverse-DNS name (configd uses 0x68 / "com.apple.configd"). Returns NULL
 * on failure, which every caller is expected to tolerate.
 */
symptom_framework_t symptom_framework_init(uint32_t id, const char *text_id);

/* Create a symptom of the given identifier. NULL on failure. */
symptom_t symptom_new(symptom_framework_t framework, symptom_ident_t ident);

/*
 * Attach a qualifier value to a symptom. `index` selects which qualifier slot;
 * IPConfiguration uses slot 0 for the interface index.
 */
int symptom_set_qualifier(symptom_t symptom, uint64_t value, uint32_t index);

/* Submit the symptom and release it. Returns 0 on success. */
int symptom_send(symptom_t symptom);

/*
 * The remainder of the framework's exported surface. No PureDarwin consumer
 * calls these yet, so their signatures are inferred from the naming and from
 * the shape of the calls above rather than from observed use - treat them as
 * provisional and check against a real caller before relying on them.
 */
symptom_t symptom_create(symptom_framework_t framework, symptom_ident_t ident);
int symptom_framework_set_version(symptom_framework_t framework, uint32_t version);
int symptom_framework_stats(symptom_framework_t framework);
int symptom_send_immediate(symptom_t symptom);
int symptom_set_additional_qualifier(symptom_t symptom, uint32_t type,
                                     size_t length, const void *value);
int symptom_set_additional_digest(symptom_t symptom, uint64_t digest);
int symptom_verbosity(symptom_framework_t framework, uint32_t level);

__END_DECLS

#endif /* _SYMPTOM_REPORTER_H_ */
