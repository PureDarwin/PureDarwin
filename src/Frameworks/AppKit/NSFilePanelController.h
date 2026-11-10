// NSFilePanelController.h - the browser, name field and buttons inside NSOpenPanel and NSSavePanel
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSSavePanel.h>

@interface NSSavePanel (NSFilePanelController)
// the controller lives beside the panel as an associated object, so the public ivar layout stays as it was
- (id)_filePanelController;
- (NSInteger)_runFilePanelModal;
- (void)_beginFilePanelForWindow:(NSWindow *)window handler:(void (^)(NSInteger result))handler;
// builds and lays out the panel's views without showing it, for offscreen rendering
- (void)_prepareFilePanelForDisplay;
- (NSString *)_nameFieldValue;
- (void)_setNameFieldValue:(NSString *)name;
- (NSString *)_liveNameFieldValue;
- (NSArray *)_chosenURLs;
@end
