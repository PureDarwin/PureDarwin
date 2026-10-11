// NSSpeechSynthesizer.h - text to speech
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/NSObject.h>
#import <AppKit/AppKitExport.h>

@class NSString, NSArray, NSDictionary, NSURL, NSError;

typedef NSString *NSSpeechSynthesizerVoiceName;
typedef NSString *NSVoiceAttributeKey;

APPKIT_EXPORT NSVoiceAttributeKey const NSVoiceName;
APPKIT_EXPORT NSVoiceAttributeKey const NSVoiceIdentifier;
APPKIT_EXPORT NSVoiceAttributeKey const NSVoiceAge;
APPKIT_EXPORT NSVoiceAttributeKey const NSVoiceGender;
APPKIT_EXPORT NSVoiceAttributeKey const NSVoiceDemoText;
APPKIT_EXPORT NSVoiceAttributeKey const NSVoiceLocaleIdentifier;
APPKIT_EXPORT NSVoiceAttributeKey const NSVoiceSupportedCharacters;
APPKIT_EXPORT NSVoiceAttributeKey const NSVoiceIndividuallySpokenCharacters;
APPKIT_EXPORT NSVoiceAttributeKey const NSVoiceLanguage;
APPKIT_EXPORT NSString * const NSVoiceGenderNeuter;
APPKIT_EXPORT NSString * const NSVoiceGenderMale;
APPKIT_EXPORT NSString * const NSVoiceGenderFemale;

@protocol NSSpeechSynthesizerDelegate;

// PureDarwin installs no synthesis voices: none are available, the default synthesizer exists but
// cannot speak, and asking for a named voice gives nil as it does on macOS for a voice not installed
@interface NSSpeechSynthesizer : NSObject {
    NSString *_voice;
    id _delegate;
    float _rate, _volume;
}

- (instancetype)initWithVoice:(NSSpeechSynthesizerVoiceName)voice;

- (BOOL)startSpeakingString:(NSString *)string;
- (BOOL)startSpeakingString:(NSString *)string toURL:(NSURL *)url;
- (void)stopSpeaking;
- (void)pauseSpeakingAtBoundary:(NSUInteger)boundary;
- (void)continueSpeaking;

@property (getter=isSpeaking, readonly) BOOL speaking;
@property (assign) id <NSSpeechSynthesizerDelegate> delegate;
- (NSSpeechSynthesizerVoiceName)voice;
- (BOOL)setVoice:(NSSpeechSynthesizerVoiceName)voice;
@property float rate;
@property float volume;
@property BOOL usesFeedbackWindow;

+ (BOOL)isAnyApplicationSpeaking;
+ (NSSpeechSynthesizerVoiceName)defaultVoice;
+ (NSArray *)availableVoices;
+ (NSDictionary *)attributesForVoice:(NSSpeechSynthesizerVoiceName)voice;

@end

@protocol NSSpeechSynthesizerDelegate <NSObject>
@optional
- (void)speechSynthesizer:(NSSpeechSynthesizer *)sender didFinishSpeaking:(BOOL)finishedSpeaking;
@end
