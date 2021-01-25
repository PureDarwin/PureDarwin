#ifndef __OS_WORKGROUP_INTERVAL_PRIVATE__
#define __OS_WORKGROUP_INTERVAL_PRIVATE__

#ifndef __OS_WORKGROUP_INDIRECT__
#error "Please #include <os/workgroup_private.h> instead of this file directly."
#include <os/workgroup_base.h> // For header doc
#endif

__BEGIN_DECLS

OS_WORKGROUP_ASSUME_NONNULL_BEGIN

/*
 * @typedef os_workgroup_interval_type_t
 *
 * @abstract
 * Describes a specialized os_workgroup_interval type the client would like to
 * create.
 *
 * Clients need the 'com.apple.private.kernel.work-interval' entitlement to
 * create all workgroups types listed below except the following:
 *
 * OS_WORKGROUP_INTERVAL_TYPE_DEFAULT,
 * OS_WORKGROUP_INTERVAL_TYPE_CA_CLIENT,
 * OS_WORKGROUP_INTERVAL_TYPE_AUDIO_CLIENT,
 *
 * Note that only real time threads are allowed to join workgroups of type
 * OS_WORKGROUP_INTERVAL_TYPE_AUDIO_CLIENT and
 * OS_WORKGROUP_INTERVAL_TYPE_COREAUDIO.
 */
OS_ENUM(os_workgroup_interval_type, uint16_t,
	OS_WORKGROUP_INTERVAL_TYPE_DEFAULT = 0x1,
	OS_WORKGROUP_INTERVAL_TYPE_CA_CLIENT,
	OS_WORKGROUP_INTERVAL_TYPE_AUDIO_CLIENT,

	OS_WORKGROUP_INTERVAL_TYPE_COREAUDIO,
	OS_WORKGROUP_INTERVAL_TYPE_COREANIMATION,
	OS_WORKGROUP_INTERVAL_TYPE_CA_RENDER_SERVER,
	OS_WORKGROUP_INTERVAL_TYPE_HID_DELIVERY,
	OS_WORKGROUP_INTERVAL_TYPE_COREMEDIA,
);

/*
 * @function os_workgroup_attr_set_interval_type
 *
 * @abstract
 * Specifies that the os_workgroup_interval_t to be created should be of a
 * specialized type. These types should only be specified when creating an
 * os_workgroup_interval_t using the os_workgroup_interval_create API - using it
 * with any other workgroup creation API will result in an error at creation
 * time.
 *
 * Setting type OS_WORKGROUP_INTERVAL_TYPE_DEFAULT on an os_workgroup_interval_t
 * is a no-op.
 *
 * EINVAL is returned if the attribute passed in hasn't been initialized.
 */
API_AVAILABLE(macos(10.16), ios(14.0), tvos(14.0), watchos(7.0))
OS_WORKGROUP_EXPORT
int
os_workgroup_attr_set_interval_type(os_workgroup_attr_t attr,
	os_workgroup_interval_type_t type);

/*
 * @abstract
 * Creates an os_workgroup_interval_t with the specified name and attributes.
 * This object tracks a repeatable workload characterized by a start time, end
 * time and targeted deadline. Example use cases include audio and graphics
 * rendering workloads.
 *
 * A newly created os_workgroup_interval_t has no initial member threads - in
 * particular the creating thread does not join the os_workgroup_interval_t
 * implicitly.
 *
 * @param name
 * A client specified string for labelling the workgroup. This parameter is
 * optional and can be NULL.
 *
 * @param clockid
 * The clockid in which timestamps passed to the os_workgroup_interval_start()
 * and os_workgroup_interval_update() functions are specified.
 *
 * @param attrs
 * The requested set of os_workgroup_t attributes. NULL is to be specified for
 * the default set of attributes. By default, an interval workgroup
 * is nonpropagating with asynchronous work and differentiated from other threads
 * in the process (see os_workgroup_attr_flags_t).
 *
 * The OS_WORKGROUP_ATTR_UNDIFFERENTIATED attribute is invalid to specify for
 * interval workgroups. If it isn't or if invalid attributes are specified, this
 * function returns NULL and sets errno.
 */
API_AVAILABLE(macos(10.16), ios(14.0), tvos(14.0), watchos(7.0))
OS_WORKGROUP_EXPORT OS_WORKGROUP_RETURNS_RETAINED
os_workgroup_interval_t _Nullable
os_workgroup_interval_create(const char * _Nullable name, os_clockid_t clock,
		os_workgroup_attr_t _Nullable attr);

/* This SPI is for use by Audio Toolbox only. This function returns a reference
 * which is the responsibility of the caller to manage.
 */
API_AVAILABLE(macos(10.16), ios(14.0), tvos(14.0), watchos(7.0))
OS_WORKGROUP_EXPORT OS_WORKGROUP_RETURNS_RETAINED
os_workgroup_t
os_workgroup_interval_copy_current_4AudioToolbox(void);

OS_WORKGROUP_ASSUME_NONNULL_END

__END_DECLS
#endif /* __OS_WORKGROUP_INTERVAL_PRIVATE__ */
