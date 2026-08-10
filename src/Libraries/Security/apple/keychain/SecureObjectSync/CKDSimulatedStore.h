//
//  CKDSimulatedStore.h
//  Security
//
//  Created by Mitch Adler on 10/31/16.
//
//

#import "CKDStore.h"

@interface CKDSimulatedStore : NSObject <CKDStore>

+ (instancetype)simulatedInterface;
- (instancetype)init;

- (void)connectToProxy: (UbiqitousKVSProxy*) proxy;

- (NSObject*)objectForKey:(NSString*)key;

- (void)setObject:(id)obj forKey:(NSString*)key;
- (void)addEntriesFromDictionary:(NSDictionary<NSString*, NSObject*> *)otherDictionary;

- (void)removeObjectForKey:(NSString*)key;
- (void)removeAllObjects;

- (NSDictionary<NSString *, id>*) copyAsDictionary;

- (void)pushWrites:(NSArray<NSString*>*)keys requiresForceSync:(BOOL)requiresForceSync;
- (BOOL)pullUpdates:(NSError**) failure;
- (void)addOneToOutGoing;


- (void)remoteSetObject:(id)obj forKey:(NSString*)key;

@end
