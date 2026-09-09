/* Copyright (c) 2006-2007 Christopher J. W. Lloyd <cjwl@objc.net>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <AppKit/NSWorkspace.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSColor.h>
#include <CoreServices/LaunchServices.h>
#import <Foundation/NSFileManager.h>
#import <Foundation/NSUserDefaults.h>
#import <Foundation/NSBundle.h>
#import <Foundation/NSProcessInfo.h>
#import <Foundation/NSDistributedNotificationCenter.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSValue.h>
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#import <AppKit/NSRaise.h>

NSString * const NSWorkspaceWillPowerOffNotification=@"NSWorkspaceWillPowerOffNotification";

NSString * const NSWorkspaceDidMountNotification=@"NSWorkspaceDidMountNotification";
NSString * const NSWorkspaceWillUnmountNotification=@"NSWorkspaceWillUnmountNotification";
NSString * const NSWorkspaceDidUnmountNotification=@"NSWorkspaceDidUnmountNotification";
NSString * const NSWorkspaceDidLaunchApplicationNotification=@"NSWorkspaceDidLaunchApplicationNotification";
NSString * const NSWorkspaceDidTerminateApplicationNotification=@"NSWorkspaceDidTerminateApplicationNotification";
NSString * const NSWorkspaceWillLaunchApplicationNotification=@"NSWorkspaceWillLaunchApplicationNotification";

NSString * const NSPlainFileType=@"NSPlainFileType";
NSString * const NSDirectoryFileType=@"NSDirectoryFileType";
NSString * const NSApplicationFileType=@"NSApplicationFileType";
NSString * const NSFilesystemFileType=@"NSFilesystemFileType";
NSString * const NSShellCommandFileType=@"NSShellCommandFileType";

NSString * const NSWorkspaceRecycleOperation=@"NSWorkspaceRecycleOperation";
NSString * const NSWorkspaceMoveOperation=@"NSWorkspaceMoveOperation";
NSString * const NSWorkspaceCopyOperation=@"NSWorkspaceCopyOperation";
NSString * const NSWorkspaceLinkOperation=@"NSWorkspaceLinkOperation";
NSString * const NSWorkspaceCompressOperation=@"NSWorkspaceCompressOperation";
NSString * const NSWorkspaceDecompressOperation=@"NSWorkspaceDecompressOperation";
NSString * const NSWorkspaceEncryptOperation=@"NSWorkspaceEncryptOperation";
NSString * const NSWorkspaceDecryptOperation=@"NSWorkspaceDecryptOperation";
NSString * const NSWorkspaceDestroyOperation=@"NSWorkspaceDestroyOperation";
NSString * const NSWorkspaceDuplicateOperation=@"NSWorkspaceDuplicateOperation";


/* Bridge to LaunchServices, which does the application scanning. */
static CFURLRef PDCopyURLForPath(NSString *path)
{
    if ([path length] == 0)
        return NULL;

    return CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)path,
                                         kCFURLPOSIXPathStyle, false);
}

static NSString *PDPathForURL(CFURLRef url)
{
    if (url == NULL)
        return nil;

    CFStringRef path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);

    return path ? [(NSString *)path autorelease] : nil;
}

/* The bundles this process launched, so -launchedApplications can answer. */
static NSMutableArray *PDLaunchedApplications(void)
{
    static NSMutableArray *launched = nil;

    if (launched == nil)
        launched = [[NSMutableArray alloc] initWithCapacity:0];

    return launched;
}

static void PDNoteLaunched(NSString *path)
{
    NSString *name = [[path lastPathComponent] stringByDeletingPathExtension];
    NSDictionary *entry = [NSDictionary dictionaryWithObjectsAndKeys:
        name ? name : @"", @"NSApplicationName",
        path ? path : @"", @"NSApplicationPath",
        nil];

    [PDLaunchedApplications() addObject:entry];
}

@implementation NSWorkspace


+(NSWorkspace *)sharedWorkspace {
   return NSThreadSharedInstance(@"NSWorkspace");
}

-init {
   _notificationCenter=[[NSNotificationCenter alloc] init];
   return self;
}

-(NSNotificationCenter *)notificationCenter {
   return _notificationCenter;
}

-(NSImage *)iconForFile:(NSString *)path {
   /* An application shows its own icon; other files fall back by type. */
   NSFileManager *manager=[NSFileManager defaultManager];
   NSString *extension=[[path pathExtension] lowercaseString];

   if([extension isEqualToString:@"app"]){
    NSBundle *bundle=[NSBundle bundleWithPath:path];
    NSString *iconName=[[bundle infoDictionary] objectForKey:@"CFBundleIconFile"];

    if([iconName length]>0){
     NSString *iconPath=[bundle pathForResource:[iconName stringByDeletingPathExtension]
                                         ofType:[iconName pathExtension]];

     if(iconPath==nil)
      iconPath=[bundle pathForResource:iconName ofType:@"icns"];
     if(iconPath==nil)
      iconPath=[bundle pathForResource:iconName ofType:@"tiff"];

     if(iconPath!=nil)
      return [[[NSImage alloc] initWithContentsOfFile:iconPath] autorelease];
    }
   }

   /* Apple always answers with something; a missing file still gets the
    * generic icon, and callers cache the result without checking for nil. */
   (void)manager;

   return [self iconForFileType:extension];
}

-(NSImage *)iconForFiles:(NSArray *)array {
   if([array count]==1)
    return [self iconForFile:[array objectAtIndex:0]];

   /* A multiple selection has no combined icon here; give the generic one. */
   return [self iconForFileType:@""];
}

-(NSImage *)iconForFileType:(NSString *)type {
   /* No icon theme is installed yet. A named image is tried first, but callers
    * cache whatever comes back and cannot handle nil, so fall back to a blank
    * image of icon size rather than returning nothing. */
   NSImage *named=([type length]>0)?[NSImage imageNamed:type]:nil;

   if(named!=nil)
    return named;

   static NSImage *generic=nil;

   if(generic==nil){
    /* Drawn rather than left empty: an NSImage with a size but no
     * representation crashes when composited. */
    generic=[[NSImage alloc] initWithSize:NSMakeSize(48,48)];

    [generic lockFocus];
    [[NSColor lightGrayColor] set];
    NSRectFill(NSMakeRect(4,4,40,40));
    [[NSColor darkGrayColor] set];
    NSFrameRect(NSMakeRect(4,4,40,40));
    [generic unlockFocus];
   }

   return generic;
}

-(NSString *)localizedDescriptionForType:(NSString *)type {
   CFURLRef url=PDCopyURLForPath([@"/x." stringByAppendingString:type?type:@""]);

   if(url==NULL)
    return type;

   CFStringRef kind=LSCopyKindStringForURL(url);
   CFRelease(url);

   return kind?[(NSString *)kind autorelease]:type;
}

-(BOOL)filenameExtension:(NSString *)extension isValidForType:(NSString *)type {
   return [extension isEqualToString:type];
}

-(NSString *)preferredFilenameExtensionForType:(NSString *)type {
   return type;
}

-(BOOL)type:(NSString *)type conformsToType:(NSString *)conformsToType {
   return [type isEqualToString:conformsToType];
}

-(NSString *)typeOfFile:(NSString *)path error:(NSError **)error {
   return [path pathExtension];
}

-(BOOL)openFile:(NSString *)path {
   CFURLRef url=PDCopyURLForPath(path);

   if(url==NULL)
    return NO;

   CFURLRef launched=NULL;
   OSStatus status=LSOpenCFURLRef(url,&launched);
   CFRelease(url);

   if(launched!=NULL){
    PDNoteLaunched(PDPathForURL(launched));
    CFRelease(launched);
   }

   return (status==noErr)?YES:NO;
}

-(BOOL)openFile:(NSString *)path withApplication:(NSString *)application {
   return [self openFile:path withApplication:application andDeactivate:YES];
}

-(BOOL)openTempFile:(NSString *)path {
   return [self openFile:path];
}

-(BOOL)openFile:(NSString *)path fromImage:(NSImage *)image at:(NSPoint)point inView:(NSView *)view {
   return [self openFile:path];
}

-(BOOL)openFile:(NSString *)path withApplication:(NSString *)application andDeactivate:(BOOL)deactivate {
   NSString *appPath=[self fullPathForApplication:application];
   CFURLRef appURL=PDCopyURLForPath(appPath?appPath:application);
   CFURLRef itemURL=PDCopyURLForPath(path);

   if(appURL==NULL){
    if(itemURL!=NULL)
     CFRelease(itemURL);
    return NO;
   }

   CFArrayRef items=NULL;
   if(itemURL!=NULL){
    const void *values[1]={itemURL};
    items=CFArrayCreate(kCFAllocatorDefault,values,1,&kCFTypeArrayCallBacks);
   }

   OSStatus status=LSOpenApplicationAtURL(appURL,items,NULL);

   if(items!=NULL)
    CFRelease(items);
   if(itemURL!=NULL)
    CFRelease(itemURL);
   CFRelease(appURL);

   if(status==noErr)
    PDNoteLaunched(appPath?appPath:application);

   return (status==noErr)?YES:NO;
}

-(BOOL)openURL:(NSURL *)url {
   if(url==nil)
    return NO;

   CFURLRef ref=(CFURLRef)url;
   CFURLRef launched=NULL;
   OSStatus status=LSOpenCFURLRef(ref,&launched);

   if(launched!=NULL){
    PDNoteLaunched(PDPathForURL(launched));
    CFRelease(launched);
   }

   return (status==noErr)?YES:NO;
}

-(BOOL)selectFile:(NSString *)path inFileViewerRootedAtPath:(NSString *)rootedAtPath {
   /* Ask the running workspace to reveal it. */
   if([path length]==0)
    return NO;

   [[NSDistributedNotificationCenter defaultCenter]
     postNotificationName:@"GWSelectFileNotification"
                   object:nil
                 userInfo:[NSDictionary dictionaryWithObject:path forKey:@"path"]];
   return YES;
}

-(void)slideImage:(NSImage *)image from:(NSPoint)from to:(NSPoint)to {
   /* no drag animation */
}

-(BOOL)performFileOperation:(NSString *)operation source:(NSString *)source destination:(NSString *)destination files:(NSArray *)files tag:(int *)tag {
   NSFileManager *manager=[NSFileManager defaultManager];
   BOOL ok=YES;

   if(tag!=NULL)
    *tag=0;

   for(NSString *name in files){
    NSString *from=[source stringByAppendingPathComponent:name];
    NSString *to=[destination stringByAppendingPathComponent:name];

    if([operation isEqualToString:NSWorkspaceMoveOperation])
     ok=[manager moveItemAtPath:from toPath:to error:NULL];
    else if([operation isEqualToString:NSWorkspaceCopyOperation])
     ok=[manager copyItemAtPath:from toPath:to error:NULL];
    else if([operation isEqualToString:NSWorkspaceLinkOperation])
     ok=[manager linkItemAtPath:from toPath:to error:NULL];
    else if([operation isEqualToString:NSWorkspaceDestroyOperation])
     ok=[manager removeItemAtPath:from error:NULL];
    else if([operation isEqualToString:NSWorkspaceDuplicateOperation])
     ok=[manager copyItemAtPath:from
                         toPath:[source stringByAppendingPathComponent:
                                   [name stringByAppendingString:@" copy"]]
                          error:NULL];
    else if([operation isEqualToString:NSWorkspaceRecycleOperation]){
     NSString *trash=[NSHomeDirectory() stringByAppendingPathComponent:@".Trash"];

     [manager createDirectoryAtPath:trash withIntermediateDirectories:YES
                         attributes:nil error:NULL];
     ok=[manager moveItemAtPath:from
                         toPath:[trash stringByAppendingPathComponent:name]
                          error:NULL];
    }
    else
     ok=NO;   /* compress/encrypt have no implementation here */

    if(!ok){
     if(tag!=NULL)
      *tag=-1;
     return NO;
    }
   }

   [self noteFileSystemChanged];
   return YES;
}

-(BOOL)getFileSystemInfoForPath:(NSString *)path isRemovable:(BOOL *)isRemovable isWritable:(BOOL *)isWritable isUnmountable:(BOOL *)isUnmountable description:(NSString **)description type:(NSString **)type {
   struct statfs info;

   if(path==nil || statfs([path fileSystemRepresentation],&info)!=0)
    return NO;

   if(isRemovable!=NULL)
    *isRemovable=(info.f_flags&MNT_REMOVABLE)?YES:NO;
   if(isWritable!=NULL)
    *isWritable=(info.f_flags&MNT_RDONLY)?NO:YES;
   if(isUnmountable!=NULL)
    *isUnmountable=(info.f_flags&MNT_ROOTFS)?NO:YES;
   if(description!=NULL)
    *description=[NSString stringWithUTF8String:info.f_mntfromname];
   if(type!=NULL)
    *type=[NSString stringWithUTF8String:info.f_fstypename];

   return YES;
}

-(BOOL)getInfoForFile:(NSString *)path application:(NSString **)application type:(NSString **)type {
   NSFileManager *manager=[NSFileManager defaultManager];

   if(![manager fileExistsAtPath:path])
    return NO;

   if(application!=NULL)
    *application=nil;   /* no launch-services registry to answer from */
   if(type!=NULL)
    *type=[path pathExtension];

   return YES;
}

-(void)checkForRemovableMedia {
   /* Polling for new media is the volume daemon's job, not ours. */
}

-(NSArray *)mountNewRemovableMedia {
   /* Nothing here mounts media on demand; report what is already present. */
   return [self mountedRemovableMedia];
}

-(NSArray *)mountedRemovableMedia {
   struct statfs *mounts=NULL;
   int count=getmntinfo(&mounts,MNT_NOWAIT);
   NSMutableArray *result=[NSMutableArray arrayWithCapacity:0];

   for(int i=0;i<count;i++)
    if(mounts[i].f_flags&MNT_REMOVABLE)
     [result addObject:[NSString stringWithUTF8String:mounts[i].f_mntonname]];

   return result;
}

-(NSArray *)mountedLocalVolumePaths {
   struct statfs *mounts=NULL;
   int count=getmntinfo(&mounts,MNT_NOWAIT);
   NSMutableArray *result=[NSMutableArray arrayWithCapacity:count>0?count:0];

   for(int i=0;i<count;i++)
    [result addObject:[NSString stringWithUTF8String:mounts[i].f_mntonname]];

   return result;
}

-(BOOL)unmountAndEjectDeviceAtPath:(NSString *)path {
   if([path length]==0)
    return NO;

   return (unmount([path fileSystemRepresentation],0)==0)?YES:NO;
}

-(BOOL)fileSystemChanged {
   return NO;
}

-(BOOL)userDefaultsChanged {
   return NO;
}

-(void)noteFileSystemChanged {
   [_notificationCenter postNotificationName:@"NSWorkspaceFileSystemChanged" object:self];
}

-(void)noteFileSystemChanged:(NSString *)path {
   [_notificationCenter postNotificationName:@"NSWorkspaceFileSystemChanged"
                                      object:self
                                    userInfo:path?[NSDictionary dictionaryWithObject:path forKey:@"path"]:nil];
}

-(void)noteUserDefaultsChanged {
   [[NSUserDefaults standardUserDefaults] synchronize];
}

-(BOOL)isFilePackageAtPath:(NSString *)path {
   NSFileManager *manager=[NSFileManager defaultManager];
   BOOL isDirectory=NO;

   if(![manager fileExistsAtPath:path isDirectory:&isDirectory] || !isDirectory)
    return NO;

   /* A directory with a known wrapper extension is presented as one file. */
   NSString *extension=[[path pathExtension] lowercaseString];

   if([extension length]==0)
    return NO;

   return ([extension isEqualToString:@"app"] ||
           [extension isEqualToString:@"bundle"] ||
           [extension isEqualToString:@"framework"] ||
           [extension isEqualToString:@"plugin"] ||
           [extension isEqualToString:@"kext"] ||
           [extension isEqualToString:@"rtfd"] ||
           [extension isEqualToString:@"pkg"])?YES:NO;
}

-(NSString *)absolutePathForAppBundleWithIdentifier:(NSString *)identifier {
   if([identifier length]==0)
    return nil;

   CFArrayRef urls=LSCopyApplicationURLsForBundleIdentifier((CFStringRef)identifier,NULL);

   if(urls==NULL)
    return nil;

   NSString *result=nil;
   if(CFArrayGetCount(urls)>0)
    result=PDPathForURL(CFArrayGetValueAtIndex(urls,0));

   CFRelease(urls);
   return result;
}

-(NSString *)pathForApplication:(NSString *)application {
   return [self fullPathForApplication:application];
}

-(NSArray *)launchedApplications {
   return [NSArray arrayWithArray:PDLaunchedApplications()];
}

-(BOOL)launchApplication:(NSString *)application {
   return [self launchApplication:application showIcon:YES autolaunch:NO];
}

-(BOOL)launchApplication:(NSString *)application showIcon:(BOOL)showIcon autolaunch:(BOOL)autolaunch {
   NSString *path=[self fullPathForApplication:application];
   CFURLRef appURL=PDCopyURLForPath(path?path:application);

   if(appURL==NULL)
    return NO;

   OSStatus status=LSOpenApplicationAtURL(appURL,NULL,NULL);
   CFRelease(appURL);

   if(status==noErr)
    PDNoteLaunched(path?path:application);

   return (status==noErr)?YES:NO;
}

-(void)findApplications {
   LSRefreshApplicationRegistry();
}

-(NSDictionary *)activeApplication {
   /* This process is the active one; there is no session-wide arbiter yet. */
   NSString *path=[[NSBundle mainBundle] bundlePath];

   return [NSDictionary dictionaryWithObjectsAndKeys:
     [[NSProcessInfo processInfo] processName], @"NSApplicationName",
     path?path:@"", @"NSApplicationPath",
     [NSNumber numberWithInt:[[NSProcessInfo processInfo] processIdentifier]],
       @"NSApplicationProcessIdentifier",
     nil];
}

-(void)hideOtherApplications {
   /* single-application session */
}

-(int)extendPowerOffBy:(int)milliseconds {
   return 0;
}


/* Searches the Applications directory of each domain for a matching bundle. */
-(NSString *)fullPathForApplication:(NSString *)appName {
   if([appName length]==0)
    return nil;

   if([appName isAbsolutePath])
    return [[NSFileManager defaultManager] fileExistsAtPath:appName] ? appName : nil;

   NSString *withExtension=[[appName pathExtension] length]>0 ? appName :
                            [appName stringByAppendingPathExtension:@"app"];
   NSFileManager *manager=[NSFileManager defaultManager];

   for(NSString *directory in NSSearchPathForDirectoriesInDomains(
        NSApplicationDirectory,NSAllDomainsMask,YES)){
    NSString *candidate=[directory stringByAppendingPathComponent:withExtension];

    if([manager fileExistsAtPath:candidate])
     return candidate;
   }
   return nil;
}

@end

@implementation NSWorkspace (CocotronAdditions)

- (BOOL)isFileHiddenAtPath:(NSString*)path
{
	return NO;
}

@end

