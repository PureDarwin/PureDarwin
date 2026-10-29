// PDControlDrawing.h - draws the macOS 15 look of the standard controls with CoreGraphics paths
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSGraphics.h>
#import <AppKit/NSCell.h>

@class NSView, NSColor;

// every rect is in the current context's space, flipped says whether y grows downwards there
typedef struct {
   BOOL flipped;
   BOOL dark;
   BOOL active;
} PDDrawContext;

PDDrawContext PDCurrentDrawContext(NSView *controlView);
BOOL PDContextIsFlipped(void);

// a rect of the given height placed at offset dy from the visual top of frame
NSRect PDRectFromTop(NSRect frame, BOOL flipped, CGFloat dy, CGFloat height);

NSColor *PDAccentColor(PDDrawContext c);

// the 22 row push button bezel, top is its visual top edge
void PDDrawPushBezel(PDDrawContext c, NSRect bezel, BOOL defaulted, BOOL pressed, BOOL enabled);
void PDDrawSquareBezel(PDDrawContext c, NSRect bezel, BOOL pressed, BOOL enabled);

void PDDrawCheckBox(PDDrawContext c, NSRect box, NSControlStateValue state, BOOL enabled, BOOL pressed);
void PDDrawRadioButton(PDDrawContext c, NSRect box, BOOL on, BOOL enabled, BOOL pressed);

// the accent box with the up and down chevrons on a popup, or the single down chevron on a combo box
void PDDrawArrowBox(PDDrawContext c, NSRect box, BOOL upAndDown, BOOL enabled);

void PDDrawTextFieldBezel(PDDrawContext c, NSRect frame, NSColor *background, CGFloat radius);
void PDDrawSearchGlass(PDDrawContext c, NSRect rect, BOOL enabled);
void PDDrawCancelButton(PDDrawContext c, NSRect rect);

void PDDrawProgressBar(PDDrawContext c, NSRect frame, double fraction, BOOL indeterminate, double phase);
void PDDrawSliderTrack(PDDrawContext c, NSRect track, BOOL vertical, CGFloat fillTo, BOOL enabled);
void PDDrawSliderKnob(PDDrawContext c, NSRect knob, BOOL enabled, BOOL pressed);
void PDDrawStepper(PDDrawContext c, NSRect frame, BOOL enabled, BOOL upPressed, BOOL downPressed);
void PDDrawSegmentTrack(PDDrawContext c, NSRect track);
void PDDrawSegmentSelection(PDDrawContext c, NSRect segment);
void PDDrawSegmentDivider(PDDrawContext c, NSRect track, CGFloat x);
void PDDrawBoxFill(PDDrawContext c, NSRect box);
void PDDrawFocusRing(PDDrawContext c, NSRect frame, CGFloat radius);

// rect methods ask PDLayoutFlipped, a draw brackets itself in PDBeginCellDrawing and PDEndCellDrawing
BOOL PDLayoutFlipped(NSView *view);
int PDBeginCellDrawing(void);
void PDEndCellDrawing(int saved);
