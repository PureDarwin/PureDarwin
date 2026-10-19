#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include "common.h"

static const char *frameworks[] = { "AVFoundation", "MediaPlayer", "AuthenticationServices", "LocalAuthentication", "CoreLocation", "Vision",
                                    "IOBluetooth", "ServiceManagement", "Accessibility", "AddressBook", "CoreMIDI", "VideoToolbox",
                                    "AudioUnit", "CoreAudio", "ApplicationServices", "ImageIO" };
static const char *classes[] = { "AVAssetReader", "AVAssetReaderTrackOutput", "AVCaptureDevice", "AVCaptureDeviceDiscoverySession",
                                 "AVCaptureDeviceInput", "AVCaptureSession", "AVCaptureVideoDataOutput", "AVSampleBufferDisplayLayer",
                                 "AVSpeechSynthesisVoice", "AVSpeechSynthesizer", "AVSpeechUtterance", "AVURLAsset", "MPMediaItemArtwork",
                                 "MPNowPlayingInfoCenter", "MPRemoteCommandCenter", "ASAuthorizationController",
                                 "ASAuthorizationSingleSignOnCredential", "ASAuthorizationSingleSignOnProvider", "LAContext",
                                 "CLLocationManager", "VNImageRequestHandler", "VNRecognizeTextRequest", "IOBluetoothDevice" };

int main(void)
{
    @autoreleasepool {
        for (size_t i = 0; i < sizeof(frameworks) / sizeof(*frameworks); i++) {
            char path[256];
            snprintf(path, sizeof(path), "/System/Library/Frameworks/%s.framework/%s", frameworks[i], frameworks[i]);
            void *h = dlopen(path, RTLD_NOW);
            CHECK(h != NULL, "dlopen %s%s%s", frameworks[i], h ? "" : ": ", h ? "" : dlerror());
        }
        for (size_t i = 0; i < sizeof(classes) / sizeof(*classes); i++)
            CHECK(NSClassFromString([NSString stringWithUTF8String:classes[i]]) != nil, "class %s", classes[i]);

        Class dev = NSClassFromString(@"AVCaptureDevice");
        CHECK([[dev performSelector:@selector(devicesWithMediaType:) withObject:@"vide"] count] == 0, "no cameras");
        Class disc = NSClassFromString(@"AVCaptureDeviceDiscoverySession");
        id session = [disc discoverySessionWithDeviceTypes:@[ @"AVCaptureDeviceTypeExternalUnknown" ] mediaType:@"vide" position:0];
        CHECK(session && [[session devices] count] == 0, "discovery session finds nothing");
        NSError *err = nil;
        id input = [NSClassFromString(@"AVCaptureDeviceInput") deviceInputWithDevice:nil error:&err];
        CHECK(input == nil && err != nil, "device input fails with %s", [[err domain] UTF8String]);
        CHECK([[NSClassFromString(@"AVSpeechSynthesisVoice") speechVoices] count] == 0, "no voices");
        id asset = [NSClassFromString(@"AVURLAsset") URLAssetWithURL:[NSURL fileURLWithPath:@"/tmp/x.mp4"] options:nil];
        err = nil;
        id reader = [NSClassFromString(@"AVAssetReader") assetReaderWithAsset:asset error:&err];
        CHECK(reader == nil && err != nil, "asset reader fails with an error");
        id layer = [[NSClassFromString(@"AVSampleBufferDisplayLayer") alloc] init];
        CHECK(layer && [layer isReadyForMoreMediaData], "sample buffer display layer");
        [layer release];

        id center = [NSClassFromString(@"MPNowPlayingInfoCenter") defaultCenter];
        [center setNowPlayingInfo:@{ @"title" : @"t" }];
        CHECK([[[center nowPlayingInfo] objectForKey:@"title"] isEqual:@"t"], "now playing info");
        id cmd = [[NSClassFromString(@"MPRemoteCommandCenter") sharedCommandCenter] playCommand];
        CHECK(cmd && [cmd addTargetWithHandler:^NSInteger(id e) { return 0; }], "remote command target");

        id ctx = [[NSClassFromString(@"LAContext") alloc] init];
        err = nil;
        CHECK(![ctx canEvaluatePolicy:2 error:&err] && err, "LAContext cannot evaluate (%ld)", (long)[err code]);
        [ctx release];
        id provider = [NSClassFromString(@"ASAuthorizationSingleSignOnProvider") authorizationProviderWithIdentityProviderURL:
                                                                                   [NSURL URLWithString:@"https://example.org"]];
        CHECK(provider && ![provider canPerformAuthorization], "SSO provider cannot authorize");
        CHECK([NSClassFromString(@"CLLocationManager") authorizationStatus] == 1, "location restricted");
        id handler = [[NSClassFromString(@"VNImageRequestHandler") alloc] initWithCVPixelBuffer:NULL options:@{}];
        id req = [[NSClassFromString(@"VNRecognizeTextRequest") alloc] initWithCompletionHandler:nil];
        err = nil;
        CHECK(![handler performRequests:@[ req ] error:&err] && err, "Vision request fails with an error");
        CHECK([[NSClassFromString(@"IOBluetoothDevice") pairedDevices] count] == 0, "no Bluetooth devices");

        Boolean (*bless)(CFStringRef, CFStringRef, void *, CFErrorRef *) = dlsym(RTLD_DEFAULT, "SMJobBless");
        CFErrorRef cferr = NULL;
        CHECK(bless && !bless(CFSTR("kSMDomainSystemLaunchd"), CFSTR("x"), NULL, &cferr) && cferr, "SMJobBless fails with an error");
        Boolean (*nonblink)(void) = dlsym(RTLD_DEFAULT, "AXPrefersNonBlinkingTextInsertionIndicator");
        CHECK(nonblink && !nonblink(), "non-blinking caret preference off");
    }
    DONE("ObjC frameworks");
}
