// PD: how AppKit hosts a CAMetalLayer in a view: a Wayland subsurface of the view's window
#import <QuartzCore/CAMetalLayer.h>

@interface CAMetalLayer (PDPrivate)
- (BOOL)_pdAttachToWindowNumber:(NSInteger)window origin:(CGPoint)origin size:(CGSize)size;
- (void)_pdDetach;
- (void)_pdSetViewSize:(CGSize)size;
- (BOOL)_pdIsAttached;
- (void *)_pdWaylandSurface:(void **)display;
- (void)_configure;
@end
