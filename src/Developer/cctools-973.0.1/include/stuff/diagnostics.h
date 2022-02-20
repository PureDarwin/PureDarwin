//
//  diagnostics.h
//  cctools libstuff
//
//  Created by Michael Trent on 6/17/20.
//

#ifndef diagnostics_h
#define diagnostics_h

#include <stuff/bool.h>

enum diagnostic_level {
  WARNING = 0,
  ERROR = 1,
  FATAL = 2,
};

/*
 * diagnostics_enable() can be used to enable or disable diagnostic logging.
 * some cctools programs do not use diagnostic logging, as they are binary
 * introspection tools, or aren't expected to run in a build environment.
 * tools that do modify binaries as part of a build should enable diagnostic
 * logging so errors can be vectored off to a separate file.
 */
void diagnostics_enable(enum bool enable);

/*
 * diagnostics_output() can be used to direct diagnostic logs to a file.
 * If logfile is NULL or unspecified diagnostics will be written to STDERR.
 */
void diagnostics_output(const char* logfile);

/*
 * diagnostics_enabled() returns TRUE if diagnostics are enabled. Call this
 * function before logging or attempting to write.
 */
enum bool diagnostics_enabled(void);

/*
 * diagnostics_log_args() logs command arguments in the diagnostics output.
 */
void diagnostics_log_args(int argc, char** argv);

/*
 * diagnostics_log_msg() logs a single instance of a diagnostic message.
 */
void diagnostics_log_msg(enum diagnostic_level level, const char* message);

/*
 * diagnostics_write() writes the complete diagnostic XML structure to the file
 * specified by CC_LOG_DIAGNOSTICS_FILE environment variable or to stderr if
 * no such file is specified. The entire XML message is written atomically.
 *
 * Ordinarily, this will be called automatically at process exit. Call this
 * manually if you need to terminate the process via _exit, abort, or some
 * other unusual means. diagnostics_write() will do no work
 * if diagnostics_log_msg() has not been called since the previous call
 * diagnostics_write()
 */
void diagnostics_write(void);

#endif /* diagnostics_h */
