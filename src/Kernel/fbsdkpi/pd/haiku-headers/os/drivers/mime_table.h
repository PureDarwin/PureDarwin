/*******************************************************************************
/
/	File:			mime_table.h
/
/	Description:	Kernel mime table and matcher module API
/
/	Copyright 2005, François Revol.
/
*******************************************************************************/

#ifndef _MIME_TABLE_MODULE_H_
#define _MIME_TABLE_MODULE_H_

#include <module.h>

struct ext_mime {
	char *extension;
	char *mime;
};

#ifdef __cplusplus
extern "C" {
#endif

/*
	status_t get_table(struct ext_mime **table)
		Returns the current kernel mime table.
		You must call free_table() when finished with it.

	void free_table(struct ext_mime *table)
		Frees the given mime table;

	const char * mime_for_ext(const char *ext)
		Returns the known mime type for the given extension.

	const char * ext_for_mime(const char *mime)
		Returns the known extension for the given mime type.

*/

#define B_MIME_TABLE_MODULE_NAME "generic/mime_table/v1"

typedef struct {
	module_info		minfo;
	status_t	(*get_table)(struct ext_mime **table);
	void		(*free_table)(struct ext_mime *table);
	const char *	(*mime_for_ext)(const char *ext);
	const char *	(*ext_for_mime)(const char *mime);
} mime_table_module_info;

#ifdef __cplusplus
}
#endif

#endif

