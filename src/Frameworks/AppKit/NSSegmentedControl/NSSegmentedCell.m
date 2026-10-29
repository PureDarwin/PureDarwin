/* Copyright (c) 2006-2007 Christopher J. W. Lloyd, 2008 Johannes Fortmann

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <sys/param.h>
#import <AppKit/NSSegmentedCell.h>
#import <AppKit/NSRaise.h>
#import "NSSegmentItem.h"
#import <AppKit/NSButtonCell.h>
#import <AppKit/NSPopUpWindow.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSStringDrawer.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSAppearance.h>
#import <AppKit/PDControlDrawing.h>

@interface NSSegmentedCell (Private)
- (void)_recomputeSegmentWidths;
@end

@implementation NSSegmentedCell

// For manually created segment cells
- (id)initImageCell:(NSImage *)image {
   if ((self = [super initImageCell:image])) {
      _segments = [[NSMutableArray arrayWithCapacity:5] retain];
      _selectedSegment = NSNotFound; // initially empty
      _trackingMode = NSSegmentSwitchTrackingSelectOne; // default
   }
   return self;
}

- (id)initTextCell:(NSString *)str {
    return [self initImageCell:nil];
}

// For nib created segment cells
- (id)initWithCoder:(NSCoder *)coder {
   [super initWithCoder:coder];

   _segments = [[coder decodeObjectForKey:@"NSSegmentImages"] retain];
   if ( !_segments)
    _segments = [[NSMutableArray arrayWithCapacity:5] retain];

   if ([coder containsValueForKey:@"NSSelectedSegment"])
    _selectedSegment = [coder decodeIntForKey:@"NSSelectedSegment"];
   else
    _selectedSegment = NSNotFound;

   _trackingMode = [coder decodeIntForKey:@"NSTrackingMode"];

   [self _recomputeSegmentWidths];

   return self;
}

-(void)dealloc {
   [_segments release];
   [_segmentComputedWidths release];
   [super dealloc];
}

-(NSInteger)segmentCount {
   return [_segments count];
}

-(NSSegmentStyle)segmentStyle {
   return _style;
}

-(NSSegmentSwitchTracking)trackingMode {
   return _trackingMode;
}

-(NSInteger)tagForSegment:(NSInteger)segment {
   return [[_segments objectAtIndex:segment] tag];
}

-(NSImage *)imageForSegment:(NSInteger)segment {
   return [[_segments objectAtIndex:segment] image];
}

-(BOOL)isEnabledForSegment:(NSInteger)segment {
   return [[_segments objectAtIndex:segment] isEnabled];
}

-(NSString *)labelForSegment:(NSInteger)segment {
   return [[_segments objectAtIndex:segment] label];
}

-(NSMenu *)menuForSegment:(NSInteger)segment {
   return [[_segments objectAtIndex:segment] menu];
}

-(NSString *)toolTipForSegment:(NSInteger)segment {
   return [[_segments objectAtIndex:segment] toolTip];
}

-(CGFloat)widthForSegment:(NSInteger)segment {
   return [[_segments objectAtIndex:segment] width];
}

-(NSImageScaling)imageScalingForSegment:(NSInteger)segment {
   return [[_segments objectAtIndex:segment] imageScaling];
}

-(NSInteger)selectedSegment {

   if(_selectedSegment==NSNotFound) {
      int i,count=[_segments count];

      for(i=0;i<count;i++)
         if([[_segments objectAtIndex:i] isSelected])
            return i;
   }

   return _selectedSegment;
}

-(BOOL)isSelectedForSegment:(NSInteger)segment {
   return [[_segments objectAtIndex:segment] isSelected];
}

-(void)setSegmentCount:(NSInteger)count {
   int currentCount = [_segments count];
   if (count == currentCount) return;
   if (count > currentCount) {
      while ([_segments count] < count) {
         NSSegmentItem* item = [[[NSSegmentItem alloc] init] autorelease];
         [_segments addObject: item];
      }
   } else {
      while ([_segments count] > count) {
         [_segments removeLastObject];
      }
   }
    [self _recomputeSegmentWidths];
}

-(void)setSegmentStyle:(NSSegmentStyle)value {
   _style=value;
}

-(void)setTrackingMode:(NSSegmentSwitchTracking)trackingMode {
   _trackingMode=trackingMode;
}

-(void)setTag:(NSInteger)tag forSegment:(NSInteger)segment {
   [[_segments objectAtIndex:segment] setTag:tag];
}

-(void)setImage:(NSImage *)image forSegment:(NSInteger)segment {
   [[_segments objectAtIndex:segment] setImage:image];
   [self _recomputeSegmentWidths];
}

-(void)setEnabled:(BOOL)enabled forSegment:(NSInteger)segment {
   [[_segments objectAtIndex:segment] setEnabled:enabled];
}

-(void)setLabel:(NSString *)label forSegment:(NSInteger)segment {
   [[_segments objectAtIndex:segment] setLabel:label];
   [self _recomputeSegmentWidths];
}

-(void)setMenu:(NSMenu *)menu forSegment:(NSInteger)segment {
   [[_segments objectAtIndex:segment] setMenu:menu];
}

-(void)setToolTip:(NSString *)string forSegment:(NSInteger)segment {
   [[_segments objectAtIndex:segment] setToolTip:string];
}

-(void)setWidth:(CGFloat)width forSegment:(NSInteger)segment {
   [[_segments objectAtIndex:segment] setWidth:width];
   [self _recomputeSegmentWidths];
}

-(void)setImageScaling:(NSImageScaling)value forSegment:(NSInteger)segment {
   [[_segments objectAtIndex:segment] setImageScaling:value];
}

-(BOOL)selectSegmentWithTag:(NSInteger)tag {
   int i,count=[_segments count];
   for(i=0;i<count;i++)
    if([[_segments objectAtIndex:i] tag]==tag){
       [self setSelectedSegment:i];
     return YES;
    }
   return NO;
}

-(void)setSelected:(BOOL)flag forSegment:(NSInteger)segment {
   [self willChangeValueForKey:@"selectedSegment"];
   _selectedSegment=NSNotFound;  // will be recomputed in -selectedSegment
   [[_segments objectAtIndex:segment] setSelected:flag];
   [self didChangeValueForKey:@"selectedSegment"];
}

-(void)setSelectedSegment:(NSInteger)segment {
   for(NSSegmentItem *item in _segments)
    [item setSelected:NO];

   _selectedSegment=segment;
   [[_segments objectAtIndex:segment] setSelected:YES];
}

-(void)makeNextSegmentKey {
   NSUnimplementedMethod();
}
-(void)makePreviousSegmentKey {
   NSUnimplementedMethod();
}

-(void)setFont:(NSFont *)font {
   [super setFont:font];
   [self _recomputeSegmentWidths];
}

-(NSButtonCell *)_cellForSegment:(NSInteger)idx
{
    NSSegmentItem *segment=[_segments objectAtIndex:idx];
    NSButtonCell *cell=[[NSButtonCell new] autorelease];
    NSString *label=[segment label];
    [cell setTitle:label];
    [cell setFont:[self font]];
    [cell setControlSize:[self controlSize]];

    [cell setHighlighted:[segment isSelected]];

   // The control is enabled/disabled as a whole
    [cell setEnabled:[self isEnabled]];

    NSImage *image=[segment image];
    if(image)
    {
        [cell setImage:image];
        // a button cell is created as a text cell - so we need to say that we want
        // image dimming
        [cell setImageDimsWhenDisabled: YES];

        if([label length])
        {
            [cell setImagePosition:NSImageLeft];
        }
        else
        {
            // center if no label
            [cell setImagePosition:NSImageOnly];
        }
        [cell setImageScaling:[segment imageScaling]];
    } else {
        [cell setImagePosition:NSNoImage];
    }

    [cell setLineBreakMode:NSLineBreakByTruncatingTail];

    return cell;
}


-(NSFont *)_segmentFont {
   return [self font]?:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:[self controlSize]]];
}

// a segment is its label or image plus 14 points, or the width it was given
-(CGFloat)_naturalWidthOfSegment:(NSInteger)idx {
   NSSegmentItem *segment=[_segments objectAtIndex:idx];
   NSString *label=[segment label];
   NSImage *image=[segment image];
   CGFloat w=[segment width];

   if(w>0)
    return w;
   if([label length])
    w=ceil([label sizeWithAttributes:[NSDictionary dictionaryWithObject:[self _segmentFont] forKey:NSFontAttributeName]].width);
   if(image!=nil)
    w+=[image size].width+([label length] ? 4 : 0);
   return w+14;
}

// the drawn widths, the outer two include the track's inset so hit testing can walk them from the frame's edge
-(void)_layoutSegmentsInWidth:(CGFloat)frameWidth {
   int i,count=[_segments count];
   CGFloat natural=0,extra;

   if (!_segmentComputedWidths)
    _segmentComputedWidths = [[NSMutableArray arrayWithCapacity:5] retain];
   else
    [_segmentComputedWidths removeAllObjects];
   for(i=0;i<count;i++)
    natural+=[self _naturalWidthOfSegment:i];
   extra=(frameWidth>0 && count>0) ? MAX(0,(frameWidth-6-natural)/count) : 0;
   for(i=0;i<count;i++){
    CGFloat w=[self _naturalWidthOfSegment:i]+extra;

    if(i==0)
     w+=3;
    if(i==count-1)
     w+=3;
    [_segmentComputedWidths addObject:[NSNumber numberWithDouble:w]];
   }
}

-(void)_recomputeSegmentWidths {
   [self _layoutSegmentsInWidth:_lastDrawRect.size.width];
}

-(void)drawSegment:(NSInteger)idx inFrame:(NSRect)frame withView:(NSView *)view {
   NSSegmentItem *segment=[_segments objectAtIndex:idx];
   PDDrawContext c=PDCurrentDrawContext(view);
   NSString *label=[segment label];
   NSImage *image=[segment image];
   BOOL selected=[segment isSelected];
   BOOL enabled=[self isEnabled] && [segment isEnabled];
   NSRect body=PDRectFromTop(frame,c.flipped,1,frame.size.height-2);

   if(selected && [self trackingMode]!=NSSegmentSwitchTrackingMomentary)
    PDDrawSegmentSelection(c,body);
   if(image!=nil){
    NSSize size=[image size];
    CGFloat lw=[label length] ? ceil([label sizeWithAttributes:[NSDictionary dictionaryWithObject:[self _segmentFont] forKey:NSFontAttributeName]].width)+4 : 0;
    NSRect r=NSMakeRect(floor(NSMidX(frame)-(size.width+lw)/2),floor(NSMidY(frame)-size.height/2),size.width,size.height);

    [image drawInRect:r fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:enabled ? 1 : 0.4 respectFlipped:YES hints:nil];
    frame.origin.x+=size.width/2+2;
   }
   if([label length]){
    NSFont *font=[self _segmentFont];
    NSColor *color=enabled ? [NSColor controlTextColor] : [NSColor disabledControlTextColor];
    NSDictionary *attributes=[NSDictionary dictionaryWithObjectsAndKeys:font,NSFontAttributeName,color,NSForegroundColorAttributeName,nil];
    NSSize size=[label sizeWithAttributes:attributes];
    CGFloat cap=[font capHeight]>0 ? [font capHeight] : [font ascender]*0.72;
    CGFloat baseline=c.flipped ? round(NSMidY(body)+cap/2+0.5) : round(NSMidY(body)-cap/2-0.5);
    NSRect r;

    r.size=NSMakeSize(MIN(size.width,frame.size.width-4),ceil([font ascender]-[font descender]));
    r.origin.x=floor(NSMidX(frame)-r.size.width/2);
    r.origin.y=c.flipped ? baseline-[font ascender] : baseline+[font ascender]-r.size.height;
    [label _clipAndDrawInRect:r withAttributes:attributes];
   }
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
   PDDrawContext c=PDCurrentDrawContext(controlView);
   int i=0, count=[self segmentCount];
   NSRect track=PDRectFromTop(NSInsetRect(cellFrame,2,0),c.flipped,floor((cellFrame.size.height-24)/2)+1,22);
   CGFloat x=NSMinX(cellFrame);
   NSInteger selected=NSNotFound;

   _lastDrawRect=cellFrame;
   [self _layoutSegmentsInWidth:cellFrame.size.width];
   PDDrawSegmentTrack(c,track);

   for(i=0; i<count; i++)
    if([[_segments objectAtIndex:i] isSelected])
     selected=i;
   for(i=0; i<count; i++) {
      CGFloat w=[[_segmentComputedWidths objectAtIndex:i] doubleValue];
      NSRect segmentFrame=NSMakeRect(x,track.origin.y,w,track.size.height);

      if(i==0){
       segmentFrame.origin.x+=3;
       segmentFrame.size.width-=3;
      }
      if(i==count-1)
       segmentFrame.size.width-=3;
      // dividers sit between two segments that are not selected
      if(i>0 && i!=selected && i-1!=selected)
       PDDrawSegmentDivider(c,track,x);
      [self drawSegment:i inFrame:segmentFrame withView:controlView];
      x+=w;
   }
}

-(NSSize)cellSize
{
    CGFloat width=6;
    int i;

    for (i = 0; i < [self segmentCount]; ++i)
        width+=[self _naturalWidthOfSegment:i];
    return NSMakeSize(ceil(width),24);
}


// updating this value in -drawWithFrame is not enough, because a subclass might have replaced it entirely.
// hence NSSegmentedControl will call this private method as well.
- (void)_wasDrawnWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
   _lastDrawRect=cellFrame;
}

-(NSInteger)_segmentForPoint:(NSPoint)point {
   int i=0, count=[self segmentCount];
   NSRect segmentFrame=_lastDrawRect;
   if (segmentFrame.size.height <= 0.0)
    segmentFrame.size.height = 100;

   if ([_segmentComputedWidths count]!=count)
    [self _recomputeSegmentWidths];

   for(i=0; i<count; i++) {
      NSSegmentItem *item=[_segments objectAtIndex:i];

      segmentFrame.size.width=[[_segmentComputedWidths objectAtIndex:i] doubleValue];
      if(NSPointInRect(point, segmentFrame)) {
         return i;
      }
      segmentFrame.origin.x+=segmentFrame.size.width;
   }
   return NSNotFound;
}

- (BOOL)startTrackingAt:(NSPoint)startPoint inView:(NSView *)controlView {
   // save the segment clicked on and its state
   _firstTrackingSegmentIndex=[self _segmentForPoint:[controlView convertPoint:startPoint fromView:nil]];
   NSSegmentItem *trackingItem=[_segments objectAtIndex:_firstTrackingSegmentIndex];
   if(![trackingItem isEnabled])
      return YES;

   _firstTrackingSegmentInitialState=[trackingItem isSelected];
   [trackingItem setSelected:!_firstTrackingSegmentInitialState];

   [self continueTracking:startPoint at:startPoint inView:controlView];
   return YES;
}

- (BOOL)continueTracking:(NSPoint)lastPoint at:(NSPoint)currentPoint inView:(NSView *)controlView {
   currentPoint=[controlView convertPoint:currentPoint fromView:nil];
   lastPoint=[controlView convertPoint:lastPoint fromView:nil];

   NSSegmentItem *trackingItem=[_segments objectAtIndex:_firstTrackingSegmentIndex];
   if(![trackingItem isEnabled])
      return YES;

   int currentSegmentIdx=[self _segmentForPoint:currentPoint];

   // change segments state depending on if inside or outside
   if(currentSegmentIdx==_firstTrackingSegmentIndex)  // we're inside, so switch state relative to initial state
   {
      [trackingItem setSelected:!_firstTrackingSegmentInitialState];
   }
   else  // we're outside, so state is initial state
   {
      [trackingItem setSelected:_firstTrackingSegmentInitialState];
   }

   [controlView setNeedsDisplayInRect:_lastDrawRect];

   return YES;
}

- (void)stopTracking:(NSPoint)lastPoint at:(NSPoint)stopPoint inView:(NSView *)controlView mouseIsUp:(BOOL)flag {
   [self continueTracking:lastPoint at:stopPoint inView:controlView];

   NSSegmentItem *trackingItem=[_segments objectAtIndex:_firstTrackingSegmentIndex];
   if(![trackingItem isEnabled])
      return;

   [self willChangeValueForKey:@"selectedSegment"];
   _selectedSegment=NSNotFound;
   // if segment is still switched, it'll be the new "selected segment"
   if([trackingItem isSelected]!=_firstTrackingSegmentInitialState)
   {
      _selectedSegment=_firstTrackingSegmentIndex;
   }

   // adapt other segments to fit tracking model
   switch(_trackingMode)
   {
      case NSSegmentSwitchTrackingMomentary:
      {
         // deactivate all segments
         for(NSSegmentItem *item in _segments)
         {
            [item setSelected:NO];
         }
         break;
      }
      case NSSegmentSwitchTrackingSelectAny:
      {
         // nothing
         break;
      }
      case NSSegmentSwitchTrackingSelectOne:
      {
         if(_selectedSegment!=NSNotFound)
         {
            for(NSSegmentItem *item in _segments)
            {
               [item setSelected:NO];
            }
            // select only the selected item
            [[_segments objectAtIndex:_selectedSegment] setSelected:YES];
         }
         break;
      }
   }
   [self didChangeValueForKey:@"selectedSegment"];

}

-(BOOL)trackMouse:(NSEvent *)event inRect:(NSRect)cellFrame ofView:(NSView *)controlView untilMouseUp:(BOOL)flag {
   NSPoint        startPoint=[event locationInWindow];
   NSInteger      segmentUnderMouse=[self _segmentForPoint:[controlView convertPoint:startPoint fromView:nil]];

   if(segmentUnderMouse==NSNotFound)
    return YES;

   NSSegmentItem *trackingItem=[_segments objectAtIndex:segmentUnderMouse];

   if(![trackingItem isEnabled])
      return YES;

   NSMenu *menu=[trackingItem menu];

   if(menu==nil)
    return [super trackMouse:event inRect:cellFrame ofView:controlView untilMouseUp:flag];

   [self setSelectedSegment:segmentUnderMouse];

   [NSMenu popUpContextMenu:menu withEvent:event forView:controlView];

   return YES;
}

@end

@implementation NSSegmentedCell (Bindings)

-(id)_replacementKeyPathForBinding:(id)binding {
    if([binding isEqual:@"selectedIndex"])
      return @"selectedSegment";
    return [super _replacementKeyPathForBinding:binding];
}

@end
