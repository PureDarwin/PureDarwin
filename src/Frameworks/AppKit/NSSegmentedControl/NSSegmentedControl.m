/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSSegmentedControl.h>
#import <AppKit/NSSegmentedCell.h>

@interface NSSegmentedCell (PrivateToControlView)
- (void)_wasDrawnWithFrame:(NSRect)cellFrame inView:(NSView *)controlView;
@end

@implementation NSSegmentedControl

// the control forwards its tracking mode to its cell, as on macOS since 10.10.3
-(NSSegmentSwitchTracking)trackingMode {
   return [(NSSegmentedCell *)[self cell] trackingMode];
}

-(void)setTrackingMode:(NSSegmentSwitchTracking)trackingMode {
   [(NSSegmentedCell *)[self cell] setTrackingMode:trackingMode];
}

// the convenience constructors macOS 10.12 added, one segment per label or image, sized to fit
+(instancetype)segmentedControlWithLabels:(NSArray *)labels trackingMode:(NSSegmentSwitchTracking)trackingMode target:(id)target action:(SEL)action {
   NSSegmentedControl *control=[[[self alloc] initWithFrame:NSZeroRect] autorelease];

   [control setSegmentCount:[labels count]];
   for(NSUInteger i=0;i<[labels count];i++)
    [control setLabel:[labels objectAtIndex:i] forSegment:i];
   [control setTrackingMode:trackingMode];
   [control setTarget:target];
   [control setAction:action];
   [control sizeToFit];
   return control;
}

+(instancetype)segmentedControlWithImages:(NSArray *)images trackingMode:(NSSegmentSwitchTracking)trackingMode target:(id)target action:(SEL)action {
   NSSegmentedControl *control=[[[self alloc] initWithFrame:NSZeroRect] autorelease];

   [control setSegmentCount:[images count]];
   for(NSUInteger i=0;i<[images count];i++)
    [control setImage:[images objectAtIndex:i] forSegment:i];
   [control setTrackingMode:trackingMode];
   [control setTarget:target];
   [control setAction:action];
   [control sizeToFit];
   return control;
}
+(Class)cellClass {
	return [NSSegmentedCell class];
}

-(NSInteger)segmentCount {
   return [_cell segmentCount];
}

-(NSSegmentStyle)segmentStyle {
   return [_cell segmentStyle];
}

-(NSInteger)tagForSegment:(NSInteger)segment {
   return [_cell tagForSegment:segment];
}

-(NSImage *)imageForSegment:(NSInteger)segment {
   return [_cell imageForSegment:segment];
}

-(BOOL)isEnabledForSegment:(NSInteger)segment {
   return [_cell isEnabledForSegment:segment];
}

-(NSString *)labelForSegment:(NSInteger)segment {
   return [_cell labelForSegment:segment];
}

-(NSMenu *)menuForSegment:(NSInteger)segment {
   return [_cell menuForSegment:segment];
}

-(NSString *)toolTipForSegment:(NSInteger)segment {
   return [_cell toolTipForSegment:segment];
}

-(CGFloat)widthForSegment:(NSInteger)segment {
   return [_cell widthForSegment:segment];
}

-(NSImageScaling)imageScalingForSegment:(NSInteger)segment {
   return [_cell imageScalingForSegment:segment];
}

-(NSInteger)selectedSegment {
   return [_cell selectedSegment];
}

-(BOOL)isSelectedForSegment:(NSInteger)segment {
   return [_cell isSelectedForSegment:segment];
}

-(void)setSegmentCount:(NSInteger)count {
   [_cell setSegmentCount:count];
}

-(void)setSegmentStyle:(NSSegmentStyle)value {
   [_cell setSegmentStyle:value];
   [self setNeedsDisplay:YES];
}

-(void)setTag:(NSInteger)tag forSegment:(NSInteger)segment {
   [_cell setTag:tag forSegment:segment];
}

-(void)setImage:(NSImage *)image forSegment:(NSInteger)segment {
   [_cell setImage:image forSegment:segment];
   [self setNeedsDisplay:YES];
}

-(void)setEnabled:(BOOL)enabled forSegment:(NSInteger)segment {
   [_cell setEnabled:enabled forSegment:segment];
   [self setNeedsDisplay:YES];
}

-(void)setLabel:(NSString *)label forSegment:(NSInteger)segment {
   [_cell setLabel:label forSegment:segment];
   [self setNeedsDisplay:YES];
}

-(void)setMenu:(NSMenu *)menu forSegment:(NSInteger)segment {
   [_cell setMenu:menu forSegment:segment];
}

-(void)setToolTip:(NSString *)string forSegment:(NSInteger)segment {
   [_cell setToolTip:string forSegment:segment];
}

-(void)setWidth:(CGFloat)width forSegment:(NSInteger)segment {
   [_cell setWidth:width forSegment:segment];
   [self setNeedsDisplay:YES];
}

-(void)setImageScaling:(NSImageScaling)value forSegment:(NSInteger)segment {
   [_cell setImageScaling:value forSegment:segment];
   [self setNeedsDisplay:YES];
}

-(BOOL)selectSegmentWithTag:(NSInteger)tag {
   BOOL result=[_cell selectSegmentWithTag:tag];

   [self setNeedsDisplay:YES];

   return result;
}

-(void)setSelected:(BOOL)flag forSegment:(NSInteger)segment {
   [_cell setSelected:flag forSegment:segment];
   [self setNeedsDisplay:YES];
}

-(void)setSelectedSegment:(NSInteger)segment {
   [_cell setSelectedSegment:segment];
   [self setNeedsDisplay:YES];
}

-(void)drawRect:(NSRect)rect {
   [super drawRect:rect];
   [_cell _wasDrawnWithFrame:rect inView:self];
}

@end

@implementation NSSegmentedControl (Bindings)
-(id)_cell
{
   return _cell;
}

-(id)_selectedLabel
{
   return [_cell labelForSegment:[_cell selectedSegment]];
}

-(void)_setSelectedLabel:(id)label
{
   int idx=[[_cell valueForKeyPath:@"segments.label"] indexOfObject:label];
   [_cell setSelectedSegment:idx];
	[self setNeedsDisplay:YES];
}

+(NSSet*)keyPathsForValuesAffectingSelectedLabel
{
   return [NSSet setWithObject:@"cell.selectedSegment"];
}

// selectedTag is implemented by NSControl - so no need for a fancy bindings version
-(NSInteger)selectedTag
{
	NSInteger selectedSegment = [_cell selectedSegment];
	NSInteger tag = -1;
	if (selectedSegment != -1) {
		tag = [_cell tagForSegment: selectedSegment];
	}
   return tag;
}

-(void)_setSelectedTag:(NSInteger)tag
{
   [_cell selectSegmentWithTag:tag];
	[self setNeedsDisplay:YES];
}

+(NSSet*)keyPathsForValuesAffectingSelectedTag {
   return [NSSet setWithObject:@"cell.selectedSegment"];
}

-(NSInteger)_selectedIndex
{
   return [_cell selectedSegment];
}

-(void)_setSelectedIndex:(NSInteger)idx
{
   [_cell setSelectedSegment:idx];
	[self setNeedsDisplay:YES];
}

+(NSSet*)keyPathsForValuesAffectingSelectedIndex {
   return [NSSet setWithObject:@"cell.selectedSegment"];
}
@end
