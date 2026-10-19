// IOBluetooth.m - the device list of a system with no Bluetooth controller driver
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <Foundation/Foundation.h>

@interface IOBluetoothObject : NSObject
@end

@implementation IOBluetoothObject
@end

@interface IOBluetoothDevice : IOBluetoothObject
@end

@implementation IOBluetoothDevice
+ (NSArray *)pairedDevices { return @[]; }
+ (NSArray *)recentDevices:(NSUInteger)numDevices { return @[]; }
+ (instancetype)deviceWithAddressString:(NSString *)address { return nil; }
- (NSString *)name { return nil; }
- (NSString *)addressString { return nil; }
- (BOOL)isConnected { return NO; }
- (BOOL)isPaired { return NO; }
@end
