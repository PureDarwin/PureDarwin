//
//  NSURL+SOSPlistWriting.h
//

#import <Security/Security.h>
#import <Foundation/NSPropertyList.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSPropertyList.h>
#import <Foundation/NSData.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSDictionary.h>
#import <utilities/debugging.h>
#import <utilities/SecFileLocations.h>

#if ! __has_feature(objc_arc)
#error This file must be compiled with ARC. Either turn on ARC for the project or use -fobjc-arc flag
#endif

// may want to have this hold incoming events in file as well

@implementation NSURL (SOSPlistWriting)

- (BOOL)writePlist: (id) plist
{
    NSError *error = nil;
    if (![NSPropertyListSerialization propertyList: plist isValidForFormat: NSPropertyListXMLFormat_v1_0])
    {
        secerror("can't save PersistentState as XML");
        return false;
    }

    NSData *data = [NSPropertyListSerialization dataWithPropertyList: plist
        format: NSPropertyListXMLFormat_v1_0 options: 0 error: &error];
    if (data == nil)
    {
        secerror("error serializing PersistentState to xml: %@", error);
        return false;
    }

    BOOL writeStatus = [data writeToURL:self options: NSDataWritingAtomic error: &error];
    if (!writeStatus)
        secerror("error writing PersistentState to file: %@", error);
    
    return writeStatus;
}

- (id) readPlist
{
    NSError *error = nil;
    NSData *data = [NSData dataWithContentsOfURL: self options: 0 error: &error];
    if (data == nil)
    {
        secdebug("keyregister", "error reading PersistentState from %@: %@", self, error);
        return nil;
    }

    // Now the deserializing:

    NSPropertyListFormat format;
    id plist = [NSPropertyListSerialization propertyListWithData: data
        options: NSPropertyListMutableContainersAndLeaves format: &format error: &error];
                                            
    if (plist == nil)
        secerror("could not deserialize PersistentState from %@: %@", self, error);
    
    return plist;
}

@end

