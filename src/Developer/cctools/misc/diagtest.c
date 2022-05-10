//
//  diagtest.c
//  cctools
//
//  Created by Michael Trent on 6/18/20.
//

#include <stdio.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "stuff/bool.h"
#include "stuff/breakout.h"
#include "stuff/diagnostics.h"
#include "stuff/errors.h"

/* name of the program for error messages (argv[0]) */
char *progname = NULL;

/* flags from arugment processing */
static enum bool flagFatal;
static enum bool flagSystemFatal;
static enum bool flagMachFatal;
static enum bool flagArchive;

static void usage(void);

int
main(
int argc,
char *argv[],
char *envp[])
{
    diagnostics_enable(getenv("CC_LOG_DIAGNOSTICS") != NULL);
    diagnostics_output(getenv("CC_LOG_DIAGNOSTICS_FILE"));
    diagnostics_log_args(argc, argv);

    progname = argv[0];
    if (argc > 2)
	usage();
    if (argc == 2) {
	if (0 == strcmp(argv[1], "-fatal"))
	    flagFatal = TRUE;
	else if (0 == strcmp(argv[1], "-system_fatal"))
	    flagSystemFatal = TRUE;
	else if (0 == strcmp(argv[1], "-mach_fatal"))
	    flagMachFatal = TRUE;
	else if (0 == strcmp(argv[1], "-archive"))
	    flagArchive = TRUE;
	else
	    usage();
    }

    if (flagFatal) {
	fatal("this is a fatal error.");
    }

    if (flagSystemFatal) {
	errno = ENOENT;
	system_fatal("this is a fatal system error.");
    }

    if (flagMachFatal) {
	mach_fatal(KERN_CODESIGN_ERROR, "this is a fatal mach error.");
    }

    if (!flagArchive) {
	warning("this is an example warning.");
	error("this is an error that wil cause a non-zero exit.");
	error_with_arch("ppc", "this is error causes a non-zero exit.");
	errno = ENOENT;
	system_error("this is a file-not-found error.");
	my_mach_error(KERN_CODESIGN_ERROR, "this is a mach error.");
    }
    else {
	struct arch arch;
	struct member member;

	arch.file_name = "test.a";
	arch.fat_arch_name = "arm64e";
	member.member_name = "test.o";
	member.member_name_size = (uint32_t)strlen(member.member_name);

	warning_arch(&arch, NULL, "this is an arch warning: ");
	warning_arch(&arch, &member, "this is an arch member warning: ");
	error_arch(&arch, NULL, "this is an arch error: ");
	error_arch(&arch, &member, "this is an arch member error: ");
	fatal_arch(&arch, &member, "this is a fatal arch error: ");
    }

    return errors == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

static
void
usage(void)
{
    fprintf(stderr,
"usage: diagtest [<option>]\n"
"  option is one of:\n"
"    -fatal\n"
"    -system_fatal\n"
"    -mach_fatal\n"
"    -archive\n");
    exit(EXIT_FAILURE);
}
