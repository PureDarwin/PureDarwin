#ifndef _SOSCONTROLSERVER_H_
#define _SOSCONTROLSERVER_H_

void SOSControlServerInitialize(void);

#if __OBJC2__
@interface SOSClient : NSObject <SOSControlProtocol>
@end

SOSClient *
SOSControlServerInternalClient(void);

#endif


#endif /* !_SOSCONTROLSERVER_H_ */
