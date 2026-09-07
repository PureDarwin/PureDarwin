/* swc: launch/devmajor-openbsd.c
 *
 * Copyright (c) 2020 Nia Alarie
 * Copyright (c) 2026 uint
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "devmajor.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool
devname_is(dev_t rdev, const char *prefix)
{
	const char *name;
	size_t len;

	name = devname(rdev, S_IFCHR);
	if (!name || name[0] == '?' || name[1] == '?') {
		return false;
	}

	len = strlen(prefix);
	return strncmp(name, prefix, len) == 0;
}

bool
device_is_input(dev_t rdev)
{
	if (devname_is(rdev, "wskbd")) {
		return true;
	}
	if (devname_is(rdev, "wsmouse")) {
		return true;
	}
	if (devname_is(rdev, "wsmux")) {
		return true;
	}
	return false;
}

bool
device_is_tty(dev_t rdev)
{
	return devname_is(rdev, "ttyC");
}

bool
device_is_drm(dev_t rdev)
{
	const char *n;

	n = devname(rdev, S_IFCHR);
	if (!n) {
		return false;
	}

	return strncmp(n, "drm", 3) == 0 || strncmp(n, "dri/card", 8) == 0 ||
	       strncmp(n, "dri/renderD", 11) == 0;
}
