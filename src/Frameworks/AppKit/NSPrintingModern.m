// NSPrintingModern.m - the PrintCore side of the printing classes
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

// PrintCore lives in ApplicationServices, which is built after AppKit,
// so its functions are looked up when first needed
#import <AppKit/NSPrintInfo.h>
#import <AppKit/NSPrinter.h>
#import <AppKit/NSPrintPanel.h>
#import <AppKit/NSPageLayout.h>
#import <AppKit/NSPrintOperation.h>
#import <AppKit/NSWindow.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSString.h>
#import <objc/runtime.h>
#import <objc/message.h>
#include <dlfcn.h>

typedef CFMutableDictionaryRef PDPMRef;
typedef int32_t PDPMStatus;
typedef struct { double top, left, bottom, right; } PDPMRect;

enum { kPDPMPortrait = 1, kPDPMLandscape = 2 };

static void *printCoreSymbol(const char *name)
{
    static void *handle;

    if (handle == NULL)
        handle = dlopen("/System/Library/Frameworks/ApplicationServices.framework/ApplicationServices", RTLD_LAZY);
    return handle ? dlsym(handle, name) : NULL;
}

static PDPMRef createPrintCoreObject(const char *constructor)
{
    PDPMStatus (*create)(PDPMRef *) = printCoreSymbol(constructor);
    PDPMRef object = NULL;

    if (create == NULL || create(&object) != 0)
        return NULL;
    return object;
}

static char kSessionKey, kSettingsKey, kFormatKey, kPrintSettingsKey;

@implementation NSPrintInfo (PDPrintCore)

-(void *)_pmObjectForKey:(void *)key constructor:(const char *)constructor {
    id object = objc_getAssociatedObject(self, key);

    if (object == nil) {
        PDPMRef ref = createPrintCoreObject(constructor);

        if (ref == NULL)
            return NULL;
        object = (id)ref;
        objc_setAssociatedObject(self, key, object, OBJC_ASSOCIATION_RETAIN);
        CFRelease(ref);
        if (key == &kSettingsKey)
            [self _pushToPMPrintSettings];
        else if (key == &kFormatKey)
            [self _pushToPMPageFormat];
    }
    return (void *)object;
}

-(void *)PMPrintSession {
    return [self _pmObjectForKey:&kSessionKey constructor:"PMCreateSession"];
}

-(void *)PMPrintSettings {
    return [self _pmObjectForKey:&kSettingsKey constructor:"PMCreatePrintSettings"];
}

-(void *)PMPageFormat {
    return [self _pmObjectForKey:&kFormatKey constructor:"PMCreatePageFormat"];
}

-(void)_pushToPMPrintSettings {
    PDPMRef settings = (PDPMRef)objc_getAssociatedObject(self, &kSettingsKey);
    PDPMStatus (*setCopies)(PDPMRef, UInt32, Boolean) = printCoreSymbol("PMSetCopies");
    PDPMStatus (*setFirst)(PDPMRef, UInt32, Boolean) = printCoreSymbol("PMSetFirstPage");
    PDPMStatus (*setLast)(PDPMRef, UInt32, Boolean) = printCoreSymbol("PMSetLastPage");
    NSDictionary *attributes = [self dictionary];
    id value;

    if (settings == NULL)
        return;
    if (setCopies && (value = [attributes objectForKey:NSPrintCopies]) != nil)
        setCopies(settings, [value unsignedIntValue], false);
    if ([[attributes objectForKey:NSPrintAllPages] boolValue])
        return;
    if (setFirst && (value = [attributes objectForKey:NSPrintFirstPage]) != nil)
        setFirst(settings, [value unsignedIntValue], false);
    if (setLast && (value = [attributes objectForKey:NSPrintLastPage]) != nil)
        setLast(settings, [value unsignedIntValue], false);
}

-(void)_pushToPMPageFormat {
    PDPMRef format = (PDPMRef)objc_getAssociatedObject(self, &kFormatKey);
    PDPMStatus (*setOrientation)(PDPMRef, UInt16, Boolean) = printCoreSymbol("PMSetOrientation");

    if (format == NULL || setOrientation == NULL)
        return;
    setOrientation(format, [self orientation] == NSLandscapeOrientation ? kPDPMLandscape : kPDPMPortrait, false);
}

-(void)updateFromPMPrintSettings {
    PDPMRef settings = (PDPMRef)objc_getAssociatedObject(self, &kSettingsKey);
    PDPMStatus (*getCopies)(PDPMRef, UInt32 *) = printCoreSymbol("PMGetCopies");
    PDPMStatus (*getFirst)(PDPMRef, UInt32 *) = printCoreSymbol("PMGetFirstPage");
    PDPMStatus (*getLast)(PDPMRef, UInt32 *) = printCoreSymbol("PMGetLastPage");
    NSMutableDictionary *attributes = [self dictionary];
    UInt32 copies, first, last;

    if (settings == NULL)
        return;
    if (getCopies && getCopies(settings, &copies) == 0)
        [attributes setObject:[NSNumber numberWithUnsignedInt:copies] forKey:NSPrintCopies];
    if (getFirst && getLast && getFirst(settings, &first) == 0 && getLast(settings, &last) == 0) {
        BOOL all = (first <= 1 && last == (UInt32)-1);

        [attributes setObject:[NSNumber numberWithBool:all] forKey:NSPrintAllPages];
        [attributes setObject:[NSNumber numberWithUnsignedInt:first] forKey:NSPrintFirstPage];
        [attributes setObject:[NSNumber numberWithUnsignedInt:last] forKey:NSPrintLastPage];
    }
}

-(void)updateFromPMPageFormat {
    PDPMRef format = (PDPMRef)objc_getAssociatedObject(self, &kFormatKey);
    PDPMStatus (*getOrientation)(PDPMRef, UInt16 *) = printCoreSymbol("PMGetOrientation");
    PDPMStatus (*getPaper)(PDPMRef, PDPMRect *) = printCoreSymbol("PMGetUnadjustedPaperRect");
    UInt16 orientation;
    PDPMRect paper;

    if (format == NULL)
        return;
    if (getOrientation && getOrientation(format, &orientation) == 0)
        [self setOrientation:(orientation == kPDPMLandscape) ? NSLandscapeOrientation : NSPortraitOrientation];
    if (getPaper && getPaper(format, &paper) == 0)
        [self setPaperSize:NSMakeSize(paper.right - paper.left, paper.bottom - paper.top)];
}

// the PMPrintSettings keys apps set directly, kept beside the attributes
-(NSMutableDictionary *)printSettings {
    NSMutableDictionary *settings = objc_getAssociatedObject(self, &kPrintSettingsKey);

    if (settings == nil) {
        settings = [NSMutableDictionary dictionary];
        objc_setAssociatedObject(self, &kPrintSettingsKey, settings, OBJC_ASSOCIATION_RETAIN);
    }
    return settings;
}

-(CGFloat)scalingFactor {
    id value = [[self dictionary] objectForKey:@"NSScalingFactor"];

    return value ? [value doubleValue] : 1.0;
}

-(void)setScalingFactor:(CGFloat)value {
    [[self dictionary] setObject:[NSNumber numberWithDouble:value] forKey:@"NSScalingFactor"];
}

-(BOOL)isSelectionOnly {
    return [[[self dictionary] objectForKey:@"NSPrintSelectionOnly"] boolValue];
}

-(void)setSelectionOnly:(BOOL)value {
    [[self dictionary] setObject:[NSNumber numberWithBool:value] forKey:@"NSPrintSelectionOnly"];
}

@end

// no print system, so there are no printers to name
@implementation NSPrinter (PDPrinters)

+(NSArray *)printerNames {
    return [NSArray array];
}

+(NSArray *)printerTypes {
    return [NSArray array];
}

+(NSPrinter *)printerWithName:(NSString *)name {
    return nil;
}

+(NSPrinter *)printerWithType:(NSString *)type {
    return nil;
}

-(NSString *)name {
    return @"";
}

-(NSString *)type {
    return @"";
}

@end

static char kAccessoriesKey;

static NSArray *accessoryControllers(id panel)
{
    return objc_getAssociatedObject(panel, &kAccessoriesKey) ?: [NSArray array];
}

static void addAccessoryController(id panel, id controller)
{
    NSArray *list = [accessoryControllers(panel) arrayByAddingObject:controller];

    objc_setAssociatedObject(panel, &kAccessoriesKey, list, OBJC_ASSOCIATION_RETAIN);
}

// a sheet runs the modal panel and reports back after the current event, as the sheet would
static void finishSheet(id panel, NSInteger result, id delegate, SEL selector, void *contextInfo)
{
    if (delegate == nil || selector == NULL)
        return;
    ((void (*)(id, SEL, id, NSInteger, void *))objc_msgSend)(delegate, selector, panel, result, contextInfo);
}

@implementation NSPrintPanel (PDModern)

-(void)addAccessoryController:(id)controller {
    if (controller != nil)
        addAccessoryController(self, controller);
}

-(void)removeAccessoryController:(id)controller {
    NSMutableArray *list = [[accessoryControllers(self) mutableCopy] autorelease];

    [list removeObjectIdenticalTo:controller];
    objc_setAssociatedObject(self, &kAccessoriesKey, list, OBJC_ASSOCIATION_RETAIN);
}

-(NSArray *)accessoryControllers {
    return accessoryControllers(self);
}

-(NSInteger)runModalWithPrintInfo:(NSPrintInfo *)printInfo {
    return [self runModal];
}

-(void)beginSheetWithPrintInfo:(NSPrintInfo *)printInfo modalForWindow:(NSWindow *)window delegate:delegate didEndSelector:(SEL)selector contextInfo:(void *)contextInfo {
    finishSheet(self, [self runModalWithPrintInfo:printInfo], delegate, selector, contextInfo);
}

@end

@implementation NSPageLayout (PDModern)

-(void)addAccessoryController:(id)controller {
    if (controller != nil)
        addAccessoryController(self, controller);
}

-(NSArray *)accessoryControllers {
    return accessoryControllers(self);
}

-(void)beginSheetWithPrintInfo:(NSPrintInfo *)printInfo modalForWindow:(NSWindow *)window delegate:delegate didEndSelector:(SEL)selector contextInfo:(void *)contextInfo {
    finishSheet(self, [self runModalWithPrintInfo:printInfo], delegate, selector, contextInfo);
}

@end
