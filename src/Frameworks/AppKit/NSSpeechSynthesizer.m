// NSSpeechSynthesizer.m - text to speech
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSSpeechSynthesizer.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>

NSVoiceAttributeKey const NSVoiceName = @"VoiceName";
NSVoiceAttributeKey const NSVoiceIdentifier = @"VoiceIdentifier";
NSVoiceAttributeKey const NSVoiceAge = @"VoiceAge";
NSVoiceAttributeKey const NSVoiceGender = @"VoiceGender";
NSVoiceAttributeKey const NSVoiceDemoText = @"VoiceDemoText";
NSVoiceAttributeKey const NSVoiceLocaleIdentifier = @"VoiceLocaleIdentifier";
NSVoiceAttributeKey const NSVoiceSupportedCharacters = @"VoiceSupportedCharacters";
NSVoiceAttributeKey const NSVoiceIndividuallySpokenCharacters = @"VoiceIndividuallySpokenCharacters";
NSVoiceAttributeKey const NSVoiceLanguage = @"VoiceLanguage";
NSString * const NSVoiceGenderNeuter = @"VoiceGenderNeuter";
NSString * const NSVoiceGenderMale = @"VoiceGenderMale";
NSString * const NSVoiceGenderFemale = @"VoiceGenderFemale";

@implementation NSSpeechSynthesizer

- (instancetype)init {
    return [self initWithVoice:nil];
}

// nil is the default voice, and any named voice is one this system does not have
- (instancetype)initWithVoice:(NSSpeechSynthesizerVoiceName)voice {
    if (voice != nil) {
        [self release];
        return nil;
    }
    if ((self = [super init]) == nil)
        return nil;
    _rate = 175.0f;
    _volume = 1.0f;
    return self;
}

- (void)dealloc {
    [_voice release];
    [super dealloc];
}

- (BOOL)startSpeakingString:(NSString *)string {
    return NO;
}

- (BOOL)startSpeakingString:(NSString *)string toURL:(NSURL *)url {
    return NO;
}

- (void)stopSpeaking {
}

- (void)pauseSpeakingAtBoundary:(NSUInteger)boundary {
}

- (void)continueSpeaking {
}

- (BOOL)isSpeaking {
    return NO;
}

- (id <NSSpeechSynthesizerDelegate>)delegate {
    return _delegate;
}

- (void)setDelegate:(id <NSSpeechSynthesizerDelegate>)delegate {
    _delegate = delegate;
}

- (NSSpeechSynthesizerVoiceName)voice {
    return _voice;
}

- (BOOL)setVoice:(NSSpeechSynthesizerVoiceName)voice {
    return voice == nil;
}

- (float)rate {
    return _rate;
}

- (void)setRate:(float)rate {
    _rate = rate;
}

- (float)volume {
    return _volume;
}

- (void)setVolume:(float)volume {
    _volume = volume;
}

- (BOOL)usesFeedbackWindow {
    return NO;
}

- (void)setUsesFeedbackWindow:(BOOL)flag {
}

+ (BOOL)isAnyApplicationSpeaking {
    return NO;
}

+ (NSSpeechSynthesizerVoiceName)defaultVoice {
    return nil;
}

+ (NSArray *)availableVoices {
    return [NSArray array];
}

+ (NSDictionary *)attributesForVoice:(NSSpeechSynthesizerVoiceName)voice {
    return nil;
}

@end
