/*
 * Copyright 2024 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

/* This header is empty. It is there so that #include <features.h> in other headers in this
 * directory will work, regardless of the presence of headers/bsd/ in the include search path.
 *
 * When headers/bsd is not in the search path, no extra features should be enabled (strict POSIX
 * and ANSI C compatibility). When headers/bsd is in the search path, features are enabled
 * depending on the C standard selected by the compiler command line.
 */
