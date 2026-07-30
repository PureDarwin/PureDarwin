/*
 * IPMonitor's AWD (Apple Wireless Diagnostics) telemetry.
 *
 * The real implementation, IPMonitorAWDReport.m, submits interface-advisory
 * metrics through <WirelessDiagnostics/WirelessDiagnostics.h> - a closed-source
 * framework with no counterpart here. There is no diagnostics service to submit
 * to, so these do nothing.
 *
 * Unlike most gaps this one carries no correctness risk: the report is
 * write-only telemetry. IPMonitorControlServer.c calls these only from
 * SubmitInterfaceAdvisoryMetric(), whose result nothing reads, and the advisory
 * state itself still lands in the dynamic store through the normal path.
 *
 * A NULL report is returned rather than a dummy object so the callers' own NULL
 * checks short-circuit the rest of the submission.
 */

#include <CoreFoundation/CoreFoundation.h>
#include "IPMonitorAWDReport.h"

InterfaceAdvisoryReportRef
InterfaceAdvisoryReportCreate(AWDIPMonitorInterfaceType type)
{
	(void)type;
	return NULL;
}

void
InterfaceAdvisoryReportSubmit(InterfaceAdvisoryReportRef report)
{
	(void)report;
}

void
InterfaceAdvisoryReportSetFlags(InterfaceAdvisoryReportRef report,
				AWDIPMonitorInterfaceAdvisoryReport_Flags flags)
{
	(void)report;
	(void)flags;
}

void
InterfaceAdvisoryReportSetAdvisoryCount(InterfaceAdvisoryReportRef report,
					uint32_t count)
{
	(void)report;
	(void)count;
}
