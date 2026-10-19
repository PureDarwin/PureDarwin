#ifndef _DRIVER_SETTINGS_H
#define _DRIVER_SETTINGS_H


#include <OS.h>


typedef struct driver_parameter {
	char *name;
	int	value_count;
	char **values;
	int parameter_count;
	struct driver_parameter *parameters;
} driver_parameter;

typedef struct driver_settings {
	int parameter_count;
	struct driver_parameter *parameters;
} driver_settings;


#ifdef __cplusplus
extern "C" {
#endif

extern void			*load_driver_settings(const char *driverName);
extern void			*load_driver_settings_file(int fd);
extern status_t		unload_driver_settings(void *handle);

extern void			*parse_driver_settings_string(const char *settingsString);
extern status_t		get_driver_settings_string(void *_handle, char *buffer,
						size_t *_bufferSize, bool flat);

extern const char	*get_driver_parameter(void *handle, const char *key,
						const char *unknownValue,	/* key not present */
						const char *noargValue);	/* key has no value */
extern bool			get_driver_boolean_parameter(void *handle, const char *key,
						bool unknownValue, bool noargValue);

extern const driver_settings *get_driver_settings(void *handle);

/* Pass this in as drivername to access safe mode settings */
#define B_SAFEMODE_DRIVER_SETTINGS	"/safemode/"

/* Pass this as the key value to check if safe mode is enabled */
#define B_SAFEMODE_SAFE_MODE		"safemode"

#ifdef __cplusplus
}
#endif

#endif	/* _DRIVER_SETTINGS_H */
