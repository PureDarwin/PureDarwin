#ifndef _WIRELESS_DIAGNOSTICS_H_
#define _WIRELESS_DIAGNOSTICS_H_

#import <Foundation/Foundation.h>

@class AWDMetricContainer;

/*
 * A metric ready to submit. The payload is a ProtocolBuffer message (a PBCodable
 * subclass) generated from the component's .proto - AWDIPConfigurationIPv6Report
 * in bootp's case.
 */
__attribute__((weak_import))
@interface AWDMetricContainer : NSObject

- (void)setMetric:(id)metric;

@end

/*
 * Per-component connection to the diagnostics service. The component id
 * identifies the reporting subsystem (bootp's IPConfiguration is 0x67) and the
 * metric identifier the particular report within it.
 */
__attribute__((weak_import))
@interface AWDServerConnection : NSObject

- (instancetype)initWithComponentId:(uint32_t)componentId;
- (AWDMetricContainer *)newMetricContainerWithIdentifier:(uint32_t)identifier;
- (BOOL)submitMetric:(AWDMetricContainer *)container;

@end

#endif /* _WIRELESS_DIAGNOSTICS_H_ */
