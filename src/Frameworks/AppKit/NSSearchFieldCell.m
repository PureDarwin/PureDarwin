/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSSearchFieldCell.h>
#import <AppKit/NSButtonCell.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSTextField.h>
#import <AppKit/NSControl.h>
#import <AppKit/NSTextField.h>
#import <AppKit/NSRaise.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/PDControlDrawing.h>

@implementation NSSearchFieldCell

-initWithCoder:(NSCoder *)coder {
   [super initWithCoder:coder];
   _searchButtonCell=[[NSButtonCell alloc] initImageCell:[NSImage imageNamed:@"NSSearchGlass"]];
   [_searchButtonCell setImageScaling:NSImageScaleProportionallyUpOrDown];
   _cancelButtonCell=[[NSButtonCell alloc] initImageCell:[NSImage imageNamed:@"NSStopProgressFreestandingTemplate"]];
   [_cancelButtonCell setImageScaling:NSImageScaleProportionallyUpOrDown];
   return self;  
}

-initTextCell:(NSString *)string {
   [super initTextCell:string];
   _searchButtonCell=[[NSButtonCell alloc] initImageCell:[NSImage imageNamed:@"NSSearchGlass"]];
   [_searchButtonCell setImageScaling:NSImageScaleProportionallyUpOrDown];
   _cancelButtonCell=[[NSButtonCell alloc] initImageCell:[NSImage imageNamed:@"NSStopProgressFreestandingTemplate"]];
   [_cancelButtonCell setImageScaling:NSImageScaleProportionallyUpOrDown];
   return self;
}

-(void)dealloc {
   [_searchButtonCell release];
   [_cancelButtonCell release];
   [super dealloc];
}

-(NSArray *)recentSearches {
   return _recentSearches;
}

-(NSString *)recentsAutosaveName {
   return _autosaveName;
}

-(int)maximumRecents {
   return _maximumRecents;
}

-(BOOL)sendsWholeSearchString {
   return _sendsWholeSearchString;
}

-(BOOL)sendsSearchStringImmediately {
   return _sendsSearchStringImmediately;
}

-(NSButtonCell *)searchButtonCell {
   return _searchButtonCell;
}

-(NSButtonCell *)cancelButtonCell {
   return _cancelButtonCell;
}

-(NSMenu *)searchMenuTemplate {
   return _searchMenuTemplate;
}

-(void)setRecentSearches:(NSArray *)searches {
   searches=[searches copy];
   [_recentSearches release];
   _recentSearches=searches;
}

-(void)setRecentsAutosaveName:(NSString *)name {
   name=[name copy];
   [_autosaveName release];
   _autosaveName=name;
}

-(void)setMaximumRecents:(int)value {
   _maximumRecents=value;
}

-(void)setSendsWholeSearchString:(BOOL)flag {
   _sendsWholeSearchString=flag;
}

-(void)setSendsSearchStringImmediately:(BOOL)flag {
   _sendsSearchStringImmediately=flag;
}

-(void)setSearchButtonCell:(NSButtonCell *)cell {
   cell=[cell retain];
   [_searchButtonCell release];
   _searchButtonCell=cell;
}

-(void)setCancelButtonCell:(NSButtonCell *)cell {
   cell=[cell retain];
   [_cancelButtonCell release];
   _cancelButtonCell=cell;
}

-(void)setSearchMenuTemplate:(NSMenu *)menu {
   menu=[menu retain];
   [_searchMenuTemplate release];
   _searchMenuTemplate=menu;
}

-(NSRect)searchTextRectForBounds:(NSRect)rect {
   return [self titleRectForBounds:rect];
}

// the magnifier sits six points in from the left, the cancel button five in from the right
-(NSRect)searchButtonRectForBounds:(NSRect)rect {
   return NSMakeRect(NSMinX(rect)+5,NSMinY(rect)+floor((rect.size.height-16)/2),16,16);
}

-(NSRect)cancelButtonRectForBounds:(NSRect)rect {
   return NSMakeRect(NSMaxX(rect)-20,NSMinY(rect)+floor((rect.size.height-14)/2),14,14);
}

-(void)resetCancelButtonCell {
   NSUnimplementedMethod();
}

-(void)resetSearchButtonCell {
   NSUnimplementedMethod();
}

-(NSRect)titleRectForBounds:(NSRect)rect {
   NSRect result=[super titleRectForBounds:rect];
   CGFloat left=NSMinX(rect)+26;

   result.size.width=NSMaxX(rect)-24-left;
   result.origin.x=left;
   return result;
}

-(NSRect)drawingRectForBounds:(NSRect)rect {
   return [self titleRectForBounds:rect];
}

-(BOOL)trackMouse:(NSEvent *)event inRect:(NSRect)frame ofView:(NSView *)view untilMouseUp:(BOOL)untilMouseUp {
   if([_cancelButtonCell trackMouse:event inRect:[self cancelButtonRectForBounds:frame] ofView:view untilMouseUp:YES]){
    [(NSControl *)view setStringValue:@""];

    if ([view respondsToSelector:@selector(selectText:)])
      [(NSTextField *)view selectText:nil];
    return YES;
   }

   return [super trackMouse:event inRect:frame ofView:view untilMouseUp:untilMouseUp];
}

// the cells this class made draw as paths, the artwork may be missing or a PDF that does not render
static BOOL hasDefaultImage(NSButtonCell *cell,NSString *name) {
   return cell!=nil && ([cell image]==nil || [cell image]==[NSImage imageNamed:name]);
}

-(void)drawWithFrame:(NSRect)frame inView:(NSView *)control {
   PDDrawContext c=PDCurrentDrawContext(control);
   NSColor *background=[self drawsBackground] ? [self backgroundColor] : nil;
   NSRect glass=[self searchButtonRectForBounds:frame];
   NSRect cancel=[self cancelButtonRectForBounds:frame];

   _controlView=control;
   PDDrawTextFieldBezel(c,frame,background,6);
   [self drawInteriorWithFrame:frame inView:control];

   if(hasDefaultImage(_searchButtonCell,@"NSSearchGlass"))
    PDDrawSearchGlass(c,NSInsetRect(glass,1.5,1.5),[self isEnabled]);
   else
    [_searchButtonCell drawWithFrame:glass inView:control];
   if([[self stringValue] length]>0){
    if(hasDefaultImage(_cancelButtonCell,@"NSStopProgressFreestandingTemplate"))
     PDDrawCancelButton(c,cancel);
    else
     [_cancelButtonCell drawWithFrame:cancel inView:control];
   }
}

@end
