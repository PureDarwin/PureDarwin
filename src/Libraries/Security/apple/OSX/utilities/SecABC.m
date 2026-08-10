//
//  SecABC.m
//  Security
//

#import "SecABC.h"
#import <os/log.h>


#if ABC_BUGCAPTURE
#import <SymptomDiagnosticReporter/SDRDiagnosticReporter.h>
#endif

void SecABCTrigger(CFStringRef type,
                   CFStringRef subtype,
                   CFStringRef subtypeContext,
                   CFDictionaryRef payload)
{
    [SecABC triggerAutoBugCaptureWithType:(__bridge NSString *)type
                                  subType:(__bridge NSString *)subtype
                           subtypeContext:(__bridge NSString *)subtypeContext
                                   events:nil
                                  payload:(__bridge NSDictionary *)payload
                          detectedProcess:nil];
}


@implementation SecABC

+ (void)triggerAutoBugCaptureWithType:(NSString *)type
                              subType:(NSString *)subType
{
    [self triggerAutoBugCaptureWithType: type
                                subType: subType
                         subtypeContext: nil
                                 events: nil
                                payload: nil
                        detectedProcess: nil];
}


+ (void)triggerAutoBugCaptureWithType:(NSString *)type
                              subType:(NSString *)subType
                       subtypeContext:(NSString *)subtypeContext
                               events:(NSArray *)events
                              payload:(NSDictionary *)payload
                      detectedProcess:(NSString *)process
{
#if ABC_BUGCAPTURE
    os_log(OS_LOG_DEFAULT, "TriggerABC for %{public}@/%{public}@/%{public}@",
           type, subType, subtypeContext);

    // no ABC on darwinos
    if ([SDRDiagnosticReporter class] == nil) {
        return;
    }

    SDRDiagnosticReporter *diagnosticReporter = [[SDRDiagnosticReporter alloc] init];
    NSMutableDictionary *signature = [diagnosticReporter signatureWithDomain:@"com.apple.security.keychain"
                                                                        type:type
                                                                     subType:subType
                                                              subtypeContext:subtypeContext
                                                             detectedProcess:process?:[[NSProcessInfo processInfo] processName]
                                                      triggerThresholdValues:nil];
    if (signature == NULL) {
        os_log(OS_LOG_DEFAULT, "TriggerABC signature generation failed");
        return;
    }

    (void)[diagnosticReporter snapshotWithSignature:signature
                                                   duration:5.0
                                                     events:events
                                                    payload:payload
                                                    actions:NULL
                                                      reply:^void(NSDictionary *response)
    {
        os_log(OS_LOG_DEFAULT, "Received response from Diagnostic Reporter - %{public}@/%{public}@/%{public}@: %{public}@",
               type, subType, subtypeContext, response);
    }];
#endif
}

@end
