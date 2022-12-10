//
//  xcode.c
//  stuff
//
//  Created by Michael Trent on 4/24/20.
//

#include "stuff/write64.h"

#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

const char* xcode_developer_path(void)
{
    static const char* xc_dev_path;

    if (!xc_dev_path) {
	void* handle = dlopen("/usr/lib/libxcselect.dylib", RTLD_NOW);
	if (handle) {
	    bool (*xc_get_path)(char *buffer, int buffer_size,
				bool *was_environment, bool *was_cltools,
				bool *was_default);
	    xc_get_path = dlsym(handle, "xcselect_get_developer_dir_path");
	    if (xc_get_path) {
		char path[MAXPATHLEN];
		bool ignore;
		if (xc_get_path(path, MAXPATHLEN, &ignore, &ignore, &ignore)) {
		    xc_dev_path = (const char*)strdup(path);
		}
	    }
	}
    }

    return xc_dev_path;
}
