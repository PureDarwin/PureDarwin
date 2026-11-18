#import <QuartzCore/CATransaction.h>
#include <dispatch/dispatch.h>
#import <QuartzCore/CALayerContext.h>
#import <Foundation/NSString.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSThread-Private.h>
#import "CATransactionGroup.h"


NSString * const kCATransactionAnimationDuration=@"kCATransactionAnimationDuration";
NSString * const kCATransactionDisableActions=@"kCATransactionDisableActions";
NSString * const kCATransactionAnimationTimingFunction=@"kCATransactionAnimationTimingFunction";
NSString * const kCATransactionCompletionBlock=@"kCATransactionCompletionBlock";

// methods this file sends that no header it imports declares, with their implementations' types: a guessed
// id return mismatches a void or floating point method, which wasm traps and other targets get away with
@interface NSObject (PDDeclaredHere)
- (id)objectForKey:(id)key; // Foundation/Collections.subproj/NSConstantCollections.m
- (void)setObject:(id)object forKey:(id)key; // Foundation/Collections.subproj/NSDictionary.m
@end

@implementation CATransaction

static NSMutableArray *transactionStack(){
   NSMutableDictionary *shared=[NSCurrentThread() sharedDictionary];
   id                   stack=[shared objectForKey:@"CATransactionStack"];
   
   if(stack==nil){
    stack=[NSMutableArray array];
    [shared setObject:stack forKey:@"CATransactionStack"];
   }
      
   return stack;
}

static CATransactionGroup *currentTransactionGroup(){
   id stack=transactionStack();
   
   CATransactionGroup *result=[stack lastObject];

   return result;
}

static CATransactionGroup *createImplicitTransactionGroupIfNeeded(){
   CATransactionGroup *check=currentTransactionGroup();
   
   if(check==nil){
    check=[[CATransactionGroup alloc] init];
   
    [transactionStack() addObject:check];
    dispatch_async(dispatch_get_main_queue(), ^{
     [CATransaction commit];
    });
   }
   
   return check;
}


+(BOOL)disableActions {
   return [[self valueForKey:kCATransactionDisableActions] boolValue];
}

+(CFTimeInterval)animationDuration {
   return [[self valueForKey:kCATransactionAnimationDuration] doubleValue];
}

+(CAMediaTimingFunction *)animationTimingFunction {
   return [self valueForKey:kCATransactionAnimationTimingFunction];
}

+(void (^)(void))completionBlock {
   return [self valueForKey:kCATransactionCompletionBlock];
}

+valueForKey:(NSString *)key {
   CATransactionGroup *group=createImplicitTransactionGroupIfNeeded();
   
   return [group valueForKey:key];
}

+(void)setAnimationDuration:(CFTimeInterval)value {
   [self setValue:[NSNumber numberWithDouble:value] forKey:kCATransactionAnimationDuration];
}

+(void)setAnimationTimingFunction:(CAMediaTimingFunction *)value {
   [self setValue:value forKey:kCATransactionAnimationTimingFunction];
}

+(void)setCompletionBlock:(void (^)(void))value {
   [self setValue:[[value copy] autorelease] forKey:kCATransactionCompletionBlock];
}

+(void)setDisableActions:(BOOL)value {
   [self setValue:[NSNumber numberWithBool:value] forKey:kCATransactionDisableActions];
}

+(void)setValue:value forKey:(NSString *)key {
   CATransactionGroup *group=createImplicitTransactionGroupIfNeeded();
   
   [group setValue:value forKey:key];
}

+(void)begin {
   CATransactionGroup *group=[[CATransactionGroup alloc] init];
   
   [transactionStack() addObject:group];
   [group release];
}

// nothing animates for long here, so the completion block runs once the changes are in
+(void)commit {
   CATransactionGroup *group=[[transactionStack() lastObject] retain];
   void (^completion)(void)=[[group valueForKey:kCATransactionCompletionBlock] retain];

   [transactionStack() removeLastObject];
   [group release];
   // the outermost commit is when the changes go to whoever presents them
   if([transactionStack() count]==0)
    [NSClassFromString(@"CALayerContext") performSelector:@selector(_transactionCommitted)];
   if(completion!=nil){
    dispatch_async(dispatch_get_main_queue(),^{
     completion();
     [completion release];
    });
   }
}

+(void)flush {
}

+(void)lock {
}

+(void)unlock {
}

@end
