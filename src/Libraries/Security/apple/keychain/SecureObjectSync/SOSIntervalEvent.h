//
//  SOSIntervalEvent.h
//  Security
//
//  Created by murf on 9/12/19.
//

#ifndef SOSIntervalEvent_h
#define SOSIntervalEvent_h

@interface SOSIntervalEvent : NSObject

@property   (nonatomic, retain)     NSUserDefaults      *defaults;
@property   (nonatomic, retain)     NSString            *dateDescription;
@property   (nonatomic)             NSTimeInterval      earliestDate;
@property   (nonatomic)             NSTimeInterval      latestDate;

- (id) initWithDefaults:(NSUserDefaults*) defaults dateDescription:(NSString *)dateDescription earliest:(NSTimeInterval) earliest latest: (NSTimeInterval) latest;
- (void) schedule;
- (bool) checkDate;
- (NSDate *) getDate;
- (void) followup;

@end

#endif /* SOSIntervalEvent_h */
