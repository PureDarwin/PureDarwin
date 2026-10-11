#import "O2Context_builtin_FT.h"
#import <Onyx2D/O2GraphicsState.h>
#import <Onyx2D/O2Image.h>

@implementation O2Context_builtin_FT

+(BOOL)canInitBackingWithContext:(O2Context *)context deviceDictionary:(NSDictionary *)deviceDictionary {
   return YES;
}

-initWithSurface:(O2Surface *)surface flipped:(BOOL)flipped {
   if([super initWithSurface:surface flipped:flipped]==nil)
    return nil;

   return self;
}

-(void)establishFontStateInDeviceIfDirty {
   O2GState *gState=O2ContextCurrentGState(self);
   
   if(gState->_fontIsDirty){
    O2GStateClearFontIsDirty(gState);
   }
}

@end
