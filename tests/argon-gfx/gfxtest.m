// gfxtest: an 800x600 window with a text field, for argon damage and present measurements
// GFXTEST_AUTO=N types N characters into the field by itself, one every 50 ms, then quits
#import <AppKit/AppKit.h>
#include <sys/resource.h>
#include <stdio.h>

@interface GfxTest : NSObject {
   NSWindow    *_window;
   NSTextField *_field;
   int          _left;
   double       _lastCPU;
   int          _typed;
}
@end

static double cpuSeconds(void){
   struct rusage usage;

   getrusage(RUSAGE_SELF,&usage);
   return usage.ru_utime.tv_sec+usage.ru_utime.tv_usec/1e6+usage.ru_stime.tv_sec+usage.ru_stime.tv_usec/1e6;
}

@implementation GfxTest

-(void)applicationDidFinishLaunching:(NSNotification *)note {
   _window=[[NSWindow alloc] initWithContentRect:NSMakeRect(100,100,800,600)
     styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable
     backing:NSBackingStoreBuffered defer:NO];
   [_window setTitle:@"gfxtest"];
   _field=[[NSTextField alloc] initWithFrame:NSMakeRect(40,520,720,24)];
   [[_window contentView] addSubview:_field];
   NSTextField *label=[[NSTextField alloc] initWithFrame:NSMakeRect(40,40,720,400)];
   [label setStringValue:@"Static text below the field. Typing above should only damage the field."];
   [label setEditable:NO];
   [label setBordered:NO];
   [[_window contentView] addSubview:label];
   [_window makeKeyAndOrderFront:nil];
   [_window makeFirstResponder:_field];
   [NSApp activateIgnoringOtherApps:YES];

   _lastCPU=cpuSeconds();
   [NSTimer scheduledTimerWithTimeInterval:5 target:self selector:@selector(report:) userInfo:nil repeats:YES];
   char *autoCount=getenv("GFXTEST_AUTO");
   if(autoCount!=NULL){
    _left=atoi(autoCount);
    [NSTimer scheduledTimerWithTimeInterval:2 target:self selector:@selector(startTyping:) userInfo:nil repeats:NO];
   }
   fprintf(stderr,"GFXTEST ready\n");
}

-(void)startTyping:(NSTimer *)timer {
   [NSTimer scheduledTimerWithTimeInterval:0.05 target:self selector:@selector(type:) userInfo:nil repeats:YES];
}

-(void)type:(NSTimer *)timer {
   if(_left<=0){
    [timer invalidate];
    [self report:nil];
    fprintf(stderr,"GFXTEST done\n");
    [NSApp terminate:nil];
    return;
   }
   NSText *editor=[_window fieldEditor:YES forObject:_field];

   [editor insertText:(_typed%10==9)?@" ":@"a"];
   _typed++;
   _left--;
}

-(void)report:(NSTimer *)timer {
   double now=cpuSeconds();

   fprintf(stderr,"GFXTEST cpu %.3f s in the last period, %d typed\n",now-_lastCPU,_typed);
   _lastCPU=now;
}

-(BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app {
   return YES;
}

@end

int main(int argc,const char *argv[]){
   NSAutoreleasePool *pool=[NSAutoreleasePool new];
   NSApplication *app=[NSApplication sharedApplication];
   GfxTest *test=[GfxTest new];

   [app setDelegate:test];
   [app run];
   [pool release];
   return 0;
}
