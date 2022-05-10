//
//  diagnostics.c
//  cctools libstuff
//
//  Created by Michael Trent on 6/17/20.
//
//  For more information on the CC_LOG_DIAGNOSTICS file format, see also
//  rdar://64357409 and the clang's lib/Frontend/LogDiagnosticPrinter.cpp.

#include "diagnostics.h"

#include "stuff/errors.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * diagnostics_state is a ternary value: -1 if uninitialized, 1 if diagnostic
 * logging is enabled, 0 if not.
 */
static int diagnostics_state = -1;

struct diagnostics_info {
    const char* log_file;
    const char* main_file;
    const char* tool;
    const char* args;
};

struct diagnostic {
    const char* level;
    char* message;
};

/*
 * diagnostics is the array of diagnostic messages logged during program
 * operation. The count of this array is stored in ndiagnostic.
 */
static struct diagnostic* diagnostics;
static int ndiagnostic;
static struct diagnostics_info info;

static void diagnostics_atexit(void);

/*
 * diagnostic_level_name is a utility for converting enum diagnostic_level to
 * a well-known string value.
 */
static const char* diagnostic_level_name(enum diagnostic_level level)
{
    const char* names[] = {
	"warning",
	"error",
	"fatal error",
    };
    int nname = sizeof(names) / sizeof(*names);

    if (level >= 0 && level < nname)
	return names[level];

    return names[ERROR];
}

void diagnostics_enable(enum bool enable)
{
    if (diagnostics_state == -1) {
	atexit(diagnostics_atexit);
    }

    diagnostics_state = enable ? 1 : 0;
}

void diagnostics_output(const char* logfile)
{
    if (info.log_file)
	free((void*)info.log_file);
    info.log_file = logfile ? strdup(logfile) : NULL;
}

enum bool diagnostics_enabled(void)
{
    return diagnostics_state == 1 ? TRUE : FALSE;
}

void diagnostics_log_args(int argc, char** argv)
{
    char* buf;
    size_t len;
    FILE* stream;

    if (argc > 0)
	info.tool = argv[0];

    if (info.args) {
	free((void*)info.args);
	info.args = NULL;
    }

    stream = open_memstream(&buf, &len);
    if (stream) {
	for (int i = 1; i < argc; ++i) {
	    fprintf(stream, "%s%s", i == 1 ? "" : " ", argv[i]);
	}
	fclose(stream);
	info.args = strdup(buf);
	free(buf);
    }
}

void diagnostics_log_msg(enum diagnostic_level level, const char* message)
{
    diagnostics = reallocf(diagnostics, sizeof(*diagnostics) * (ndiagnostic+1));
    struct diagnostic* d = &diagnostics[ndiagnostic++];

    d->level = diagnostic_level_name(level);
    d->message = strdup(message);
}

void diagnostics_write(void)
{
    char* buf;
    size_t len;
    FILE* stream;
    int fd;

    /*
     * do nothing if nothing to say. this prevents tools from writing noise
     * into the CC_LOG_DIAGNOSTICS file on exit.
     */
    if (ndiagnostic == 0)
	return;

    /*
     * write XML data to a dynamic in-memory buffer.
     */
    stream = open_memstream(&buf, &len);
    if (!stream)
	return;

    fprintf(stream, "<dict>\n");

    if (info.tool) {
	fprintf(stream, "  <key>tool</key>\n");
	fprintf(stream, "  <string>%s</string>\n", info.tool);
    }
    else if (progname) {
	fprintf(stream, "  <key>tool</key>\n");
	fprintf(stream, "  <string>%s</string>\n", progname);
    }

    if (info.args) {
	fprintf(stream, "  <key>args</key>\n");
	fprintf(stream, "  <string>%s</string>\n", info.args);
    }

    fprintf(stream, "  <key>diagnostics</key>\n");
    fprintf(stream, "  <array>\n");
    for (int i = 0; i < ndiagnostic; ++i) {
	struct diagnostic* d = &diagnostics[i];
	fprintf(stream, "    <dict>\n");
	fprintf(stream, "      <key>level</key>\n");
	fprintf(stream, "      <string>%s</string>\n", d->level);
	/*
	 * ID is not meaningful to cctools.
	 *
	 *fprintf(stream, "      <key>ID</key>\n");
	 *fprintf(stream, "      <integer>%d</integer>\n", 0);
	 */
	if (d->message) {
	    fprintf(stream, "      <key>message</key>\n");
	    fprintf(stream, "      <string>%s</string>\n", d->message);
	}
	fprintf(stream, "    </dict>\n");
    }
    fprintf(stream, "  </array>\n");
    fprintf(stream, "</dict>\n");

    fclose(stream);

    /*
     * open the CC_LOG_DIAGNOSTICS file. If no file was specified, write to
     * the stderr filehandle.
     */
    if (info.log_file) {
	fd = open(info.log_file, O_WRONLY | O_APPEND | O_CREAT, 0644);
	if (-1 == fd) {
	    fprintf(stderr, "error: cannot open file at %s: %s\n",
		    info.log_file, strerror(errno));
	    free(buf);
	    return;
	}
    }
    else {
	fd = STDERR_FILENO;
    }

    /*
     * write the XML data to the log file. By writing the entire contents to
     * file in a single syscall, we ensure the write is atomic. (Also, because
     * we need the write to be atomic, we will use the write syscall directly
     * rather than libstuff's write64.)
     */
    write(fd, buf, len);

    /*
     * clean up
     */
    if (fd != STDERR_FILENO)
	close(fd);

    free(buf);

    for (int i = 0; i < ndiagnostic; ++i) {
	struct diagnostic* d = &diagnostics[i];
	free(d->message);
    }
    free(diagnostics);
    diagnostics = NULL;
    ndiagnostic = 0;
}

void diagnostics_atexit(void)
{
    if (diagnostics_enabled())
	diagnostics_write();
}
