#ifndef __OS_WORKGROUP_OBJECT_PRIVATE__
#define __OS_WORKGROUP_OBJECT_PRIVATE__

#ifndef __OS_WORKGROUP_INDIRECT__
#error "Please #include <os/workgroup.h> instead of this file directly."
#include <os/workgroup_base.h> // For header doc
#endif

#include <os/workgroup_object.h>

__BEGIN_DECLS

OS_WORKGROUP_ASSUME_NONNULL_BEGIN

/* Attribute creation and specification */

/* This is for clients who want to build their own workgroup attribute from
 * scratch instead of configuring their attributes on top of the default set of
 * attributes */
#define OS_WORKGROUP_ATTR_INITIALIZER_EMPTY { .sig = _OS_WORKGROUP_ATTR_SIG_EMPTY_INIT }

/*!
 * @enum os_workgroup_attr_flags_t
 *
 * @abstract A bitfield of flags describing options for workgroup configuration
 */
OS_ENUM(os_workgroup_attr_flags, uint32_t,
	/*!
	 * @const OS_WORKGROUP_ATTR_NONPROPAGATING
	 *
	 * Asynchronous work initiated by threads which are members of a
	 * workgroup with OS_WORKGROUP_ATTR_NONPROPAGATING attribute, will not
	 * automatically be tracked as part of the workgroup. This applies to work
	 * initiated by calls such as dispatch_async() that may propagate other
	 * execution context properties.
	 *
	 * os_workgroups which are propagating by default can opt out this behavior
	 * by specifying the OS_WORKGROUP_ATTR_NONPROPAGATING flag.
	 */
	OS_WORKGROUP_ATTR_NONPROPAGATING = (1 << 1),

	/*!
	 * @const OS_WORKGROUP_ATTR_UNDIFFERENTIATED
	 *
	 * Member threads of a workgroup with the attribute flag
	 * OS_WORKGROUP_ATTR_UNDIFFERENTIATED are tracked and measured together with
	 * other threads in their process by the system for scheduling and
	 * performance control.
	 *
	 * os_workgroups which are tracked separately from other threads in
	 * the process by default, can opt out of it by specifying the
	 * OS_WORKGROUP_ATTR_UNDIFFERENTIATED flag.
	 */
	OS_WORKGROUP_ATTR_UNDIFFERENTIATED = (1 << 2)
);

/*!
 * @function os_workgroup_attr_set_flags
 *
 * @abstract
 * Sets the user specified flags in the workgroup attribute. If invalid
 * attributes are specified, this function will set and return an error.
 */
API_AVAILABLE(macos(10.16), ios(14.0), tvos(14.0), watchos(7.0))
OS_WORKGROUP_EXPORT OS_WORKGROUP_WARN_RESULT
int
os_workgroup_attr_set_flags(os_workgroup_attr_t wga,
		os_workgroup_attr_flags_t flags);


/*!
 * @function os_workgroup_create
 *
 * @abstract
 * Creates an os_workgroup_t with the specified name and attributes.
 * A newly created os_workgroup_t has no initial member threads - in particular
 * the creating thread does not join the os_workgroup_t implicitly.
 *
 * @param name
 * A client specified string for labelling the workgroup. This parameter is
 * optional and can be NULL.
 *
 * @param wga
 * The requested set of os_workgroup_t attributes. NULL is to be specified for
 * the default set of attributes. A workgroup with default attributes is
 * propagating with asynchronous work and differentiated from other threads in
 * the process (see os_workgroup_attr_flags_t).
 *
 * The attribute flag OS_WORKGROUP_ATTR_NONPROPAGATING MUST currently be
 * specified. If it isn't or if invalid attributes are specified, this function
 * will return NULL and set an errno.
 */
API_AVAILABLE(macos(10.16), ios(14.0), tvos(14.0), watchos(7.0))
OS_WORKGROUP_EXPORT OS_WORKGROUP_RETURNS_RETAINED
os_workgroup_t _Nullable
os_workgroup_create(const char * _Nullable name,
	os_workgroup_attr_t _Nullable wga);

/* To be deprecated once coreaudio adopts */
#define OS_WORKGROUP_ATTR_INITIALIZER OS_WORKGROUP_ATTR_INITIALIZER_DEFAULT

typedef uint32_t os_workgroup_index;

/* Deprecated in favor of os_workgroup_join */
OS_WORKGROUP_EXPORT OS_WORKGROUP_WARN_RESULT
int
os_workgroup_join_self(os_workgroup_t wg, os_workgroup_join_token_t token_out,
		os_workgroup_index *_Nullable id_out);

/* Deprecated in favor of os_workgroup_leave */
OS_WORKGROUP_EXPORT
void
os_workgroup_leave_self(os_workgroup_t wg, os_workgroup_join_token_t token);

OS_WORKGROUP_ASSUME_NONNULL_END

__END_DECLS

#endif /* __OS_WORKGROUP_OBJECT__ */
