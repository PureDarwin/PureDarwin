// the menu bar server protocol shared by AppKit and MenuBar.app, carried over distributed objects
#import <Foundation/Foundation.h>

#define PDMenuServerName @"org.puredarwin.MenuServer"
#define PDMenuClientNameFormat @"org.puredarwin.MenuClient.%d"
// posted by a menu bar that has just started serving, so running apps publish to it again
#define PDMenuServerStartedNotification @"org.puredarwin.MenuServer.started"

// menuData is a plist tree of title and items, each item has title, isSeparator, enabled, state,
// keyEquivalent, keyEquivalentModifierMask and an optional submenu
@protocol PDMenuServer
- (oneway void)updateMenuForWindow:(bycopy NSNumber *)windowId
                          menuData:(bycopy NSDictionary *)menuData
                        clientName:(bycopy NSString *)clientName;
// the client's window took keyboard focus, so its menus become the bar's
- (oneway void)activateClient:(bycopy NSString *)clientName
                    forWindow:(bycopy NSNumber *)windowId;
@end

// indexPath addresses an item in the menu as published
@protocol PDMenuClient
- (oneway void)activateMenuItemAtPath:(NSArray *)indexPath
                            forWindow:(NSNumber *)windowId;
- (oneway void)requestMenuUpdateForWindow:(NSNumber *)windowId;
- (bycopy id)validateMenuStateForWindow:(NSNumber *)windowId;
// the Dock asks a running app to come forward, it raises its front window
- (oneway void)activateApplicationFromDock;
@end
