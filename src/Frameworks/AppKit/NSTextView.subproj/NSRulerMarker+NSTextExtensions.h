//
//  NSRulerMarker+NSTextExtensions.h
//  AppKit
//
//  Created by Airy ANDRE on 16/11/12.
//
//

#import <AppKit/AppKit.h>

// Private methods to help text markers creation
@interface NSRulerMarker (NSTextExtensions)
+ (NSRulerMarker *)leftMarginMarkerWithRulerView:(NSRulerView *)ruler location:(CGFloat)location;
+ (NSRulerMarker *)rightMarginMarkerWithRulerView:(NSRulerView *)ruler location:(CGFloat)location;
+ (NSRulerMarker *)firstIndentMarkerWithRulerView:(NSRulerView *)ruler location:(CGFloat)location;
+ (NSRulerMarker *)leftIndentMarkerWithRulerView:(NSRulerView *)ruler location:(CGFloat)location;
+ (NSRulerMarker *)rightIndentMarkerWithRulerView:(NSRulerView *)ruler location:(CGFloat)location;
+ (NSRulerMarker *)leftTabMarkerWithRulerView:(NSRulerView *)ruler location:(CGFloat)location;
+ (NSRulerMarker *)rightTabMarkerWithRulerView:(NSRulerView *)ruler location:(CGFloat)location;
+ (NSRulerMarker *)centerTabMarkerWithRulerView:(NSRulerView *)ruler location:(CGFloat)location;
+ (NSRulerMarker *)decimalTabMarkerWithRulerView:(NSRulerView *)ruler location:(CGFloat)location;
@end
