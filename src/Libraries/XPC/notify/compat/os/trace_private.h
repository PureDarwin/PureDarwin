#ifndef PD_OS_TRACE_PRIVATE_H
#define PD_OS_TRACE_PRIVATE_H

typedef unsigned int os_trace_mode_t;
#define OS_TRACE_MODE_DISABLE 0

static inline void
os_trace_set_mode(os_trace_mode_t mode)
{
	(void)mode;
}

#endif /* PD_OS_TRACE_PRIVATE_H */
