/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSPasteboard.h>
#import <AppKit/NSDisplay.h>
#import <AppKit/NSRaise.h>
#import <AppKit/NSApplication.h>
#import <WindowServer/rpc.h>
#include <unistd.h>
#include <pthread.h>

NSString *const NSPasteboardTypeString = @"public.utf8-plain-text";
NSString *const NSPasteboardTypePDF = @"com.adobe.pdf";
NSString *const NSPasteboardTypeTIFF = @"public.tiff";
NSString *const NSPasteboardTypeRTF = @"public.rtf";
NSString *const NSPasteboardTypeRTFD = @"com.apple.flat-rtfd";
NSString *const NSPasteboardTypeHTML = @"public.html";
NSString *const NSPasteboardTypeTabularText = @"public.utf8-tab-separated-values-text";
NSString *const NSPasteboardTypeFont = @"com.apple.cocoa.pasteboard.character-formatting";
NSString *const NSPasteboardTypeRuler = @"com.apple.cocoa.pasteboard.paragraph-formatting";
NSString *const NSPasteboardTypeColor = @"com.apple.cocoa.pasteboard.color";
NSString *const NSPasteboardTypeFileURL = @"public.file-url";
NSString *const NSPasteboardTypeURL = @"public.url";

NSString * const NSColorPboardType=@"NSColorPboardType";
NSString * const NSFileContentsPboardType=@"NSFileContentsPboardType";
NSString * const NSFilenamesPboardType=@"NSFilenamesPboardType";
NSString * const NSFontPboardType=@"NSFontPboardType";
NSString * const NSPDFPboardType=@"NSPDFPboardType";
NSString * const NSPICTPboardType=@"NSPICTPboardType";
NSString * const NSPostScriptPboardType=@"NSPostScriptPboardType";
NSString * const NSRTFDPboardType=@"NSRTFDPboardType";
NSString * const NSRTFPboardType=@"NSRTFPboardType";
NSString * const NSRulerPboardType=@"NSRulerPboardType";
NSString * const NSStringPboardType=@"NSStringPboardType";
NSString * const NSTabularTextPboardType=@"NSTabularTextPboardType";
NSString * const NSTIFFPboardType=@"NSTIFFPboardType";
NSString * const NSURLPboardType=@"NSURLPboardType";

NSString * const NSDragPboard=@"NSDragPboard";
NSString * const NSFindPboard=@"NSFindPboard";
NSString * const NSFontPboard=@"NSFontPboard";
NSString * const NSGeneralPboard=@"NSGeneralPboard";
NSString * const NSRulerPboard=@"NSRulerPboard";

// a legacy type name stands for its modern UTI, both name the same data
static NSString *PDCanonicalType(NSString *type) {
   static NSDictionary *legacy=nil;

   if(legacy==nil)
    legacy=[[NSDictionary alloc] initWithObjectsAndKeys:
     NSPasteboardTypeString,NSStringPboardType,
     NSPasteboardTypePDF,NSPDFPboardType,
     NSPasteboardTypeTIFF,NSTIFFPboardType,
     NSPasteboardTypeRTF,NSRTFPboardType,
     NSPasteboardTypeRTFD,NSRTFDPboardType,
     NSPasteboardTypeHTML,NSHTMLPboardType,
     NSPasteboardTypeHTML,@"NSPasteboardTypeHTML",
     NSPasteboardTypeTabularText,NSTabularTextPboardType,
     NSPasteboardTypeFont,NSFontPboardType,
     NSPasteboardTypeRuler,NSRulerPboardType,
     NSPasteboardTypeColor,NSColorPboardType,
     nil];

   NSString *modern=(type!=nil)?[legacy objectForKey:type]:nil;

   return modern!=nil?modern:type;
}

// the legacy name macOS still lists next to a modern type
static NSString *PDLegacyType(NSString *type) {
   static NSDictionary *aliases=nil;

   if(aliases==nil)
    aliases=[[NSDictionary alloc] initWithObjectsAndKeys:
     NSStringPboardType,NSPasteboardTypeString,
     NSPDFPboardType,NSPasteboardTypePDF,
     NSTIFFPboardType,NSPasteboardTypeTIFF,
     NSRTFPboardType,NSPasteboardTypeRTF,
     NSRTFDPboardType,NSPasteboardTypeRTFD,
     NSHTMLPboardType,NSPasteboardTypeHTML,
     NSTabularTextPboardType,NSPasteboardTypeTabularText,
     NSFontPboardType,NSPasteboardTypeFont,
     NSRulerPboardType,NSPasteboardTypeRuler,
     NSColorPboardType,NSPasteboardTypeColor,
     nil];
   return [aliases objectForKey:type];
}

// the MIME types a pasteboard type goes out as, best first, and unknown types under a private prefix
static NSArray *PDMimeTypesForType(NSString *type) {
   if([type isEqualToString:NSPasteboardTypeString])
    return @[ @"text/plain;charset=utf-8", @"UTF8_STRING", @"text/plain", @"STRING", @"TEXT" ];
   if([type isEqualToString:NSPasteboardTypeHTML])
    return @[ @"text/html" ];
   if([type isEqualToString:NSPasteboardTypeRTF])
    return @[ @"text/rtf" ];
   if([type isEqualToString:NSPasteboardTypePNG])
    return @[ @"image/png" ];
   if([type isEqualToString:NSPasteboardTypeTIFF])
    return @[ @"image/tiff" ];
   if([type isEqualToString:NSPasteboardTypePDF])
    return @[ @"application/pdf" ];
   if([type isEqualToString:NSPasteboardTypeFileURL] || [type isEqualToString:NSPasteboardTypeURL])
    return @[ @"text/uri-list" ];
   return @[ [@"application/x-pasteboard-type;" stringByAppendingString:type] ];
}

static NSString *PDTypeForMimeType(NSString *mime) {
   NSString *lower=[mime lowercaseString];

   if([lower hasPrefix:@"application/x-pasteboard-type;"])
    return [mime substringFromIndex:[@"application/x-pasteboard-type;" length]];
   if([lower hasPrefix:@"text/plain"] || [lower isEqualToString:@"utf8_string"] || [lower isEqualToString:@"string"] || [lower isEqualToString:@"text"])
    return NSPasteboardTypeString;
   if([lower hasPrefix:@"text/html"])
    return NSPasteboardTypeHTML;
   if([lower isEqualToString:@"text/rtf"] || [lower isEqualToString:@"application/rtf"])
    return NSPasteboardTypeRTF;
   if([lower isEqualToString:@"image/png"])
    return NSPasteboardTypePNG;
   if([lower isEqualToString:@"image/tiff"])
    return NSPasteboardTypeTIFF;
   if([lower isEqualToString:@"application/pdf"])
    return NSPasteboardTypePDF;
   if([lower isEqualToString:@"text/uri-list"])
    return NSPasteboardTypeURL;
   return nil;
}

// MIME type to pasteboard type for what this process last offered, read on the main thread only
static NSDictionary *publishedMimeTypes=nil;

static void PDWriteAll(int fd,NSData *data) {
   const char *bytes=[data bytes];
   size_t left=[data length];

   while(left>0){
    ssize_t wrote=write(fd,bytes,left);
    if(wrote<=0)
     break;
    bytes+=wrote;
    left-=(size_t)wrote;
   }
   close(fd);
}

// one paste request from another client, answered on the main thread where lazy owners may run
@interface PDSelectionRequest : NSObject {
@public
    NSString *_mimeType;
    int _fd;
}
@end

@implementation PDSelectionRequest

-(void)dealloc {
   [_mimeType release];
   [super dealloc];
}

-(void)answer {
   if(getenv("PD_WS_TRACE")!=NULL)
    fprintf(stderr,"pasteboard: answer runs, main thread %d\n",(int)pthread_main_np());
   NSPasteboard *general=[NSPasteboard generalPasteboard];
   NSString *type=[publishedMimeTypes objectForKey:_mimeType];
   NSData *data=(type!=nil)?[general dataForType:type]:nil;

   // a URL goes out as a uri-list line
   if(data!=nil && [_mimeType isEqualToString:@"text/uri-list"]){
    NSString *url=[general stringForType:type];
    data=[[url stringByAppendingString:@"\r\n"] dataUsingEncoding:NSUTF8StringEncoding];
   }
   if(getenv("PD_WS_TRACE")!=NULL)
    NSLog(@"pasteboard: answering %@ as %@ with %lu bytes",_mimeType,type,(unsigned long)[data length]);
   if(data!=nil)
    PDWriteAll(_fd,data);
   else
    close(_fd);
}

@end

// another client pastes: the request waits for the main thread's run loop, in any of the usual modes
static void PDSendSelection(const char *mimeType,int fd) {
   if(getenv("PD_WS_TRACE")!=NULL)
    fprintf(stderr,"pasteboard: request for %s, main thread %d\n",mimeType,(int)pthread_main_np());
   NSAutoreleasePool *pool=[[NSAutoreleasePool alloc] init];
   PDSelectionRequest *request=[[[PDSelectionRequest alloc] init] autorelease];

   request->_mimeType=[[NSString alloc] initWithUTF8String:mimeType];
   request->_fd=fd;
   [request performSelectorOnMainThread:@selector(answer) withObject:nil waitUntilDone:NO
                                  modes:@[ NSDefaultRunLoopMode, NSEventTrackingRunLoopMode, NSModalPanelRunLoopMode ]];
   [pool release];
}

@implementation NSPasteboard

+(NSPasteboard *)generalPasteboard {
   return [self pasteboardWithName:NSGeneralPboard];
}

// named boards live for the life of the process, the general board also
// follows the compositor's selection so copy and paste reach other apps
+(NSPasteboard *)pasteboardWithName:(NSString *)name {
   static NSMutableDictionary *boards=nil;

   if(name==nil)
    name=NSGeneralPboard;

   if(boards==nil)
    boards=[[NSMutableDictionary alloc] initWithCapacity:0];

   NSPasteboard *board=[boards objectForKey:name];

   if(board==nil){
    board=[[NSPasteboard alloc] init];
    board->_name=[name copy];
    board->_items=[[NSMutableDictionary alloc] initWithCapacity:0];
    board->_types=[[NSMutableArray alloc] initWithCapacity:0];
    board->_changeCount=0;
    board->_systemMimeTypes=[[NSMutableDictionary alloc] init];
    [boards setObject:board forKey:name];
   }

   return board;
}

+(NSPasteboard *)pasteboardWithUniqueName {
   return [self pasteboardWithName:[[NSProcessInfo processInfo] globallyUniqueString]];
}

-(BOOL)_isGeneral {
   return [_name isEqualToString:NSGeneralPboard];
}

// takes another client's newer selection as this board's contents
-(void)_pdSyncFromSystem {
   unsigned generation;
   char **mimeTypes=NULL;
   int count;

   if(![self _isGeneral])
    return;
   generation=_windowServerSelectionGeneration();
   if(generation==_systemGeneration)
    return;
   _systemGeneration=generation;
   if(_windowServerSelectionIsOurs())
    return;
   count=_windowServerCopySelectionMimeTypes(&mimeTypes);
   if(count==0){
    free(mimeTypes);
    return;
   }

   if(_owner!=nil && [_owner respondsToSelector:@selector(pasteboardChangedOwner:)])
    [_owner pasteboardChangedOwner:self];
   _owner=nil;
   [_items removeAllObjects];
   [_types removeAllObjects];
   [_systemMimeTypes removeAllObjects];
   _changeCount++;
   _systemBacked=YES;

   [self _pdTakeMimeTypes:mimeTypes count:count];
}

// another client's MIME types become this board's types, read from it on demand
-(void)_pdTakeMimeTypes:(char **)mimeTypes count:(int)count {
   for(int i=0;i<count;i++){
    NSString *mime=[NSString stringWithUTF8String:mimeTypes[i]];
    NSString *type=PDTypeForMimeType(mime);

    if(type!=nil && [_systemMimeTypes objectForKey:type]==nil){
     [_systemMimeTypes setObject:mime forKey:type];
     [_types addObject:type];
    }
    free(mimeTypes[i]);
   }
   free(mimeTypes);

   // a uri-list of files is also a file URL and a filename list
   if([_types containsObject:NSPasteboardTypeURL]){
    NSData *list=[self dataForType:NSPasteboardTypeURL];
    NSString *text=[[[NSString alloc] initWithData:list encoding:NSUTF8StringEncoding] autorelease];
    NSMutableArray *paths=[NSMutableArray array];
    NSString *first=nil;

    for(NSString *line in [text componentsSeparatedByCharactersInSet:[NSCharacterSet newlineCharacterSet]]){
     if([line length]==0 || [line hasPrefix:@"#"])
      continue;
     if(first==nil)
      first=line;
     if([line hasPrefix:@"file:"])
      [paths addObject:[[NSURL URLWithString:line] path] ?: line];
    }
    if(first!=nil)
     [_items setObject:[first dataUsingEncoding:NSUTF8StringEncoding] forKey:NSPasteboardTypeURL];
    if([paths count]>0){
     [_types addObject:NSPasteboardTypeFileURL];
     [_items setObject:[first dataUsingEncoding:NSUTF8StringEncoding] forKey:NSPasteboardTypeFileURL];
     [_types addObject:NSFilenamesPboardType];
     [_items setObject:[NSPropertyListSerialization dataFromPropertyList:paths format:NSPropertyListXMLFormat_v1_0 errorDescription:NULL]
                forKey:NSFilenamesPboardType];
    }
   }
}

// the board a drag from another client is read through, filled from the offer when it enters
+(NSPasteboard *)_pdDragOfferPasteboard {
   NSPasteboard *board=[self pasteboardWithName:@"PDDragOffer"];
   char **mimeTypes=NULL;
   int count=_windowServerCopyDragMimeTypes(&mimeTypes,NULL);

   board->_owner=nil;
   [board->_items removeAllObjects];
   [board->_types removeAllObjects];
   [board->_systemMimeTypes removeAllObjects];
   board->_changeCount++;
   board->_systemBacked=YES;
   board->_dragBacked=YES;
   [board _pdTakeMimeTypes:mimeTypes count:count];
   return board;
}

// what a type goes out as when this process is the drag source
+(NSArray *)_pdMimeTypesForType:(NSString *)type {
   return PDMimeTypesForType(PDCanonicalType(type));
}

+(NSString *)_pdTypeForMimeType:(NSString *)mime {
   return PDTypeForMimeType(mime);
}

// offers this board to other clients once the current burst of writes is done
-(void)_pdSchedulePublish {
   if(![self _isGeneral])
    return;
   _systemBacked=NO;
   [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_pdPublish) object:nil];
   [self performSelector:@selector(_pdPublish) withObject:nil afterDelay:0
                 inModes:@[ NSDefaultRunLoopMode, NSEventTrackingRunLoopMode, NSModalPanelRunLoopMode ]];
}

-(void)_pdPublish {
   NSMutableArray *mimeTypes=[NSMutableArray array];
   NSMutableDictionary *map=[NSMutableDictionary dictionary];
   const char **list;
   NSUInteger count;

   for(NSString *type in _types){
    for(NSString *mime in PDMimeTypesForType(type)){
     if([map objectForKey:mime]==nil){
      [map setObject:type forKey:mime];
      [mimeTypes addObject:mime];
     }
    }
   }
   [publishedMimeTypes release];
   publishedMimeTypes=[map copy];

   count=[mimeTypes count];
   list=malloc(sizeof(char *)*(count+1));
   for(NSUInteger i=0;i<count;i++)
    list[i]=[[mimeTypes objectAtIndex:i] UTF8String];
   _windowServerSetSelection(list,(int)count,PDSendSelection);
   free(list);
}

-(NSString *)name {
   return _name;
}

-(int)changeCount {
   [self _pdSyncFromSystem];
   return _changeCount;
}

// modern types first, each followed by the legacy name for it, as macOS lists them
-(NSArray *)types {
   NSMutableArray *result=[NSMutableArray arrayWithCapacity:[_types count]*2];

   [self _pdSyncFromSystem];
   for(NSString *type in _types){
    NSString *legacy=PDLegacyType(type);

    [result addObject:type];
    if(legacy!=nil && ![_types containsObject:legacy])
     [result addObject:legacy];
   }
   return result;
}

-(NSString *)availableTypeFromArray:(NSArray *)types {
   [self _pdSyncFromSystem];
   for(NSString *check in types){
    if([_types containsObject:PDCanonicalType(check)])
     return check;
   }
   return nil;
}

-(NSData *)dataForType:(NSString *)type {
   NSData *data;

   [self _pdSyncFromSystem];
   type=PDCanonicalType(type);
   data=[_items objectForKey:type];

   /* A lazy owner supplies the bytes only when they are first asked for. */
   if(data==nil && _owner!=nil &&
      [_owner respondsToSelector:@selector(pasteboard:provideDataForType:)]){
    // owners may be asked for the legacy name they declared
    [_owner pasteboard:self provideDataForType:PDLegacyType(type)!=nil && ![_types containsObject:type]?PDLegacyType(type):type];
    data=[_items objectForKey:type];
   }

   // another client's data is read once and kept
   if(data==nil && _systemBacked && [_systemMimeTypes objectForKey:type]!=nil){
    size_t length=0;
    const char *mime=[[_systemMimeTypes objectForKey:type] UTF8String];
    void *bytes=_dragBacked?_windowServerReadDrag(mime,&length):_windowServerReadSelection(mime,&length);

    if(bytes!=NULL){
     data=[NSData dataWithBytesNoCopy:bytes length:length freeWhenDone:YES];
     [_items setObject:data forKey:type];
    }
   }
   return data;
}


-(NSString *)stringForType:(NSString *)type {
   NSData *data=[self dataForType:type];
   const unsigned char *bytes=[data bytes];
   NSStringEncoding encoding=NSUTF8StringEncoding;

   if(data==nil)
    return nil;
   // text is UTF-8 as on macOS, older writers put UTF-16 with a byte order mark
   if([data length]>=2 && ((bytes[0]==0xff && bytes[1]==0xfe) || (bytes[0]==0xfe && bytes[1]==0xff))){
    NSString *string=[[[NSString alloc] initWithData:data encoding:NSUnicodeStringEncoding] autorelease];
    if([string length]>0 && [string characterAtIndex:0]==0xfeff)
     string=[string substringFromIndex:1];
    return string;
   }
   return [[[NSString alloc] initWithData:data encoding:encoding] autorelease];
}

-(id)propertyListForType:(NSString *)type {
	NSData* data = [self dataForType: type];
	NSString* errorDesc = nil;
	id plist = [NSPropertyListSerialization propertyListFromData: data mutabilityOption: NSPropertyListImmutable format: NULL errorDescription: &errorDesc];
	if (plist && errorDesc == nil) {
		return plist;
	}
	NSLog(@"propertyListForType: produced error: %@", errorDesc);
	return nil;
}

// the 10.6 way to take ownership: no types and no owner until data is written
-(NSInteger)clearContents {
   return [self declareTypes:[NSArray array] owner:nil];
}

-(int)declareTypes:(NSArray *)types owner:(id)owner {
   if(_owner!=nil && _owner!=owner &&
      [_owner respondsToSelector:@selector(pasteboardChangedOwner:)])
    [_owner pasteboardChangedOwner:self];

   [_items removeAllObjects];
   [_types removeAllObjects];
   [_systemMimeTypes removeAllObjects];
   _owner=owner;
   _changeCount++;

   return [self addTypes:types owner:owner];
}

-(int)addTypes:(NSArray *)types owner:(id)owner {
   _owner=owner;

   for(NSString *type in types){
    type=PDCanonicalType(type);
    if(![_types containsObject:type])
     [_types addObject:type];
   }
   [self _pdSchedulePublish];

   return _changeCount;
}

-(BOOL)setData:(NSData *)data forType:(NSString *)type {
   if(data==nil || type==nil)
    return NO;

   type=PDCanonicalType(type);
   if(![_types containsObject:type])
    [_types addObject:type];

   [_items setObject:data forKey:type];
   [self _pdSchedulePublish];

   return YES;
}

-(BOOL)setString:(NSString *)string forType:(NSString *)type {
   NSData *data=[string dataUsingEncoding:NSUTF8StringEncoding];
   return [self setData:data forType:type];
}

-(BOOL)setPropertyList:(id)plist forType:(NSString *)type {
	NSString* errorDesc = nil;
	NSData* data = [NSPropertyListSerialization dataFromPropertyList: plist format: NSPropertyListXMLFormat_v1_0 errorDescription: &errorDesc];
	if (data && errorDesc == nil) {
		return [self setData: data forType: type]; 
	}
	NSLog(@"setPropertyList:forType: produced error: %@", errorDesc);
   return NO;
}

@end

NSString * const NSPasteboardNameFind=@"NSFindPboard";
NSString * const NSPasteboardTypePNG=@"public.png";
NSString * const NSPasteboardTypeSound=@"com.apple.cocoa.pasteboard.sound";
NSString * const NSPasteboardTypeMultipleTextSelection=@"com.apple.cocoa.pasteboard.multiple-text-selection";
NSString * const NSPasteboardTypeTextFinderOptions=@"com.apple.cocoa.pasteboard.find-panel-search-options";
NSString * const NSHTMLPboardType=@"Apple HTML pasteboard type";
