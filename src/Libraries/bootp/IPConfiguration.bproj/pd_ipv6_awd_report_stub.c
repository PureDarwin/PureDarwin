/*
 * IPConfiguration's AWD (Apple Wireless Diagnostics) telemetry.
 *
 * The real implementation, IPv6AWDReport.m, builds protobuf metric objects and
 * submits them through <WirelessDiagnostics/WirelessDiagnostics.h>. There is no
 * diagnostics service here to submit to, so these do nothing - the same
 * treatment SystemConfiguration's pd_awd_report_stub.c gives IPMonitor's.
 *
 * This carries no correctness risk: the reports are write-only telemetry.
 * rtadv.c gathers one while processing router advertisements and submits it;
 * nothing reads the result, and every piece of state it describes is already
 * acted on through the normal code path.
 *
 * Create returns NULL so the callers' own NULL checks short-circuit the rest of
 * the report, exactly as the IPMonitor stub does.
 */

#include <CoreFoundation/CoreFoundation.h>
#include "IPv6AWDReport.h"

IPv6AWDReportRef
IPv6AWDReportCreate(InterfaceType type)
{
    (void)type;
    return NULL;
}

void
IPv6AWDReportSubmit(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetAPNName(IPv6AWDReportRef report, CFStringRef apn_name)
{
    (void)report;
    (void)apn_name;
}

void
IPv6AWDReportSetLinkLocalAddressDuplicated(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetAutoconfAddressDuplicated(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetAutoconfAddressDeprecated(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetAutoconfAddressDetached(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetAutoconfAddressAcquired(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetAutoconfRestarted(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetAutoconfRDNSS(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetAutoconfDNSSL(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetDHCPv6AddressAcquired(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetDHCPv6DNSServers(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetDHCPv6DNSDomainList(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetManualAddressConfigured(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetXLAT464Enabled(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetXLAT464PLATDiscoveryFailed(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetPrefixPreferredLifetime(IPv6AWDReportRef report, uint32_t lifetime)
{
    (void)report;
    (void)lifetime;
}

void
IPv6AWDReportSetPrefixValidLifetime(IPv6AWDReportRef report, uint32_t lifetime)
{
    (void)report;
    (void)lifetime;
}

void
IPv6AWDReportSetPrefixLifetimeNotInfinite(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetRouterLifetime(IPv6AWDReportRef report, uint16_t lifetime)
{
    (void)report;
    (void)lifetime;
}

void
IPv6AWDReportSetRouterLifetimeNotMaximum(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetRouterSourceAddressCollision(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetRouterLifetimeZero(IPv6AWDReportRef report)
{
    (void)report;
}

void
IPv6AWDReportSetDefaultRouterCount(IPv6AWDReportRef report, UInt32 count)
{
    (void)report;
    (void)count;
}

void
IPv6AWDReportSetExpiredDefaultRouterCount(IPv6AWDReportRef report, UInt32 count)
{
    (void)report;
    (void)count;
}

void
IPv6AWDReportSetPrefixCount(IPv6AWDReportRef report, UInt32 count)
{
    (void)report;
    (void)count;
}

void
IPv6AWDReportSetExpiredPrefixCount(IPv6AWDReportRef report, UInt32 count)
{
    (void)report;
    (void)count;
}

void
IPv6AWDReportSetRouterSolicitationCount(IPv6AWDReportRef report, UInt32 count)
{
    (void)report;
    (void)count;
}

void
IPv6AWDReportSetControlQueueUnsentCount(IPv6AWDReportRef report, UInt32 count)
{
    (void)report;
    (void)count;
}

void
IPv6AWDReportSetAutoconfAddressAcquisitionSeconds(IPv6AWDReportRef report, UInt32 seconds)
{
    (void)report;
    (void)seconds;
}

void
IPv6AWDReportSetDHCPv6AddressAcquisitionSeconds(IPv6AWDReportRef report, UInt32 seconds)
{
    (void)report;
    (void)seconds;
}

void
IPv6AWDReportSetDNSConfigurationAcquisitionSeconds(IPv6AWDReportRef report, UInt32 seconds)
{
    (void)report;
    (void)seconds;
}
