// PDControlDrawing.m - draws the macOS 15 look of the standard controls with CoreGraphics paths
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// the colours and offsets are measured from macOS 15 renderings of the same cells (src/Tools/uigallery)

#import "PDControlDrawing.h"
#import <AppKit/NSAppearance.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSView.h>
#import <AppKit/NSWindow.h>
#import <math.h>

BOOL PDContextIsFlipped(void) {
   return [[NSGraphicsContext currentContext] isFlipped];
}

PDDrawContext PDCurrentDrawContext(NSView *controlView) {
   PDDrawContext c;
   NSWindow *window=[controlView window];

   c.flipped=PDContextIsFlipped();
   c.dark=[[NSAppearance currentDrawingAppearance] _isDark];
   // a window that is not key draws its controls in grey, a cell outside any window draws as active
   c.active=(window==nil || [window isKeyWindow] || [window isMainWindow]);
   return c;
}

NSRect PDRectFromTop(NSRect frame, BOOL flipped, CGFloat dy, CGFloat height) {
   NSRect r=frame;

   r.size.height=height;
   if(flipped)
    r.origin.y=NSMinY(frame)+dy;
   else
    r.origin.y=NSMaxY(frame)-dy-height;
   return r;
}

NSColor *PDAccentColor(PDDrawContext c) {
   if(!c.active)
    return c.dark ? [NSColor colorWithCalibratedWhite:0.42 alpha:1] : [NSColor colorWithCalibratedWhite:0.72 alpha:1];
   return [[NSColor controlAccentColor] colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
}

static CGContextRef port(void) {
   return [[NSGraphicsContext currentContext] graphicsPort];
}

static void addRoundedRect(CGContextRef ctx, CGRect r, CGFloat radius) {
   radius=MIN(radius,MIN(r.size.width,r.size.height)/2);
   if(radius<=0){
    CGContextAddRect(ctx,r);
    return;
   }
   CGContextMoveToPoint(ctx,CGRectGetMinX(r)+radius,CGRectGetMinY(r));
   CGContextAddArcToPoint(ctx,CGRectGetMaxX(r),CGRectGetMinY(r),CGRectGetMaxX(r),CGRectGetMaxY(r),radius);
   CGContextAddArcToPoint(ctx,CGRectGetMaxX(r),CGRectGetMaxY(r),CGRectGetMinX(r),CGRectGetMaxY(r),radius);
   CGContextAddArcToPoint(ctx,CGRectGetMinX(r),CGRectGetMaxY(r),CGRectGetMinX(r),CGRectGetMinY(r),radius);
   CGContextAddArcToPoint(ctx,CGRectGetMinX(r),CGRectGetMinY(r),CGRectGetMaxX(r),CGRectGetMinY(r),radius);
   CGContextClosePath(ctx);
}

static void fillRounded(CGRect r, CGFloat radius, CGFloat red, CGFloat green, CGFloat blue, CGFloat alpha) {
   CGContextRef ctx=port();

   CGContextBeginPath(ctx);
   addRoundedRect(ctx,r,radius);
   CGContextSetRGBFillColor(ctx,red,green,blue,alpha);
   CGContextFillPath(ctx);
}

static void fillGrey(CGRect r, CGFloat radius, CGFloat white, CGFloat alpha) {
   fillRounded(r,radius,white,white,white,alpha);
}

// a vertical gradient from the visual top colour to the visual bottom colour
// the shape is filled once for its antialiased edge, then one band per point inside the corners
static void fillRoundedGradient(PDDrawContext c, CGRect r, CGFloat radius, const CGFloat top[3], const CGFloat bottom[3]) {
   CGContextRef ctx=port();
   int i,rows=(int)ceil(r.size.height);
   CGFloat mid[3];

   radius=MIN(radius,MIN(r.size.width,r.size.height)/2);
   for(i=0;i<3;i++)
    mid[i]=(top[i]+bottom[i])/2;
   fillRounded(r,radius,mid[0],mid[1],mid[2],1);
   for(i=0;i<rows;i++){
    CGFloat t=(rows>1) ? (CGFloat)i/(rows-1) : 0;
    CGFloat y=i+0.5;
    CGFloat d=MIN(y,r.size.height-y);
    CGFloat inset=0.75;

    // how far the corner arc sits in from the side at this row
    if(d<radius)
     inset+=radius-sqrt(MAX(0,radius*radius-(radius-d)*(radius-d)));
    if(inset*2>=r.size.width)
     continue;
    CGContextSetRGBFillColor(ctx,top[0]+(bottom[0]-top[0])*t,top[1]+(bottom[1]-top[1])*t,top[2]+(bottom[2]-top[2])*t,1);
    CGContextFillRect(ctx,CGRectMake(r.origin.x+inset,c.flipped ? r.origin.y+i : CGRectGetMaxY(r)-i-1,r.size.width-2*inset,MIN(1,r.size.height-i)));
   }
}

// moves a rect by dy towards the visual bottom
static CGRect down(PDDrawContext c, CGRect r, CGFloat dy) {
   r.origin.y+=c.flipped ? dy : -dy;
   return r;
}

static void accentGradient(PDDrawContext c, CGFloat top[3], CGFloat bottom[3]) {
   NSColor *accent=PDAccentColor(c);
   CGFloat r=[accent redComponent],g=[accent greenComponent],b=[accent blueComponent];

   // the default blue runs from 2891ff at the top to 017bff at the bottom
   top[0]=r+(1-r)*0.15; top[1]=g+(1-g)*0.15; top[2]=b+(1-b)*0.15;
   bottom[0]=r; bottom[1]=g; bottom[2]=b;
}

void PDDrawPushBezel(PDDrawContext c, NSRect bezel, BOOL defaulted, BOOL pressed, BOOL enabled) {
   CGRect outer=PDRectFromTop(bezel,c.flipped,0,bezel.size.height-1);
   CGRect shadow=down(c,NSInsetRect(outer,1,0),1);
   CGRect body=PDRectFromTop(NSInsetRect(outer,1,0),c.flipped,1,outer.size.height-1);
   CGFloat radius=6;

   if(c.dark){
    fillGrey(down(c,outer,0.5),radius,0,enabled ? 0.25 : 0.12);
    if(defaulted && enabled){
     CGFloat top[3],bottom[3];

     accentGradient(c,top,bottom);
     fillRoundedGradient(c,body,radius-1,top,bottom);
    }
    else
     fillGrey(body,radius-1,1,enabled ? (pressed ? 0.36 : 0.25) : 0.12);
    return;
   }

   if(defaulted && enabled){
    CGFloat top[3],bottom[3];

    fillRounded(shadow,radius,0,0.35,0.75,0.25);
    fillRounded(outer,radius,0,0.25,0.6,0.06);
    accentGradient(c,top,bottom);
    if(pressed){
     int i;

     for(i=0;i<3;i++){
      top[i]*=0.85;
      bottom[i]*=0.85;
     }
    }
    fillRoundedGradient(c,body,radius-1,top,bottom);
    return;
   }
   fillGrey(down(c,shadow,1),radius,0,0.04);
   fillGrey(shadow,radius,0,enabled ? 0.25 : 0.12);
   fillGrey(outer,radius,0,enabled ? 0.08 : 0.05);
   fillGrey(body,radius-1,enabled ? (pressed ? 0.94 : 1.0) : 0.965,1);
}

void PDDrawSquareBezel(PDDrawContext c, NSRect bezel, BOOL pressed, BOOL enabled) {
   PDDrawPushBezel(c,bezel,NO,pressed,enabled);
}

static void strokePath(CGContextRef ctx, CGFloat width, CGFloat red, CGFloat green, CGFloat blue, CGFloat alpha) {
   CGContextSetRGBStrokeColor(ctx,red,green,blue,alpha);
   CGContextSetLineWidth(ctx,width);
   CGContextSetLineCap(ctx,kCGLineCapRound);
   CGContextSetLineJoin(ctx,kCGLineJoinRound);
   CGContextStrokePath(ctx);
}

// a point given from the visual top left of box
static CGPoint at(PDDrawContext c, NSRect box, CGFloat x, CGFloat y) {
   return CGPointMake(NSMinX(box)+x,c.flipped ? NSMinY(box)+y : NSMaxY(box)-y);
}

// the off state, a white well with a hairline that is darker along the top
static void drawToggleWell(PDDrawContext c, NSRect box, CGFloat radius, BOOL enabled) {
   CGRect top=box;

   if(c.dark){
    fillGrey(box,radius,1,enabled ? 0.16 : 0.08);
    fillGrey(NSInsetRect(box,1,1),radius-1,0,0.35);
    fillGrey(NSInsetRect(box,1,1),radius-1,1,enabled ? 0.12 : 0.05);
    return;
   }
   fillGrey(down(c,box,0.5),radius,0,enabled ? 0.06 : 0.03);
   fillGrey(box,radius,0,enabled ? 0.12 : 0.08);
   top.size.height=radius;
   if(!c.flipped)
    top.origin.y=NSMaxY(box)-radius;
   CGContextSaveGState(port());
   CGContextClipToRect(port(),top);
   fillGrey(box,radius,0,enabled ? 0.09 : 0.02);
   CGContextRestoreGState(port());
   fillGrey(NSInsetRect(box,1,1),radius-1,enabled ? 1.0 : 0.965,1);
}

// the on state, the accent gradient with a soft shadow under it
static void drawToggleFill(PDDrawContext c, NSRect box, CGFloat radius, BOOL enabled, BOOL pressed) {
   CGFloat top[3],bottom[3];
   int i;

   if(!enabled){
    drawToggleWell(c,box,radius,NO);
    return;
   }
   accentGradient(c,top,bottom);
   if(pressed){
    for(i=0;i<3;i++){
     top[i]*=0.85;
     bottom[i]*=0.85;
    }
   }
   fillRounded(down(c,box,1),radius,0,0.35,0.75,c.dark ? 0 : 0.22);
   fillRoundedGradient(c,box,radius,top,bottom);
}

void PDDrawCheckBox(PDDrawContext c, NSRect box, NSControlStateValue state, BOOL enabled, BOOL pressed) {
   CGContextRef ctx=port();
   CGFloat s=box.size.width/14;
   CGFloat radius=3.5*s;
   CGFloat mark=enabled ? 1 : (c.dark ? 0.45 : 0.72);

   if(state==NSOffState){
    drawToggleWell(c,box,radius,enabled);
    return;
   }
   // an inactive window keeps the white well and draws the mark dark
   if(!c.active && enabled){
    drawToggleWell(c,box,radius,YES);
    mark=c.dark ? 0.85 : 0.15;
   }
   else
    drawToggleFill(c,box,radius,enabled,pressed);
   CGContextBeginPath(ctx);
   if(state==NSMixedState){
    CGContextMoveToPoint(ctx,at(c,box,3.5*s,7*s).x,at(c,box,3.5*s,7*s).y);
    CGContextAddLineToPoint(ctx,at(c,box,10.5*s,7*s).x,at(c,box,10.5*s,7*s).y);
    strokePath(ctx,2*s,mark,mark,mark,1);
   }
   else {
    CGPoint a=at(c,box,3.6*s,7.6*s),b=at(c,box,6*s,10.6*s),e=at(c,box,10.6*s,3.6*s);

    CGContextMoveToPoint(ctx,a.x,a.y);
    CGContextAddLineToPoint(ctx,b.x,b.y);
    CGContextAddLineToPoint(ctx,e.x,e.y);
    strokePath(ctx,1.7*s,mark,mark,mark,1);
   }
}

void PDDrawRadioButton(PDDrawContext c, NSRect box, BOOL on, BOOL enabled, BOOL pressed) {
   CGFloat radius=box.size.width/2;
   CGFloat dot=box.size.width*6/14;
   CGFloat mark=enabled ? 1 : (c.dark ? 0.45 : 0.72);

   if(!on){
    drawToggleWell(c,box,radius,enabled);
    return;
   }
   if(!c.active && enabled){
    drawToggleWell(c,box,radius,YES);
    mark=c.dark ? 0.85 : 0.15;
    dot=box.size.width*5/14;
   }
   else
    drawToggleFill(c,box,radius,enabled,pressed);
   fillGrey(NSInsetRect(box,(box.size.width-dot)/2,(box.size.height-dot)/2),dot/2,mark,1);
}

// a chevron in a 7 by 4 box pointing up or down, centred on (x,y)
static void chevron(PDDrawContext c, CGFloat x, CGFloat y, BOOL up, CGFloat width, CGFloat height, CGFloat line, CGFloat white, CGFloat alpha) {
   CGContextRef ctx=port();
   CGFloat dir=(up ? -1 : 1)*(c.flipped ? 1 : -1);

   CGContextBeginPath(ctx);
   CGContextMoveToPoint(ctx,x-width/2,y-dir*height/2);
   CGContextAddLineToPoint(ctx,x,y+dir*height/2);
   CGContextAddLineToPoint(ctx,x+width/2,y-dir*height/2);
   strokePath(ctx,line,white,white,white,alpha);
}

void PDDrawArrowBox(PDDrawContext c, NSRect box, BOOL upAndDown, BOOL enabled) {
   CGFloat top[3],bottom[3];
   CGFloat cx=NSMidX(box),cy=NSMidY(box);
   BOOL tinted=c.active && enabled;
   CGFloat ink=tinted ? 1 : (c.dark ? 0.85 : 0.2);

   if(tinted){
    accentGradient(c,top,bottom);
    fillRoundedGradient(c,box,4,top,bottom);
   }
   else if(!upAndDown){
    // the untinted combo button is an 18 point white square with a grey ring and a shadow below
    NSRect ring=NSInsetRect(box,-1,-1);

    fillGrey(down(c,ring,1),5,0,c.dark ? 0.3 : 0.08);
    fillGrey(ring,5,c.dark ? 1 : 0,c.dark ? 0.18 : 0.065);
    fillGrey(NSInsetRect(ring,1,1),4,c.dark ? 0.4 : 1,1);
   }
   if(upAndDown){
    chevron(c,cx,cy-3.5*(c.flipped ? 1 : -1),YES,6,3,1.4,ink,enabled ? 1 : 0.4);
    chevron(c,cx,cy+3.5*(c.flipped ? 1 : -1),NO,6,3,1.4,ink,enabled ? 1 : 0.4);
   }
   else
    chevron(c,cx,cy+1*(c.flipped ? 1 : -1),NO,6.5,3.5,1.4,ink,enabled ? 1 : 0.4);
}

void PDDrawTextFieldBezel(PDDrawContext c, NSRect frame, NSColor *background, CGFloat radius) {
   CGRect inner=NSInsetRect(frame,1,1);
   CGRect top=frame,bottom=frame;

   top.size.height=1;
   bottom.size.height=1;
   if(c.flipped)
    bottom.origin.y=NSMaxY(frame)-1;
   else
    top.origin.y=NSMaxY(frame)-1;

   if(c.dark){
    fillGrey(frame,radius,1,0.14);
    fillGrey(inner,MAX(0,radius-1),0.196,1);
    fillGrey(inner,MAX(0,radius-1),1,0.05);
   }
   else {
    // sides and top d9d9d9, the bottom edge b4b4b4
    fillGrey(frame,radius,0,0.08);
    CGContextSaveGState(port());
    CGContextClipToRect(port(),bottom);
    fillGrey(frame,radius,0,0.17);
    CGContextRestoreGState(port());
    fillGrey(inner,MAX(0,radius-1),1,1);
   }
   if(background!=nil && ![background isEqual:[NSColor textBackgroundColor]]){
    CGContextSaveGState(port());
    [background setFill];
    CGContextBeginPath(port());
    addRoundedRect(port(),inner,MAX(0,radius-1));
    CGContextFillPath(port());
    CGContextRestoreGState(port());
   }
}

void PDDrawSearchGlass(PDDrawContext c, NSRect rect, BOOL enabled) {
   CGContextRef ctx=port();
   CGFloat ink=c.dark ? 0.85 : 0.0;
   CGFloat alpha=enabled ? (c.dark ? 0.85 : 0.85) : 0.3;
   CGPoint centre=at(c,rect,5.5,5.5);
   CGPoint a=at(c,rect,9.2,9.2),b=at(c,rect,12.5,12.5);

   CGContextBeginPath(ctx);
   CGContextAddEllipseInRect(ctx,CGRectMake(centre.x-4.6,centre.y-4.6,9.2,9.2));
   strokePath(ctx,1.2,ink,ink,ink,alpha);
   CGContextBeginPath(ctx);
   CGContextMoveToPoint(ctx,a.x,a.y);
   CGContextAddLineToPoint(ctx,b.x,b.y);
   strokePath(ctx,1.5,ink,ink,ink,alpha);
}

void PDDrawCancelButton(PDDrawContext c, NSRect rect) {
   CGContextRef ctx=port();
   CGFloat d=MIN(rect.size.width,rect.size.height);
   NSRect circle=NSMakeRect(NSMidX(rect)-d/2,NSMidY(rect)-d/2,d,d);
   CGFloat k=d*0.2;

   fillGrey(circle,d/2,c.dark ? 0.85 : 0.0,c.dark ? 0.85 : 0.75);
   CGContextBeginPath(ctx);
   CGContextMoveToPoint(ctx,NSMidX(circle)-k,NSMidY(circle)-k);
   CGContextAddLineToPoint(ctx,NSMidX(circle)+k,NSMidY(circle)+k);
   CGContextMoveToPoint(ctx,NSMidX(circle)-k,NSMidY(circle)+k);
   CGContextAddLineToPoint(ctx,NSMidX(circle)+k,NSMidY(circle)-k);
   strokePath(ctx,1.4,c.dark ? 0.1 : 1,c.dark ? 0.1 : 1,c.dark ? 0.1 : 1,1);
}

// the track is six rows tall, seven rows below the top of a 20 row frame
void PDDrawProgressBar(PDDrawContext c, NSRect frame, double fraction, BOOL indeterminate, double phase) {
   CGFloat h=6;
   NSRect track=PDRectFromTop(frame,c.flipped,floor((frame.size.height-h)/2),h);
   NSColor *accent=PDAccentColor(c);

   if(c.dark)
    fillGrey(track,h/2,1,0.10);
   else {
    fillGrey(track,h/2,0,0.09);
    fillGrey(down(c,NSInsetRect(track,0.5,0),0.5),h/2,0.88,1);
    fillGrey(NSMakeRect(track.origin.x+0.5,track.origin.y+(c.flipped ? 1 : 0),track.size.width-1,h-1),(h-1)/2,0.878,1);
   }
   CGContextSaveGState(port());
   CGContextBeginPath(port());
   addRoundedRect(port(),track,h/2);
   CGContextClip(port());
   [accent setFill];
   if(indeterminate){
    CGFloat w=MAX(h,floor(track.size.width*0.3));
    CGFloat x=NSMinX(track)-w+fmod(phase,1.0)*(track.size.width+w);
    NSRect bar=NSIntersectionRect(NSMakeRect(x,track.origin.y,w,h),track);

    if(bar.size.width<h)
     bar.size.width=0;

    CGContextBeginPath(port());
    addRoundedRect(port(),bar,h/2);
    CGContextFillPath(port());
   }
   else if(fraction>0){
    NSRect bar=track;

    bar.size.width=MAX(h,round(track.size.width*MIN(1.0,fraction)));
    CGContextBeginPath(port());
    addRoundedRect(port(),bar,h/2);
    CGContextFillPath(port());
   }
   CGContextRestoreGState(port());
}

void PDDrawSliderTrack(PDDrawContext c, NSRect track, BOOL vertical, CGFloat fillTo, BOOL enabled) {
   CGFloat thick=vertical ? track.size.width : track.size.height;
   NSColor *accent=c.active ? PDAccentColor(c) : [NSColor colorWithCalibratedWhite:c.dark ? 0.36 : 0.835 alpha:1];
   NSRect fill=track;

   if(c.dark)
    fillGrey(track,thick/2,1,0.10);
   else {
    fillGrey(track,thick/2,0,0.09);
    fillGrey(NSInsetRect(track,0.5,0.5),thick/2,0.878,1);
   }
   if(vertical){
    if(c.flipped){
     fill.origin.y=fillTo;
     fill.size.height=NSMaxY(track)-fillTo;
    }
    else
     fill.size.height=fillTo-NSMinY(track);
   }
   else
    fill.size.width=fillTo-NSMinX(track);
   if(fill.size.width<=0 || fill.size.height<=0)
    return;
   if(!enabled)
    accent=[accent colorWithAlphaComponent:0.4];
   CGContextSaveGState(port());
   [accent setFill];
   CGContextBeginPath(port());
   addRoundedRect(port(),fill,thick/2);
   CGContextFillPath(port());
   CGContextRestoreGState(port());
}

// a 21 row white disc with a hairline and a shadow under it
void PDDrawSliderKnob(PDDrawContext c, NSRect knob, BOOL enabled, BOOL pressed) {
   CGFloat d=MIN(knob.size.width,knob.size.height);
   NSRect disc=NSMakeRect(NSMidX(knob)-d/2,NSMidY(knob)-d/2,d,d);

   if(c.dark){
    fillGrey(down(c,disc,0.5),d/2,0,0.3);
    fillGrey(disc,d/2,pressed ? 0.7 : 0.6,enabled ? 1 : 0.5);
    return;
   }
   fillGrey(down(c,disc,1),d/2,0,0.05);
   fillGrey(down(c,disc,0.5),d/2,0,0.22);
   fillGrey(disc,d/2,0,0.06);
   fillGrey(NSInsetRect(disc,0.5,0.5),d/2-0.5,pressed ? 0.94 : (enabled ? 1 : 0.97),1);
}

// a 15 by 23 capsule split by a soft band, the chevrons a little under 6 points wide
void PDDrawStepper(PDDrawContext c, NSRect frame, BOOL enabled, BOOL upPressed, BOOL downPressed) {
   CGFloat w=MIN(15,frame.size.width),h=MIN(23,frame.size.height);
   NSRect body=PDRectFromTop(NSMakeRect(NSMinX(frame)+floor((frame.size.width-w)/2),frame.origin.y,w,frame.size.height),c.flipped,floor((frame.size.height-h)/2),h);
   NSRect upper=PDRectFromTop(body,c.flipped,0,floor(h/2));
   NSRect lower=PDRectFromTop(body,c.flipped,ceil(h/2),floor(h/2));
   CGFloat ink=c.dark ? 0.9 : 0.15;
   CGFloat alpha=enabled ? 1 : 0.35;

   if(c.dark){
    fillGrey(down(c,body,0.5),6,0,0.3);
    fillGrey(body,6,1,0.25);
   }
   else {
    fillGrey(down(c,body,1),6,0,0.06);
    fillGrey(body,6,0,0.2);
    fillGrey(NSInsetRect(body,1,1),5,1,1);
    fillGrey(PDRectFromTop(NSInsetRect(body,1,0),c.flipped,floor(h/2)-1,3),0,0,0.07);
   }
   if(upPressed)
    fillGrey(NSInsetRect(upper,1,1),5,0,0.12);
   if(downPressed)
    fillGrey(NSInsetRect(lower,1,1),5,0,0.12);
   chevron(c,NSMidX(body),NSMidY(upper)+(c.flipped ? 0.5 : -0.5),YES,5.5,3,1.3,ink,alpha);
   chevron(c,NSMidX(body),NSMidY(lower)-(c.flipped ? 0.5 : -0.5),NO,5.5,3,1.3,ink,alpha);
}

void PDDrawSegmentTrack(PDDrawContext c, NSRect track) {
   if(c.dark)
    fillGrey(track,6,1,0.10);
   else {
    fillGrey(track,6,0,0.065);
    fillGrey(NSInsetRect(track,0.5,0.5),5.5,0.89,0.6);
   }
}

void PDDrawSegmentSelection(PDDrawContext c, NSRect segment) {
   if(c.dark){
    fillGrey(segment,5,1,0.28);
    return;
   }
   fillGrey(down(c,segment,0.5),5,0,0.18);
   fillGrey(segment,5,0,0.05);
   fillGrey(NSInsetRect(segment,0.5,0.5),4.5,1,1);
}

void PDDrawSegmentDivider(PDDrawContext c, NSRect track, CGFloat x) {
   NSRect line=NSMakeRect(floor(x),NSMinY(track)+6,1,track.size.height-12);

   fillGrey(line,0,c.dark ? 1 : 0,c.dark ? 0.2 : 0.1);
}

// a 3 percent fill under a 7.5 percent hairline, radius 5
void PDDrawBoxFill(PDDrawContext c, NSRect box) {
   CGContextRef ctx=port();

   fillGrey(box,5,c.dark ? 1 : 0,c.dark ? 0.05 : 0.03);
   CGContextBeginPath(ctx);
   addRoundedRect(ctx,NSInsetRect(box,0.5,0.5),4.5);
   CGContextSetRGBStrokeColor(ctx,c.dark ? 1 : 0,c.dark ? 1 : 0,c.dark ? 1 : 0,c.dark ? 0.1 : 0.075);
   CGContextSetLineWidth(ctx,1);
   CGContextStrokePath(ctx);
}

// the accent ring around a focused control, three points wide and outside the frame
void PDDrawFocusRing(PDDrawContext c, NSRect frame, CGFloat radius) {
   CGContextRef ctx=port();
   NSColor *accent=[[NSColor keyboardFocusIndicatorColor] colorUsingColorSpaceName:NSCalibratedRGBColorSpace]?:PDAccentColor(c);

   CGContextSaveGState(ctx);
   CGContextBeginPath(ctx);
   addRoundedRect(ctx,NSInsetRect(frame,-1.5,-1.5),radius>0 ? radius+1.5 : 3);
   CGContextSetRGBStrokeColor(ctx,[accent redComponent],[accent greenComponent],[accent blueComponent],0.5);
   CGContextSetLineWidth(ctx,3);
   CGContextStrokePath(ctx);
   CGContextRestoreGState(ctx);
}

// while a cell draws, its rect methods follow the context it draws into, otherwise its view
static int sLayoutFlipped=-1;

BOOL PDLayoutFlipped(NSView *view) {
   if(sLayoutFlipped>=0)
    return sLayoutFlipped;
   return view!=nil ? [view isFlipped] : PDContextIsFlipped();
}

int PDBeginCellDrawing(void) {
   int saved=sLayoutFlipped;

   sLayoutFlipped=PDContextIsFlipped();
   return saved;
}

void PDEndCellDrawing(int saved) {
   sLayoutFlipped=saved;
}
