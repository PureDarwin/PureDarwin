/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <AppKit/NSModernAppKit.h>
#import <AppKit/NSOpenPanel.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSDisplay.h>
#import <AppKit/NSFilePanelController.h>
#import <Foundation/NSURL.h>

@interface NSSavePanel (Private)
-(void)_setFilename:(NSString*)filename;
+(NSString *)_initialDirectory;
@end

@implementation NSOpenPanel

+(NSOpenPanel *)openPanel {
   return [[self new] autorelease];
}

-init {
   [super init];
   _filenames=[NSArray new];
   [self setTitle:NSLocalizedStringFromTableInBundle(@"Open", nil, [NSBundle bundleForClass: [NSOpenPanel class]], @"The title of the open panel")];
   _allowsMultipleSelection=NO;
   _canChooseDirectories=NO;
   _canChooseFiles=YES;
   _resolvesAliases=YES;
   return self;
}

-(void)dealloc {
   [_filenames release];
   [super dealloc];
}

-(NSArray *)filenames {
   id ret=nil;
   @synchronized(self)
   {
      ret=[[_filenames copy] autorelease];
   }
   return ret;
}

-(NSArray *)URLs {
   NSArray        *paths=[self filenames];
   NSMutableArray *result=[NSMutableArray arrayWithCapacity:[paths count]];
   int             i,count=[paths count];
   
   for(i=0;i<count;i++)
    [result addObject:[NSURL fileURLWithPath:[paths objectAtIndex:i]]];
    
   return result;
}

-(int)runModalForDirectory:(NSString *)directory file:(NSString *)file types:(NSArray *)types {
   [self _setFilename:file];
   [self setDirectory:directory];
   return [[NSDisplay currentDisplay] openPanel:self runModalForDirectory:directory file:file types:types];
}

-(int)runModalForTypes:(NSArray *)types {
   return [self runModalForDirectory:[self directory] file:nil types:types];
}

-(int)runModalForDirectory:(NSString *)directory file:(NSString *)file {
   return [self runModalForDirectory:directory file:file types:[self allowedFileTypes]];
}

-(int)runModal {
   return [self runModalForDirectory:nil file:nil types:[self allowedFileTypes]];
}

-(BOOL)allowsMultipleSelection {
   return _allowsMultipleSelection;
}

-(BOOL)canChooseDirectories {
   return _canChooseDirectories;
}

-(BOOL)canChooseFiles {
   return _canChooseFiles;
}

-(BOOL)resolvesAliases {
   return _resolvesAliases;
}

// there is no cloud storage, so every file is already local
-(BOOL)canDownloadUbiquitousContents {
   return YES;
}

-(void)setCanDownloadUbiquitousContents:(BOOL)flag {
}

-(BOOL)canResolveUbiquitousConflicts {
   return NO;
}

-(void)setCanResolveUbiquitousConflicts:(BOOL)flag {
}

-(void)setAllowsMultipleSelection:(BOOL)flag {
   _allowsMultipleSelection=flag;
}

-(void)setCanChooseDirectories:(BOOL)flag {
   _canChooseDirectories=flag;
}

-(void)setCanChooseFiles:(BOOL)flag {
   _canChooseFiles=flag;
}

-(void)setResolvesAliases:(BOOL)value {
   _resolvesAliases=value;
}

-(void)beginSheetForDirectory:(NSString *)path
                         file:(NSString *)name
                        types:(NSArray *)fileTypes
               modalForWindow:(NSWindow *)docWindow
                modalDelegate:(id)modalDelegate
               didEndSelector:(SEL)didEndSelector
                  contextInfo:(void *)contextInfo
{
   if(fileTypes!=nil)
    [self setAllowedFileTypes:fileTypes];
   [self beginSheetForDirectory:path file:name modalForWindow:docWindow modalDelegate:modalDelegate didEndSelector:didEndSelector contextInfo:contextInfo];
}

-(void)beginForDirectory:(NSString *)path file:(NSString *)name types:(NSArray *)fileTypes modelessDelegate:(id)delegate didEndSelector:(SEL)didEndSelector contextInfo:(void *)contextInfo {
   if(fileTypes!=nil)
    [self setAllowedFileTypes:fileTypes];
   [self beginSheetForDirectory:path file:name modalForWindow:nil modalDelegate:delegate didEndSelector:didEndSelector contextInfo:contextInfo];
}

@end
