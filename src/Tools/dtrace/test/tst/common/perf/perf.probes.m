/*
 * Measures the number of probes, fbt probes, fbt modules
 * and the number of different USDT providers
 */
#include <darwintest.h>
#include <darwintest_perf.h>
#include <darwintest_utils.h>

#include <perfdata/perfdata.h>

#include <dtrace.h>

#include <unistd.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/sysctl.h>

#import <Foundation/Foundation.h>


static dtrace_hdl_t *g_dtp;

static int
check_usdt_enabled(void)
{
	int err;
	int dof_mode;
	size_t dof_mode_size = sizeof(dof_mode);
	err = sysctlbyname("kern.dtrace.dof_mode", &dof_mode, &dof_mode_size, NULL, 0);
	if (!err && dof_mode == 0) {
		T_LOG("usdt probes are not enabled on this system");
		return 0;
	}
	else if (err) {
		T_LOG("could not figure out whether usdt probes are enabled");
		return 0;
	}
	return 1;
}

static void
dtrace_exit(void)
{
	dtrace_close(g_dtp);
}

static void
dtrace_init(void)
{
	int err;
	T_SETUPBEGIN;
	if ((g_dtp = dtrace_open(DTRACE_VERSION, 0, &err)) == NULL) {
		T_ASSERT_FAIL("failed to initialize dtrace");
	}
	T_SETUPEND;
	T_ATEND(dtrace_exit);
}

static int
list_probe(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp, void (^cb)(const dtrace_probedesc_t *cb))
{
#pragma unused(dtp)
	cb(pdp);
	return 0;
}

static void
list_statement(dtrace_hdl_t *dtp, dtrace_prog_t *pgp, dtrace_stmtdesc_t *stp, void (^cb)(const dtrace_probedesc_t *cb))
{
#pragma unused(pgp)
	dtrace_ecbdesc_t *edp = stp->dtsd_ecbdesc;
	T_ASSERT_EQ(dtrace_probe_iter(dtp, &edp->dted_probe, (dtrace_probe_f*)list_probe, cb), 0, "dtrace_probe_iter");
}

static int
list_probes(const char *probedef, void (^cb)(const dtrace_probedesc_t *cb))
{
	dtrace_prog_t *prog;

	T_QUIET;
	T_ASSERT_NOTNULL(g_dtp, "dtrace handle");

	prog = dtrace_program_strcompile(g_dtp, probedef, DTRACE_PROBESPEC_NAME, 0, 0, NULL);
	T_ASSERT_NOTNULL(prog, "dtrace_program_strcompile");

	(void) dtrace_stmt_iter(g_dtp, prog,
				(dtrace_stmt_f *)list_statement, cb);
}

T_DECL(dtrace_fbt_probes, "measures the count of fbt probes and modules on the system", T_META_CHECK_LEAKS(false), T_META_BOOTARGS_SET("-unsafe_kernel_text"))
{
	__block int fbt_entry = 0, fbt_return = 0;
	int usdt_enabled = check_usdt_enabled();

	dtrace_init();

	NSMutableSet *modules = [NSMutableSet setWithCapacity:0];

	list_probes("fbt:::entry", ^(const dtrace_probedesc_t *pdp) {
		T_QUIET;
		T_ASSERT_NOTNULL(pdp, "dtrace_probedesc_t on fbt entry");

		[modules addObject: @(pdp->dtpd_mod)];

		fbt_entry++;
	});
	list_probes("fbt:::return", ^(const dtrace_probedesc_t *pdp) {
		T_QUIET;
		T_ASSERT_NOTNULL(pdp, "dtrace_probedesc_t on fbt return");

		fbt_return++;
	});

	T_ASSERT_GT([modules count], 1, "FBT probes have more than one module");

	char filename[MAXPATHLEN] = "dtrace.probes.fbt" PD_FILE_EXT;
	dt_resultfile(filename, sizeof(filename));
	T_LOG("perfdata file: %s", filename);
	pdwriter_t wr = pdwriter_open(filename, "dtrace.probes.fbt", 1, 0);
	T_WITH_ERRNO; T_ASSERT_NOTNULL(wr, "pdwriter_open %s", filename);

	pdwriter_new_value(wr, "entry", PDUNIT_CUSTOM(probes), fbt_entry);
	pdwriter_record_variable(wr, "usdt_enabled", usdt_enabled);
	pdwriter_new_value(wr, "return", PDUNIT_CUSTOM(probes), fbt_return);
	pdwriter_record_variable(wr, "usdt_enabled", usdt_enabled);
	pdwriter_new_value(wr, "modules", PDUNIT_CUSTOM(modules), [modules count]);
	pdwriter_record_variable(wr, "usdt_enabled", usdt_enabled);
	pdwriter_new_value(wr, "return_gap", PDUNIT_CUSTOM(probes), fbt_entry - fbt_return);
	pdwriter_record_variable(wr, "usdt_enabled", usdt_enabled);
	pdwriter_new_value(wr, "have_return_percent", PDUNIT_CUSTOM(percentage), ((double)fbt_return / fbt_entry) * 100);
	pdwriter_record_variable(wr, "usdt_enabled", usdt_enabled);

	pdwriter_close(wr);
}

T_DECL(dtrace_probes, "measures the count of probes and providers", T_META_CHECK_LEAKS(false), T_META_BOOTARGS_SET("-unsafe_kernel_text"))
{
	int usdt_enabled = check_usdt_enabled();
	dtrace_init();

	NSMutableSet *fasttrap_providers = [NSMutableSet setWithCapacity:0];
	NSMutableSet *providers = [NSMutableSet setWithCapacity:0];

	__block int probes = 0;

	list_probes(":::", ^(const dtrace_probedesc_t *pdp) {
		T_QUIET;
		T_ASSERT_NOTNULL(pdp, "dtrace_probedesc_t on :::");
		NSString *provider = @(pdp->dtpd_provider);

		/*
		 * Fasttrap providers have a digit in their name
		 */
		if (strspn(pdp->dtpd_provider, "0123456789")) {
			[fasttrap_providers addObject:provider];
		} else {
			[providers addObject:provider];
		}

		probes++;
	});

	char filename[MAXPATHLEN] = "dtrace.probes." PD_FILE_EXT;
	dt_resultfile(filename, sizeof(filename));
	T_LOG("perfdata file: %s\n", filename);
	pdwriter_t wr = pdwriter_open(filename, "dtrace.probes", 1, 0);
	T_WITH_ERRNO; T_ASSERT_NOTNULL(wr, "pdwriter_open %s", filename);

	pdwriter_new_value(wr, "probes", PDUNIT_CUSTOM(probes), probes);
	pdwriter_record_variable(wr, "usdt_enabled", usdt_enabled);
	pdwriter_new_value(wr, "providers", PDUNIT_CUSTOM(providers), [providers count]);
	pdwriter_record_variable(wr, "usdt_enabled", usdt_enabled);
	pdwriter_new_value(wr, "fasttrap_providers", PDUNIT_CUSTOM(providers), [fasttrap_providers count]);
	pdwriter_record_variable(wr, "usdt_enabled", usdt_enabled);

	pdwriter_close(wr);
}
