#import <QuartzCore/CABase.h>
#import <Foundation/NSString.h>
#import <mach/mach_time.h>

static double conversionFactor(){
   struct mach_timebase_info timebase;
   kern_return_t error=mach_timebase_info(&timebase);
   
   if(error!=KERN_SUCCESS){
    NSLog(@"mach_timebase_info returned %d",error);
    return 1;
   }

   return  0.000000001 * ((double)timebase.numer/(double)timebase.denom);
}

CFTimeInterval CACurrentMediaTime(void) {
   uint64_t value=mach_absolute_time();

   return ((double) value)*conversionFactor();
}
