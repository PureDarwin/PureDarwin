//
//  SOSTransportCircleCK.h
//  Security
//

#ifndef SOSTransportCircleCK_h
#define SOSTransportCircleCK_h

#import "SOSTransportCircle.h"
@class SOSCircleStorageTransport;

@interface SOSCKCircleStorage : SOSCircleStorageTransport
{
    
}

-(id)init;
-(id)initWithAccount:(SOSAccount*)acct;

@end;


#endif /* SOSTransportCircleCK_h */
