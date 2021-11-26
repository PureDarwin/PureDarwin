#include "internal.h"

#pragma mark Definitions
#define CTL_OUTPUT_WIDTH (80)
#define CTL_OUTPUT_OPTARG_INDENT (32)
#define CTL_OUTPUT_OPTARG_OVERFLOW (CTL_OUTPUT_OPTARG_INDENT - 4)
#define SUBCOMMAND_LINKER_SET "__subcommands"

#define OS_SUBCOMMAND_OPTIONS_FOREACH(_osco_i, _osc, _which, _i) \
		while (((_osco_i) = &osc->osc_ ## _which[(_i)]) && \
			((_i) += 1, 1) && \
			!((_osco_i)->osco_flags & OS_SUBCOMMAND_OPTION_FLAG_TERMINATOR))

#pragma mark Types
OS_ENUM(os_subcommand_option_spec_fmt, uint64_t,
	OS_SUBCOMMAND_OPTION_SPEC_SHORT,
	OS_SUBCOMMAND_OPTION_SPEC_LONG,
	OS_SUBCOMMAND_OPTION_SPEC_COMBINED,
);

#pragma mark Forward Declarations
static void _print_header(
		FILE *f,
		const char *hdr,
		bool *already_done);
static const os_subcommand_t *_os_subcommand_find(
		const char *name);
static void _os_subcommand_print_usage(
		const os_subcommand_t *osc,
		FILE *f);
static void _os_subcommand_print_help_line(
		const os_subcommand_t *osc,
		FILE *f);
static void _print_subcommand_list(
		const os_subcommand_t *osc,
		FILE *f);

#pragma mark Module Globals
static const os_subcommand_t __help_cmd;
static const os_subcommand_t *_help_cmd = &__help_cmd;

static const os_subcommand_t __main_cmd;
static const os_subcommand_t *_main_cmd = &__main_cmd;
static const os_subcommand_t *_internal_main_cmd = &__main_cmd;

static struct ttysize __ttys = {
	.ts_lines = 24,
	.ts_cols = CTL_OUTPUT_WIDTH,
};

static const struct ttysize *_ttys = &__ttys;

#pragma mark Module Private
static void
_init_column_count(void)
{
	const char *columns_env = NULL;
	char *end = NULL;
	struct ttysize ttys = {
		.ts_lines = 24,
		.ts_cols = CTL_OUTPUT_WIDTH,
	};
	int ret = -1;

	columns_env = getenv("COLUMNS");
	if (columns_env) {
		unsigned short cols = -1;

		cols = strtoul(columns_env, &end, 0);
		if (end != columns_env && end[0] != 0) {
			ttys.ts_lines = cols;
		}
	} else {
		ret = ioctl(0, TIOCGSIZE, &ttys);
		if (ret) {
			ttys.ts_lines = 24;
			ttys.ts_cols = CTL_OUTPUT_WIDTH;
		}
	}

	__ttys = ttys;
}

static void
_stoupper(char *str)
{
	size_t i = 0;
	size_t len = strlen(str);

	for (i = 0; i < len; i++) {
		char *cp = &str[i];
		*cp = ___toupper(*cp);
	}
}

#pragma mark Main Subcommand
static int _main_invoke(const os_subcommand_t *osc,
	int argc,
	const char *argv[]);

static const os_subcommand_option_t _main_positional[] = {
	[0] = {
		.osco_version = OS_SUBCOMMAND_OPTION_VERSION,
		.osco_flags = 0,
		.osco_option = NULL,
		.osco_argument_usage = "subcommand",
		.osco_argument_human = "The subcommand to invoke",
	},
	OS_SUBCOMMAND_OPTION_TERMINATOR,
};

static const os_subcommand_t __main_cmd = {
	.osc_version = OS_SUBCOMMAND_VERSION,
	.osc_flags = OS_SUBCOMMAND_FLAG_MAIN,
	.osc_name = "_main",
	.osc_desc = "main command",
	.osc_optstring = NULL,
	.osc_options = NULL,
	.osc_required = NULL,
	.osc_optional = NULL,
	.osc_positional = _main_positional,
	.osc_invoke = &_main_invoke,
};

static int
_main_invoke(const os_subcommand_t *osc, int argc, const char *argv[])
{
	return 0;
}

#pragma mark Help Subcommand
static int _help_invoke(const os_subcommand_t *osc,
	int argc,
	const char *argv[]);

static const os_subcommand_option_t _help_positional[] = {
	[0] = {
		.osco_version = OS_SUBCOMMAND_OPTION_VERSION,
		.osco_flags = 0,
		.osco_option = NULL,
		.osco_argument_usage = "SUBCOMMAND",
		.osco_argument_human = "The subcommand to query for help",
	},
	OS_SUBCOMMAND_OPTION_TERMINATOR,
};

static const os_subcommand_t __help_cmd = {
	.osc_version = OS_SUBCOMMAND_VERSION,
	.osc_flags = 0,
	.osc_name = "help",
	.osc_desc = "prints helpful information",
	.osc_optstring = NULL,
	.osc_options = NULL,
	.osc_required = NULL,
	.osc_optional = NULL,
	.osc_positional = _help_positional,
	.osc_invoke = &_help_invoke,
};

static int
_help_invoke(const os_subcommand_t *osc, int argc, const char *argv[])
{
	int xit = -1;
	const char *cmdname = NULL;
	const os_subcommand_t *target = NULL;
	FILE *f = stdout;

	if (argc > 1) {
		cmdname = argv[1];
	}

	// Print usage information for the requested subcommand.
	target = _os_subcommand_find(cmdname);
	if (!target) {
		// If it's a bogus subcommand, just print top-level usage.
		fprintf(stderr, "unrecognized subcommand: %s\n", cmdname);
		target = _main_cmd;
		xit = EX_UNAVAILABLE;
	} else {
		xit = 0;
	}

	if (xit) {
		f = stderr;
	}

	_os_subcommand_print_usage(target, f);

	if (target == _main_cmd) {
		_print_subcommand_list(_help_cmd, f);
	}

	return xit;
}

#pragma mark Utilities
static void
_print_header(FILE *f, const char *hdr, bool *already_done)
{
	if (already_done && *already_done) {
		return;
	}

	crfprintf_np(f, "");
	crfprintf_np(f, "%s:", hdr);
	crfprintf_np(f, "");

	if (already_done) {
		*already_done = true;
	}
}

#pragma mark Module Routines
static char *
_os_subcommand_copy_option_spec_short(const os_subcommand_t *osc,
		const os_subcommand_option_t *osco)
{
	const struct option *opt = osco->osco_option;
	char optbuff[64] = "";
	char argbuff[64] = "";
	char *final = NULL;
	int ret = -1;

	if (opt) {
		snprintf(optbuff, sizeof(optbuff), "-%c", opt->val);

		switch (opt->has_arg) {
		case no_argument:
			break;
		case optional_argument:
			snprintf(argbuff, sizeof(argbuff), "[%s]",
					osco->osco_argument_usage);
			break;
		case required_argument:
			snprintf(argbuff, sizeof(argbuff), "<%s>",
					osco->osco_argument_usage);
			break;
		default:
			__builtin_unreachable();
		}

		if (!(osco->osco_flags & OS_SUBCOMMAND_OPTION_FLAG_ENUM)) {
			_stoupper(argbuff);
		}
	} else {
		snprintf(optbuff, sizeof(optbuff), "%s", osco->osco_argument_usage);
		if (!(osco->osco_flags & OS_SUBCOMMAND_OPTION_FLAG_ENUM)) {
			_stoupper(optbuff);
		}
	}

	ret = asprintf(&final, "%s%s", optbuff, argbuff);
	if (ret < 0) {
		os_assert_zero(ret);
	}

	return final;
}

static char *
_os_subcommand_copy_option_spec_long(const os_subcommand_t *osc,
		const os_subcommand_option_t *osco)
{
	const struct option *opt = osco->osco_option;
	char optbuff[64] = "";
	char argbuff[64] = "";
	char *final = NULL;
	int ret = -1;

	if (opt) {
		snprintf(optbuff, sizeof(optbuff), "--%s", opt->name);

		switch (opt->has_arg) {
		case no_argument:
			break;
		case optional_argument:
			snprintf(argbuff, sizeof(argbuff), "[=%s]",
					osco->osco_argument_usage);
			break;
		case required_argument:
			snprintf(argbuff, sizeof(argbuff), "=<%s>",
					osco->osco_argument_usage);
			break;
		default:
			__builtin_unreachable();
		}

		if (!(osco->osco_flags & OS_SUBCOMMAND_OPTION_FLAG_ENUM)) {
			_stoupper(argbuff);
		}
	} else {
		snprintf(optbuff, sizeof(optbuff), "%s", osco->osco_argument_usage);
		if (!(osco->osco_flags & OS_SUBCOMMAND_OPTION_FLAG_ENUM)) {
			_stoupper(optbuff);
		}
	}

	ret = asprintf(&final, "%s%s", optbuff, argbuff);
	if (ret < 0) {
		os_assert_zero(ret);
	}

	return final;
}

static char *
_os_subcommand_copy_option_spec(const os_subcommand_t *osc,
		const os_subcommand_option_t *osco, os_subcommand_option_spec_fmt_t fmt)
{
	int ret = -1;
	char *spec = NULL;
	char *__os_free spec_old = NULL;

	switch (fmt) {
	case OS_SUBCOMMAND_OPTION_SPEC_SHORT:
		_os_subcommand_copy_option_spec_short(osc, osco);
		break;
	case OS_SUBCOMMAND_OPTION_SPEC_LONG:
		_os_subcommand_copy_option_spec_long(osc, osco);
		break;
	case OS_SUBCOMMAND_OPTION_SPEC_COMBINED:
		spec = _os_subcommand_copy_option_spec_long(osc, osco);
		if (osco->osco_option) {
			spec_old = spec;

			ret = asprintf(&spec, "-%c | %s", osco->osco_option->val, spec);
			if (ret < 0) {
				os_assert_zero(ret);
			}
		}

		break;
	default:
		__builtin_unreachable();
	}

	return spec;
}

static char *
_os_subcommand_copy_usage_line(const os_subcommand_t *osc)
{
	char *usage_line = NULL;
	size_t i = 0;
	const os_subcommand_option_t *osco_i = NULL;
	const char *optional_spec = "";
	char subcmd_name[64];
	int ret = -1;

	// The usage line does not enumerate all possible optional options, just the
	// required options. If there are optional options, then display that but
	// otherwise leave them to be described by more extensive usage information.
	if (osc->osc_optional) {
		optional_spec = " [options]";
	}

	if (osc == _main_cmd) {
		strlcpy(subcmd_name, "", sizeof(subcmd_name));
	} else {
		snprintf(subcmd_name, sizeof(subcmd_name), " %s", osc->osc_name);
	}

	ret = asprintf(&usage_line, "%s%s%s",
			getprogname(), subcmd_name, optional_spec);
	if (ret < 0) {
		os_assert_zero(ret);
	}

	i = 0;
	OS_SUBCOMMAND_OPTIONS_FOREACH(osco_i, osc, required, i) {
		char *__os_free usage_line_old = NULL;
		char *__os_free osco_spec = NULL;

		usage_line_old = usage_line;

		osco_spec = _os_subcommand_copy_option_spec_long(osc, osco_i);
		ret = asprintf(&usage_line, "%s %s", usage_line, osco_spec);
		if (ret < 0) {
			os_assert_zero(ret);
		}
	}

	i = 0;
	OS_SUBCOMMAND_OPTIONS_FOREACH(osco_i, osc, positional, i) {
		char *__os_free usage_line_old = NULL;
		char *__os_free osco_spec = NULL;
		const char *braces[] = {
			"<",
			">",
		};

		if (osco_i->osco_flags & OS_SUBCOMMAND_OPTION_FLAG_OPTIONAL_POS) {
			braces[0] = "[";
			braces[1] = "]";
		}

		usage_line_old = usage_line;

		osco_spec = _os_subcommand_copy_option_spec_long(osc, osco_i);
		ret = asprintf(&usage_line, "%s %s%s%s",
				usage_line, braces[0], osco_spec, braces[1]);
		if (ret < 0) {
			os_assert_zero(ret);
		}
	}

	if (osc == _main_cmd && osc != _internal_main_cmd) {
		// Always include the positional subcommand when printing usage for the
		// main subcommand. We do not expect it to be specified in a user-
		// provided main subcommand.
		const os_subcommand_option_t *subopt = &_main_positional[0];
		char *__os_free usage_line_old = NULL;
		char *__os_free osco_spec = NULL;

		usage_line_old = usage_line;

		osco_spec = _os_subcommand_copy_option_spec_long(osc, subopt);
		ret = asprintf(&usage_line, "%s <%s>", usage_line, osco_spec);
		if (ret < 0) {
			os_assert_zero(ret);
		}
	}

	return usage_line;
}

static void
_os_subcommand_print_option_usage(const os_subcommand_t *osc,
		const os_subcommand_option_t *osco, FILE *f)
{
	char *__os_free opt_spec = NULL;
	ssize_t initpad = -CTL_OUTPUT_OPTARG_INDENT;

	opt_spec = _os_subcommand_copy_option_spec(osc, osco,
			OS_SUBCOMMAND_OPTION_SPEC_COMBINED);
	fprintf(f, "    %-24s    ", opt_spec);

	// If the usage specifier is long, start the description on the next line.
	if (strlen(opt_spec) >= CTL_OUTPUT_OPTARG_OVERFLOW) {
		initpad = CTL_OUTPUT_OPTARG_INDENT;
		crfprintf_np(f, "");
	}

	wfprintf_np(f, initpad, CTL_OUTPUT_OPTARG_INDENT, _ttys->ts_cols, "%s",
			osco->osco_argument_human);
}

static void
_os_subcommand_print_help_line(const os_subcommand_t *osc, FILE *f)
{
	ssize_t initpad = -CTL_OUTPUT_OPTARG_INDENT;

	fprintf(f, "    %-24s    ", osc->osc_name);

	// If the usage specifier is long, start the description on the next line.
	if (strlen(osc->osc_name) >= CTL_OUTPUT_OPTARG_OVERFLOW) {
		initpad = CTL_OUTPUT_OPTARG_INDENT;
		crfprintf_np(f, "");
	}

	wfprintf_np(f, initpad, CTL_OUTPUT_OPTARG_INDENT, _ttys->ts_cols, "%s",
			osc->osc_desc);
}

static void
_os_subcommand_print_usage(const os_subcommand_t *osc, FILE *f)
{
	size_t i = 0;
	const os_subcommand_option_t *osco_i = NULL;
	char *__os_free usage_line = NULL;
	bool header_printed = false;

	usage_line = _os_subcommand_copy_usage_line(osc);

	wfprintf_np(f, 0, 4, _ttys->ts_cols, "USAGE:");
	crfprintf_np(f, "");
	wfprintf_np(f, 4, 4, _ttys->ts_cols, "%s", usage_line);

	if (osc->osc_long_desc) {
		// The long description gets printed in its own paragraph.
		_print_header(f, "DESCRIPTION", NULL);
		wfprintf_np(f, 4, 4, _ttys->ts_cols, "%s", osc->osc_long_desc);
	} else if (osc->osc_desc) {
		// The short description gets printed on the same line.
		crfprintf_np(f, "");
		wfprintf_np(f, 0, 4, _ttys->ts_cols, "DESCRIPTION: %s",
				osc->osc_desc);
	}

	if (osc->osc_required || osc->osc_positional || osc == _main_cmd) {
		i = 0;
		OS_SUBCOMMAND_OPTIONS_FOREACH(osco_i, osc, required, i) {
			_print_header(f, "REQUIRED", &header_printed);
			_os_subcommand_print_option_usage(osc, osco_i, f);
		}

		i = 0;
		OS_SUBCOMMAND_OPTIONS_FOREACH(osco_i, osc, positional, i) {
			_print_header(f, "REQUIRED", &header_printed);

			if (osco_i->osco_flags & OS_SUBCOMMAND_OPTION_FLAG_OPTIONAL_POS) {
				continue;
			}

			_os_subcommand_print_option_usage(osc, osco_i, f);
		}

		if (osc == _main_cmd && osc != _internal_main_cmd) {
			// We do not expect the user's main command to specify that a
			// subcommand must follow, so always defer to ours.
			_print_header(f, "REQUIRED", &header_printed);
			_os_subcommand_print_option_usage(osc, &_main_positional[0], f);
		}
	}

	header_printed = false;

	if (osc->osc_optional || osc->osc_positional) {
		i = 0;
		OS_SUBCOMMAND_OPTIONS_FOREACH(osco_i, osc, optional, i) {
			_print_header(f, "OPTIONAL", &header_printed);
			_os_subcommand_print_option_usage(osc, osco_i, f);
		}

		i = 0;
		OS_SUBCOMMAND_OPTIONS_FOREACH(osco_i, osc, positional, i) {
			if (osco_i->osco_flags & OS_SUBCOMMAND_OPTION_FLAG_OPTIONAL_POS) {
				_print_header(f, "OPTIONAL", &header_printed);
				_os_subcommand_print_option_usage(osc, osco_i, f);
			}
		}
	}
}

static const os_subcommand_t *
_os_subcommand_find(const char *name)
{
	const os_subcommand_t **oscip = NULL;

	if (!name) {
		return _main_cmd;
	}

	if (strcmp(_help_cmd->osc_name, name) == 0) {
		return &__help_cmd;
	}

	LINKER_SET_FOREACH(oscip, const os_subcommand_t **, SUBCOMMAND_LINKER_SET) {
		const os_subcommand_t *osci = *oscip;

		if (osci->osc_flags & OS_SUBCOMMAND_FLAG_MAIN) {
			// The main subcommand cannot be invoked directly.
			continue;
		}

		if (strcmp(osci->osc_name, name) == 0) {
			return osci;
		}
	}

	return NULL;
}

static int
_os_subcommand_be_helpful(const os_subcommand_t *osc,
		int argc, const char *argv[])
{
	int res = 0;

	if (osc->osc_flags & OS_SUBCOMMAND_FLAG_HELPFUL) {
		if (argc == 1) {
			_os_subcommand_print_usage(osc, stdout);
			res = 1;
			goto __out;
		}
	}

	if (osc->osc_flags & OS_SUBCOMMAND_FLAG_HELPFUL_FIRST_OPTION) {
		if (argc == 2 && (strcmp(argv[1], "help") == 0 ||
				strcmp(argv[1], "-h") == 0 ||
				strcmp(argv[1], "-help") == 0 ||
				strcmp(argv[1], "--help") == 0)) {
			_os_subcommand_print_usage(osc, stdout);
			res = 1;
			goto __out;
		}
	}

__out:
	return res;
}

static void
_print_subcommand_list(const os_subcommand_t *osc, FILE *f)
{
	const os_subcommand_t **oscip = NULL;
	bool header_printed = false;

	LINKER_SET_FOREACH(oscip, const os_subcommand_t **,
			SUBCOMMAND_LINKER_SET) {
		const os_subcommand_t *osci = *oscip;

		_print_header(f, "SUBCOMMANDS", &header_printed);

		if ((osci->osc_flags & OS_SUBCOMMAND_FLAG_MAIN) ||
				(osci->osc_flags & OS_SUBCOMMAND_FLAG_HIDDEN)) {
			continue;
		}

		_os_subcommand_print_help_line(osci, f);
	}

	// Print the help subcommand last.
	_os_subcommand_print_help_line(osc, f);
}

#pragma mark API
int
os_subcommand_main(int argc, const char *argv[],
		os_subcommand_main_flags_t flags)
{
	int xit = -1;
	const char *cmdname = NULL;
	const os_subcommand_t *osc = NULL;
	const os_subcommand_t **oscip = NULL;

	_init_column_count();

	// Find the main subcommand if any exists. Otherwise we'll just use our pre-
	// canned main subcommand.
	LINKER_SET_FOREACH(oscip, const os_subcommand_t **, SUBCOMMAND_LINKER_SET) {
		osc = *oscip;
		if (osc->osc_flags & OS_SUBCOMMAND_FLAG_MAIN) {
			_main_cmd = osc;
			break;
		}
	}

	osc = NULL;

	// See if we just need to print help for the main command.
	if (_os_subcommand_be_helpful(_main_cmd, argc, argv)) {
		_print_subcommand_list(_help_cmd, stdout);
		xit = 0;
		goto __out;
	}

	// Invoke the main subcommand to snarf any global options. Our default
	// implementation does nothing and just returns 0.
	xit = _main_cmd->osc_invoke(_main_cmd, argc, argv);
	if (xit) {
		goto __out;
	}

	// Advance argument pointer and make the subcommand argv[0].
	argc -= optind;
	argv += optind;
	cmdname = argv[0];

	if (argc < 1) {
		os_subcommand_fprintf(NULL, stderr, "please provide a subcommand");
		xit = EX_USAGE;
		goto __out;
	}

	osc = _os_subcommand_find(cmdname);
	if (osc) {
		if (osc->osc_flags & OS_SUBCOMMAND_FLAG_REQUIRE_ROOT) {
			if (geteuid()) {
				os_subcommand_fprintf(osc, stderr,
						"subcommand requires root: %s",
						cmdname);
				xit = EX_NOPERM;
				goto __out;
			}
		}

		if (osc->osc_flags & OS_SUBCOMMAND_FLAG_TTYONLY) {
			if (!isatty(STDOUT_FILENO) || !isatty(STDIN_FILENO)) {
				os_subcommand_fprintf(osc, stderr,
						"subcommand requires a tty: %s",
						cmdname);
				xit = EX_UNAVAILABLE;
				goto __out;
			}
		}

		if (_os_subcommand_be_helpful(osc, argc, argv)) {
			xit = 0;
			goto __out;
		}

		xit = osc->osc_invoke(osc, argc, argv);
	} else {
		os_subcommand_fprintf(NULL, stderr, "unknown subcommand: %s", cmdname);
		xit = EX_USAGE;
	}

__out:
	if (xit == EX_USAGE) {
		if (!osc) {
			// If we couldn't find the subcommand, then print the list of known
			// subcommands.
			_print_subcommand_list(_help_cmd, stderr);
		} else {
			_os_subcommand_print_usage(osc, stderr);
		}
	}

	return xit;
}

void
os_subcommand_fprintf(const os_subcommand_t *osc, FILE *f,
		const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vcrfprintf_np(f, fmt, ap);
	va_end(ap);
}

void
os_subcommand_vfprintf(const os_subcommand_t *osc, FILE *f,
		const char *fmt, va_list ap)
{
	if (!osc || (osc->osc_flags & OS_SUBCOMMAND_FLAG_MAIN)) {
		fprintf(f, "%s: ", getprogname());
	} else {
		fprintf(f, "%s-%s: ", getprogname(), osc->osc_name);
	}

	vcrfprintf_np(f, fmt, ap);
}
