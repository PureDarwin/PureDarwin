/* Copyright (c) 2006-2007 Christopher J. W. Lloyd <cjwl@objc.net>, 2008 Johannes Fortmann

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <AppKit/NSSavePanel.h>
#import <AppKit/NSOpenPanel.h>
#import <AppKit/NSView.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSDisplay.h>
#import <AppKit/NSRaise.h>
#import <AppKit/NSFilePanelController.h>
#import <objc/runtime.h>

static char NSSavePanelCanCreateDirectoriesKey, NSSavePanelAllowsOtherFileTypesKey;

@implementation NSSavePanel

+(NSSavePanel *)savePanel {
   return [[self new] autorelease];
}

// a new panel starts where the last one in this application was confirmed
+(NSString *)_initialDirectory {
   NSString *last=[[NSUserDefaults standardUserDefaults] stringForKey:@"NSNavLastRootDirectory"];
   BOOL      isDirectory=NO;

   last=[last stringByExpandingTildeInPath];
   if([last length]>0 && [[NSFileManager defaultManager] fileExistsAtPath:last isDirectory:&isDirectory] && isDirectory)
    return last;

   return NSHomeDirectory();
}

-init {
   [super initWithContentRect:NSMakeRect(0,0,430,160) styleMask:NSTitledWindowMask|NSResizableWindowMask backing:NSBackingStoreBuffered defer:YES];

   _dialogTitle=[NSLocalizedStringFromTableInBundle(@"Save", nil, [NSBundle bundleForClass:[NSSavePanel class]], @"save panel title") copy];
   [super setTitle:_dialogTitle];
   _filename=@"";
   _directory=[[NSSavePanel _initialDirectory] copy];
   _requiredFileType=@"";
   _message=@"";
   _treatsFilePackagesAsDirectories=NO;
   _accessoryView=nil;
   [self setReleasedWhenClosed:NO];
   [self setHidesOnDeactivate:NO];
   return self;
}

-(void)dealloc {
   [_dialogTitle release];
   [_filename release];
   [_directory release];
   [_requiredFileType release];
   [_message release];
   [_prompt release];
   [_accessoryView release];
   [super dealloc];
}

-(void)_setFilename:(NSString*)filename {
   @synchronized(self) {
      if(filename!=_filename) {
         [_filename release];
         _filename=[filename copy];
         if(_filename==nil) {
            _filename=@"";
         }
      }
   }
}

// a save panel's URL is its folder and the name in the field, also before and after a cancel
-(NSURL *)URL {
   NSString *path=[self filename];

   if([path length]==0)
    return nil;

   return [NSURL fileURLWithPath:path];
}

-(NSString *)filename {
   id ret=nil;
	@synchronized(self)
	{
		ret=[[_filename copy] autorelease];
	}
   if([ret length]==0 && ![self isKindOfClass:[NSOpenPanel class]] && [_directory length]>0)
    ret=[_directory stringByAppendingPathComponent:[self _liveNameFieldValue]];
   return ret;
}

// the older calls take the start directory and name, then run the same panel as runModal
-(int)runModalForDirectory:(NSString *)directory file:(NSString *)file {
   if(directory!=nil)
    [self setDirectory:directory];
   if(file!=nil)
    [self _setNameFieldValue:file];
   return [self runModal];
}

-(int)runModal {
   return [self _runFilePanelModal];
}

-(NSString *)directory {
   return _directory;
}

-(BOOL)treatsFilePackagesAsDirectories {
   return _treatsFilePackagesAsDirectories;
}

-(NSView *)accessoryView {
   return _accessoryView;
}

-(void)setTitle:(NSString *)title {
   title=[title copy];
   [_dialogTitle release];
   _dialogTitle=title;
   [super setTitle:title?title:@""];
}

-(void)setDirectory:(NSString *)directory {
   directory=[directory copy];
   [_directory release];
   _directory=directory;
}

-(void)setRequiredFileType:(NSString *)type {
	@synchronized(self)
	{
		type=[type copy];
		[_requiredFileType release];
		_requiredFileType=type;
	}
   [self setAllowedFileTypes:[type length]>0?[NSArray arrayWithObject:type]:nil];
}

-(NSString*)requiredFileType
{
   id ret=nil;
	@synchronized(self)
	{
		ret=[[_requiredFileType copy] autorelease];
	}
   return ret;
}

-(void)setMessage:(NSString*)message;
{
	@synchronized(self)
	{
		if(_message!=message)
		{
			[_message release];
			_message=[(message?message:@"") copy];
		}
	}
}

-(NSString*)message;
{
   id ret=nil;
	@synchronized(self)
	{
		ret=[[_message copy] autorelease];
	}
   return ret;
}

-(void)setPrompt:(NSString*)prompt;
{
	@synchronized(self)
	{
		if(_prompt!=prompt)
		{
			[_prompt release];
			_prompt=[prompt copy];
		}
	}
}

// with no prompt set the button reads Save, or Open in an open panel
-(NSString*)prompt;
{
	id ret=nil;
	@synchronized(self)
	{
		ret=[[_prompt copy] autorelease];
	}
	if(ret==nil)
	 ret=[self isKindOfClass:[NSOpenPanel class]]?@"Open":@"Save";
	return ret;
}

-(void)setTreatsFilePackagesAsDirectories:(BOOL)flag {
   _treatsFilePackagesAsDirectories=flag;
}

-(void)setAccessoryView:(NSView *)view {
   view=[view retain];
   [_accessoryView release];
   _accessoryView=view;
}

-(void)setCanCreateDirectories:(BOOL)value {
   NSUnimplementedMethod();
}

-(void)setAllowsOtherFileTypes:(BOOL)value {
   NSUnimplementedMethod();
}

- (void)beginSheetForDirectory:(NSString *)path
						  file:(NSString *)name 
				modalForWindow:(NSWindow *)docWindow
				 modalDelegate:(id)modalDelegate 
				didEndSelector:(SEL)didEndSelector 
				   contextInfo:(void *)contextInfo
{
	id inv=[NSInvocation invocationWithMethodSignature:[self methodSignatureForSelector:@selector(_background_beginSheetForDirectory:file:modalForWindow:modalDelegate:didEndSelector:contextInfo:)]];
	[inv setTarget:self];
	[inv setSelector:@selector(_background_beginSheetForDirectory:file:modalForWindow:modalDelegate:didEndSelector:contextInfo:)];
	[inv setArgument:&path atIndex:2];
	[inv setArgument:&name atIndex:3];
	[inv setArgument:&docWindow atIndex:4];
	[inv setArgument:&modalDelegate atIndex:5];
	[inv setArgument:&didEndSelector atIndex:6];
	[inv setArgument:&contextInfo atIndex:7];
	[inv retainArguments];
	[inv performSelectorInBackground:@selector(invoke) withObject:nil];
}

- (void)_selector_savePanelDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode  contextInfo:(void  *)contextInfo;
{
	
}

- (void)_background_beginSheetForDirectory:(NSString *)path
									  file:(NSString *)name 
							modalForWindow:(NSWindow *)docWindow
							 modalDelegate:(id)modalDelegate 
							didEndSelector:(SEL)didEndSelector 
							   contextInfo:(void *)contextInfo
{
	id pool=[NSAutoreleasePool new];
	int ret=[self runModalForDirectory:path file:name];
   
	id inv=[NSInvocation invocationWithMethodSignature:[self methodSignatureForSelector:@selector(_selector_savePanelDidEnd:returnCode:contextInfo:)]];
	
	[inv setTarget:modalDelegate];
	[inv setSelector:didEndSelector];
	[inv setArgument:&self atIndex:2];
	[inv setArgument:&ret atIndex:3];
	[inv setArgument:&contextInfo atIndex:4];
	[inv retainArguments];
	
	[inv performSelectorOnMainThread:@selector(invoke) withObject:nil waitUntilDone:YES];
   [pool release];
}

@end
