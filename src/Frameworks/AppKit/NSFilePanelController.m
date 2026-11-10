// NSFilePanelController.m - the browser, name field and buttons inside NSOpenPanel and NSSavePanel
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSFilePanelController.h>
#import <AppKit/NSOpenPanel.h>
#import <AppKit/NSModernAppKit.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSAlert.h>
#import <AppKit/NSButton.h>
#import <AppKit/NSButtonCell.h>
#import <AppKit/NSTextField.h>
#import <AppKit/NSPopUpButton.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSMenuItem.h>
#import <AppKit/NSScrollView.h>
#import <AppKit/NSTableView.h>
#import <AppKit/NSTableColumn.h>
#import <AppKit/NSTableHeaderView.h>
#import <AppKit/NSImageCell.h>
#import <AppKit/NSTextFieldCell.h>
#import <AppKit/NSSearchField.h>
#import <AppKit/NSBox.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSBezierPath.h>
#import <AppKit/NSWorkspace.h>
#import <AppKit/NSEvent.h>
#import <objc/runtime.h>
#import <dlfcn.h>

#define SIDEBAR_WIDTH 134
#define TOOLBAR_HEIGHT 40
#define FULLSIZE_TOOLBAR_HEIGHT 52
#define BOTTOM_HEIGHT 48
#define COMPACT_WIDTH 430

enum {
    PDPanelIdle,
    PDPanelModal,
    PDPanelSheet,
    PDPanelModeless
};

@interface NSSavePanel (PDFilePanelPrivate)
- (void)_setFilename:(NSString *)filename;
@end

@interface PDFileEntry : NSObject {
@public
    NSString *path;
    NSString *name;
    NSDate *date;
    unsigned long long size;
    NSString *kind;
    NSImage *icon;
    BOOL directory;
    BOOL package;
    BOOL enabled;
    BOOL header;
}
@end

@implementation PDFileEntry

- (void)dealloc
{
    [path release];
    [name release];
    [date release];
    [kind release];
    [icon release];
    [super dealloc];
}

- (NSString *)name
{
    return name;
}

- (NSDate *)date
{
    return date;
}

- (NSNumber *)size
{
    return [NSNumber numberWithUnsignedLongLong:directory && !package ? 0 : size];
}

- (NSString *)kind
{
    return kind;
}

@end

// a top-down container so every frame below is measured from the top-left as on the Mac
@interface PDFilePanelView : NSView {
@public
    id controller;
}
@end

// the file list also takes typing, Return and the sort clicks on its header
@interface PDFileTableView : NSTableView {
@public
    NSMutableString *typed;
    NSTimeInterval lastTyped;
}
@end

@interface NSFilePanelController : NSObject {
@public
    NSSavePanel *panel;
    PDFilePanelView *root;
    NSTextField *messageField;
    NSTextField *nameLabel;
    NSTextField *nameField;
    NSButton *disclosure;
    NSTextField *whereLabel;
    NSPopUpButton *wherePopUp;
    NSButton *backButton;
    NSButton *forwardButton;
    NSPopUpButton *pathPopUp;
    NSSearchField *searchField;
    NSScrollView *sidebarScroll;
    NSTableView *sidebar;
    NSBox *sidebarLine;
    NSScrollView *listScroll;
    PDFileTableView *list;
    NSBox *bottomLine;
    NSButton *newFolderButton;
    NSButton *hideExtensionButton;
    NSTextField *typeLabel;
    NSPopUpButton *typePopUp;
    NSButton *cancelButton;
    NSButton *okButton;
    NSButton *goToButton;
    NSButton *hiddenButton;
    NSButton *parentButton;
    NSView *accessory;

    NSString *directory;
    NSMutableArray *backStack;
    NSMutableArray *forwardStack;
    NSMutableArray *places;
    NSMutableArray *entries;
    NSString *sortKey;
    BOOL sortAscending;
    BOOL expanded;
    BOOL built;
    NSInteger mode;
    NSInteger typeIndex;
    NSArray *chosen;
    void (^handler)(NSInteger);
    NSWindow *parent;
}
- (instancetype)initWithPanel:(NSSavePanel *)panel;
- (void)layout;
- (void)reload;
- (void)typeSelect:(NSString *)text;
- (void)listReturn;
- (void)sortByColumn:(NSTableColumn *)column;
@end

@implementation PDFilePanelView

- (BOOL)isFlipped
{
    return YES;
}

- (void)resizeSubviewsWithOldSize:(NSSize)oldSize
{
    [controller layout];
}

- (void)drawRect:(NSRect)rect
{
    [[NSColor windowBackgroundColor] set];
    NSRectFill(rect);
}

// the toolbar, the bottom bar and any other empty part of the panel move the window, as on macOS
- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}

- (void)mouseDown:(NSEvent *)event
{
    [[self window] performWindowDragWithEvent:event];
}

@end

// the sidebar's empty background below its rows moves the window too
@interface PDSidebarTableView : NSTableView
@end

@implementation PDSidebarTableView

- (void)mouseDown:(NSEvent *)event
{
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];

    if ([self rowAtPoint:point] < 0) {
        [[self window] performWindowDragWithEvent:event];
        return;
    }
    [super mouseDown:event];
}

@end

@implementation PDFileTableView

- (void)dealloc
{
    [typed release];
    [super dealloc];
}

- (void)keyDown:(NSEvent *)event
{
    NSString *chars = [event characters];
    NSUInteger flags = [event modifierFlags] & (NSCommandKeyMask | NSControlKeyMask | NSAlternateKeyMask);
    unichar c = [chars length] ? [chars characterAtIndex:0] : 0;

    if (flags == 0 && (c == '\r' || c == 3)) {
        [(NSFilePanelController *)[self delegate] listReturn];
        return;
    }
    if (flags == 0 && c >= 0x20 && c < 0xF700 && c != 0x7F) {
        NSTimeInterval now = [NSDate timeIntervalSinceReferenceDate];

        if (typed == nil)
            typed = [NSMutableString new];
        if (now - lastTyped > 1.0)
            [typed setString:@""];
        lastTyped = now;
        [typed appendString:chars];
        [(NSFilePanelController *)[self delegate] typeSelect:typed];
        return;
    }
    [super keyDown:event];
}

@end

// back, forward and the disclosure button draw a chevron in the current text colour
@interface PDChevronButtonCell : NSButtonCell {
@public
    int direction;
}
@end

@implementation PDChevronButtonCell

- (void)drawInteriorWithFrame:(NSRect)frame inView:(NSView *)view
{
    NSBezierPath *path = [NSBezierPath bezierPath];
    CGFloat x = NSMidX(frame), y = NSMidY(frame);
    CGFloat d = direction == 0 ? -1 : 1;

    [([self isEnabled] ? [NSColor controlTextColor] : [NSColor disabledControlTextColor]) set];
    [path setLineWidth:1.6];
    if (direction == 2) {
        CGFloat flip = [view isFlipped] ? -1 : 1;

        [path moveToPoint:NSMakePoint(x - 4, y + 2 * flip)];
        [path lineToPoint:NSMakePoint(x, y - 2 * flip)];
        [path lineToPoint:NSMakePoint(x + 4, y + 2 * flip)];
    }
    else {
        [path moveToPoint:NSMakePoint(x - 2 * d, y - 5)];
        [path lineToPoint:NSMakePoint(x + 2 * d, y)];
        [path lineToPoint:NSMakePoint(x - 2 * d, y + 5)];
    }
    [path stroke];
}

@end

static void makeChevron(NSButton *button, int direction)
{
    PDChevronButtonCell *cell = [[[PDChevronButtonCell alloc] initTextCell:@""] autorelease];

    cell->direction = direction;
    [cell setBezelStyle:NSRoundedBezelStyle];
    [cell setButtonType:NSMomentaryPushInButton];
    [cell setTarget:[button target]];
    [cell setAction:[button action]];
    [cell setKeyEquivalent:[button keyEquivalent]];
    [cell setKeyEquivalentModifierMask:[button keyEquivalentModifierMask]];
    [button setCell:cell];
}

static NSImage *smallIcon(NSImage *icon)
{
    NSImage *small = [[icon copy] autorelease];

    [small setSize:NSMakeSize(16, 16)];
    return small;
}

static NSString *sizeString(unsigned long long size)
{
    if (size == 1)
        return @"1 byte";
    if (size < 1000)
        return [NSString stringWithFormat:@"%llu bytes", size];
    double value = size / 1000.0;
    NSArray *units = @[ @"KB", @"MB", @"GB", @"TB" ];
    NSUInteger i = 0;

    while (value >= 1000.0 && i + 1 < [units count]) {
        value /= 1000.0;
        i++;
    }
    if (i == 0 || value >= 100)
        return [NSString stringWithFormat:@"%.0f %@", value, units[i]];
    return [NSString stringWithFormat:@"%.1f %@", value, units[i]];
}

// type identifiers come from UniformTypeIdentifiers, loaded on first use when the app did not link it
static Class utTypeClass(void)
{
    Class ut = NSClassFromString(@"UTType");

    if (ut == nil) {
        dlopen("/System/Library/Frameworks/UniformTypeIdentifiers.framework/UniformTypeIdentifiers", RTLD_LAZY);
        ut = NSClassFromString(@"UTType");
    }
    return ut;
}

// an allowed type is either a filename extension or a type identifier, as on macOS
static BOOL isIdentifier(NSString *type)
{
    return [type rangeOfString:@"."].location != NSNotFound;
}

static NSString *extensionForType(NSString *type)
{
    if (!isIdentifier(type))
        return type;
    id ut = [utTypeClass() typeWithIdentifier:type];
    NSString *extension = [ut preferredFilenameExtension];

    return [extension length] ? extension : nil;
}

static BOOL extensionMatchesType(NSString *extension, NSString *type)
{
    if ([extension length] == 0)
        return NO;
    if (!isIdentifier(type))
        return [extension caseInsensitiveCompare:type] == NSOrderedSame;

    Class ut = utTypeClass();
    id fileType = [ut typeWithFilenameExtension:extension];
    id allowed = [ut typeWithIdentifier:type];

    if (fileType == nil || allowed == nil)
        return NO;
    return [fileType conformsToType:allowed];
}

static NSString *typeDescription(NSString *type)
{
    id ut = isIdentifier(type) ? [utTypeClass() typeWithIdentifier:type] : [utTypeClass() typeWithFilenameExtension:type];
    NSString *description = [ut localizedDescription];

    if ([description length])
        return description;
    return [[type uppercaseString] stringByAppendingString:@" document"];
}

// names sort as in the Finder, ignoring case and reading digit runs as numbers
static NSComparisonResult finderCompare(NSString *a, NSString *b)
{
    return [a compare:b options:NSCaseInsensitiveSearch | NSNumericSearch];
}

// the root volume goes by the name of its /Volumes link, as the startup disk does on macOS
static NSString *rootVolumeName(void)
{
    NSFileManager *fm = [NSFileManager defaultManager];

    for (NSString *volume in [fm contentsOfDirectoryAtPath:@"/Volumes" error:NULL]) {
        if ([[[@"/Volumes" stringByAppendingPathComponent:volume] stringByResolvingSymlinksInPath] isEqualToString:@"/"])
            return volume;
    }
    return @"PureDarwin";
}

static NSString *standardDirectory(NSString *path)
{
    path = [[path stringByExpandingTildeInPath] stringByStandardizingPath];
    return [path length] ? path : @"/";
}

@implementation NSFilePanelController

- (instancetype)initWithPanel:(NSSavePanel *)aPanel
{
    self = [super init];
    panel = aPanel;
    backStack = [NSMutableArray new];
    forwardStack = [NSMutableArray new];
    places = [NSMutableArray new];
    entries = [NSMutableArray new];
    sortKey = @"name";
    sortAscending = YES;
    typeIndex = 0;
    if (![self isOpen])
        expanded = [[NSUserDefaults standardUserDefaults] boolForKey:@"NSNavPanelExpandedStateForSaveMode"];
    return self;
}

- (void)dealloc
{
    [root release];
    [directory release];
    [backStack release];
    [forwardStack release];
    [places release];
    [entries release];
    [chosen release];
    [handler release];
    [super dealloc];
}

- (BOOL)isOpen
{
    return [panel isKindOfClass:[NSOpenPanel class]];
}

- (NSOpenPanel *)openPanel
{
    return (NSOpenPanel *)panel;
}

- (id)panelDelegate
{
    return [panel delegate];
}

- (BOOL)showsBrowser
{
    return [self isOpen] || expanded;
}

#pragma mark views

- (NSButton *)button:(NSString *)title action:(SEL)action
{
    NSButton *button = [[[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 80, 24)] autorelease];

    [button setButtonType:NSMomentaryPushInButton];
    [button setBezelStyle:NSRoundedBezelStyle];
    [button setTitle:title];
    [button setTarget:self];
    [button setAction:action];
    [button setFont:[NSFont systemFontOfSize:13]];
    [root addSubview:button];
    return button;
}

- (NSTextField *)label:(NSString *)text
{
    NSTextField *label = [NSTextField labelWithString:text ?: @""];

    [label setFont:[NSFont systemFontOfSize:13]];
    [root addSubview:label];
    return label;
}

- (NSTableColumn *)column:(NSString *)identifier title:(NSString *)title width:(CGFloat)width
{
    NSTableColumn *column = [[[NSTableColumn alloc] initWithIdentifier:identifier] autorelease];

    [[column headerCell] setStringValue:title];
    [column setWidth:width];
    [column setMinWidth:16];
    [column setEditable:NO];
    [[column dataCell] setFont:[NSFont systemFontOfSize:13]];
    [[column dataCell] setLineBreakMode:NSLineBreakByTruncatingMiddle];
    return column;
}

- (NSImageCell *)iconCell
{
    NSImageCell *cell = [[[NSImageCell alloc] init] autorelease];

    [cell setImageScaling:NSImageScaleProportionallyUpOrDown];
    [cell setImageAlignment:NSImageAlignCenter];
    return cell;
}

- (void)build
{
    NSSavePanel *p = panel;

    if (built)
        return;
    built = YES;

    root = [[PDFilePanelView alloc] initWithFrame:[[p contentView] frame]];
    root->controller = self;
    [p setContentView:root];

    messageField = [self label:@""];
    [messageField setAlignment:NSCenterTextAlignment];

    nameLabel = [self label:@""];
    [nameLabel setAlignment:NSRightTextAlignment];
    nameField = [[[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 240, 22)] autorelease];
    [nameField setFont:[NSFont systemFontOfSize:13]];
    [nameField setTarget:self];
    [nameField setAction:@selector(nameFieldAction:)];
    [nameField setDelegate:(id)self];
    [root addSubview:nameField];
    disclosure = [self button:@"" action:@selector(toggleExpanded:)];
    makeChevron(disclosure, 2);

    whereLabel = [self label:@"Where:"];
    [whereLabel setAlignment:NSRightTextAlignment];
    wherePopUp = [[[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 240, 25) pullsDown:NO] autorelease];
    [wherePopUp setTarget:self];
    [wherePopUp setAction:@selector(whereAction:)];
    [root addSubview:wherePopUp];

    backButton = [self button:@"" action:@selector(goBack:)];
    [backButton setKeyEquivalent:@"["];
    [backButton setKeyEquivalentModifierMask:NSCommandKeyMask];
    makeChevron(backButton, 0);
    forwardButton = [self button:@"" action:@selector(goForward:)];
    [forwardButton setKeyEquivalent:@"]"];
    [forwardButton setKeyEquivalentModifierMask:NSCommandKeyMask];
    makeChevron(forwardButton, 1);
    pathPopUp = [[[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 210, 25) pullsDown:NO] autorelease];
    [pathPopUp setTarget:self];
    [pathPopUp setAction:@selector(pathAction:)];
    [root addSubview:pathPopUp];
    searchField = [[[NSSearchField alloc] initWithFrame:NSMakeRect(0, 0, 156, 22)] autorelease];
    [searchField setTarget:self];
    [searchField setAction:@selector(searchAction:)];
    [[searchField cell] setPlaceholderString:@"Search"];
    [root addSubview:searchField];

    sidebar = [[[PDSidebarTableView alloc] initWithFrame:NSMakeRect(0, 0, SIDEBAR_WIDTH, 100)] autorelease];
    [sidebar addTableColumn:[self column:@"icon" title:@"" width:24]];
    [[sidebar tableColumnWithIdentifier:@"icon"] setDataCell:[self iconCell]];
    [sidebar addTableColumn:[self column:@"name" title:@"" width:SIDEBAR_WIDTH - 30]];
    [[[sidebar tableColumnWithIdentifier:@"name"] dataCell] setLineBreakMode:NSLineBreakByTruncatingTail];
    [sidebar setHeaderView:nil];
    [sidebar setRowHeight:24];
    [sidebar setIntercellSpacing:NSMakeSize(2, 4)];
    [sidebar setBackgroundColor:[NSColor controlBackgroundColor]];
    [sidebar setDataSource:(id)self];
    [sidebar setDelegate:(id)self];
    [sidebar setTarget:self];
    [sidebar setAction:@selector(sidebarAction:)];
    sidebarScroll = [[[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, SIDEBAR_WIDTH, 100)] autorelease];
    [sidebarScroll setDocumentView:sidebar];
    [sidebarScroll setHasVerticalScroller:YES];
    [sidebarScroll setAutohidesScrollers:YES];
    [sidebarScroll setBorderType:NSNoBorder];
    [root addSubview:sidebarScroll];
    sidebarLine = [[[NSBox alloc] initWithFrame:NSMakeRect(SIDEBAR_WIDTH, 0, 1, 100)] autorelease];
    [sidebarLine setBoxType:NSBoxSeparator];
    [root addSubview:sidebarLine];

    list = [[[PDFileTableView alloc] initWithFrame:NSMakeRect(0, 0, 400, 100)] autorelease];
    [list addTableColumn:[self column:@"icon" title:@"" width:18]];
    [[list tableColumnWithIdentifier:@"icon"] setDataCell:[self iconCell]];
    [[list tableColumnWithIdentifier:@"icon"] setResizingMask:NSTableColumnNoResizing];
    [list addTableColumn:[self column:@"name" title:@"Name" width:250]];
    [list addTableColumn:[self column:@"date" title:@"Date Modified" width:150]];
    [list addTableColumn:[self column:@"size" title:@"Size" width:70]];
    [[[list tableColumnWithIdentifier:@"size"] dataCell] setAlignment:NSRightTextAlignment];
    [list addTableColumn:[self column:@"kind" title:@"Kind" width:120]];
    for (NSString *key in @[ @"name", @"date", @"size", @"kind" ])
        [[list tableColumnWithIdentifier:key] setSortDescriptorPrototype:[NSSortDescriptor sortDescriptorWithKey:key ascending:YES]];
    [list setRowHeight:18];
    [list setIntercellSpacing:NSMakeSize(3, 2)];
    [list setUsesAlternatingRowBackgroundColors:YES];
    [list setAllowsEmptySelection:YES];
    [list setAllowsColumnReordering:NO];
    [list setDataSource:(id)self];
    [list setDelegate:(id)self];
    [list setTarget:self];
    [list setDoubleAction:@selector(listDoubleClick:)];
    // the scroller hides when it is not needed, so there is no corner box above it
    [list setCornerView:nil];
    listScroll = [[[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 400, 100)] autorelease];
    [listScroll setDocumentView:list];
    [listScroll setHasVerticalScroller:YES];
    [listScroll setHasHorizontalScroller:NO];
    [listScroll setAutohidesScrollers:YES];
    [listScroll setBorderType:NSNoBorder];
    [root addSubview:listScroll];

    bottomLine = [[[NSBox alloc] initWithFrame:NSMakeRect(0, 0, 100, 1)] autorelease];
    [bottomLine setBoxType:NSBoxSeparator];
    [root addSubview:bottomLine];

    newFolderButton = [self button:@"New Folder" action:@selector(newFolder:)];
    [newFolderButton setKeyEquivalent:@"n"];
    [newFolderButton setKeyEquivalentModifierMask:NSCommandKeyMask | NSShiftKeyMask];
    hideExtensionButton = [[[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 130, 18)] autorelease];
    [hideExtensionButton setButtonType:NSSwitchButton];
    [hideExtensionButton setTitle:@"Hide extension"];
    [hideExtensionButton setTarget:self];
    [hideExtensionButton setAction:@selector(hideExtensionAction:)];
    [root addSubview:hideExtensionButton];
    typeLabel = [self label:[self isOpen] ? @"Show:" : @"Format:"];
    [typeLabel setAlignment:NSRightTextAlignment];
    typePopUp = [[[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 180, 25) pullsDown:NO] autorelease];
    [typePopUp setTarget:self];
    [typePopUp setAction:@selector(typeAction:)];
    [root addSubview:typePopUp];

    cancelButton = [self button:@"Cancel" action:@selector(cancelAction:)];
    [cancelButton setKeyEquivalent:@"\e"];
    okButton = [self button:@"" action:@selector(okAction:)];
    [okButton setKeyEquivalent:@"\r"];
    [p setDefaultButtonCell:[okButton cell]];

    // keyboard commands without a visible control, as in the Finder
    goToButton = [self button:@"" action:@selector(goToFolder:)];
    [goToButton setKeyEquivalent:@"g"];
    [goToButton setKeyEquivalentModifierMask:NSCommandKeyMask | NSShiftKeyMask];
    hiddenButton = [self button:@"" action:@selector(toggleHidden:)];
    [hiddenButton setKeyEquivalent:@"."];
    [hiddenButton setKeyEquivalentModifierMask:NSCommandKeyMask | NSShiftKeyMask];
    parentButton = [self button:@"" action:@selector(goToParent:)];
    [parentButton setKeyEquivalent:[NSString stringWithFormat:@"%C", (unichar)NSUpArrowFunctionKey]];
    [parentButton setKeyEquivalentModifierMask:NSCommandKeyMask];
    for (NSButton *b in @[ goToButton, hiddenButton, parentButton ]) {
        [b setFrame:NSMakeRect(-100, -100, 1, 1)];
        [b setTransparent:YES];
        [b setRefusesFirstResponder:YES];
    }
}

// a macOS 15 open panel shows no title, its toolbar fills the title bar, a title the app sets is shown
- (void)updateTitlebar
{
    NSSavePanel *p = panel;
    BOOL hideTitle = [self isOpen] && [[p title] isEqualToString:@"Open"];
    NSUInteger style = [p styleMask];

    style = hideTitle ? (style | NSFullSizeContentViewWindowMask) : (style & ~NSFullSizeContentViewWindowMask);
    if (style != [p styleMask])
        [p setStyleMask:style];
    [p setTitleVisibility:hideTitle ? NSWindowTitleHidden : NSWindowTitleVisible];
    [p setTitlebarAppearsTransparent:hideTitle];
}

// pulls the panel's public properties into the views, every time the panel is shown
- (void)sync
{
    NSSavePanel *p = panel;
    NSString *prompt = [p prompt];
    NSString *label = [p nameFieldLabel];
    NSArray *types = [p allowedFileTypes];

    [self build];
    [self updateTitlebar];
    [messageField setStringValue:[p message] ?: @""];
    [nameLabel setStringValue:[label length] ? label : @"Save As:"];
    [nameField setStringValue:[p _nameFieldValue] ?: @""];
    [okButton setTitle:[prompt length] ? prompt : ([self isOpen] ? @"Open" : @"Save")];
    [hideExtensionButton setState:[p isExtensionHidden] ? NSOnState : NSOffState];

    [typePopUp removeAllItems];
    if ([types count] > 1) {
        if ([self isOpen])
            [typePopUp addItemWithTitle:@"All Supported Types"];
        for (NSString *type in types)
            [typePopUp addItemWithTitle:[NSString stringWithFormat:@"%@ (.%@)", typeDescription(type), extensionForType(type) ?: type]];
        if (typeIndex >= [typePopUp numberOfItems])
            typeIndex = 0;
        if (![self isOpen]) {
            NSString *extension = [[p _nameFieldValue] pathExtension];

            for (NSUInteger i = 0; i < [types count]; i++) {
                if (extensionMatchesType(extension, types[i]))
                    typeIndex = i;
            }
        }
        [typePopUp selectItemAtIndex:typeIndex];
    }

    if (accessory != [p accessoryView]) {
        [accessory removeFromSuperview];
        accessory = [p accessoryView];
        if (accessory != nil)
            [root addSubview:accessory];
    }
    [list setAllowsMultipleSelection:[self isOpen] && [[self openPanel] allowsMultipleSelection]];
    [searchField setStringValue:@""];
    [self reloadPlaces];
    [self setDirectory:[p directory] record:NO];
}

- (void)layout
{
    NSSavePanel *p = panel;
    NSRect bounds = [root bounds];
    CGFloat W = NSWidth(bounds), H = NSHeight(bounds);
    BOOL open = [self isOpen], browser = [self showsBrowser];
    BOOL hasMessage = [[messageField stringValue] length] > 0;
    BOOL hasTypes = [typePopUp numberOfItems] > 1;
    BOOL canCreate = [p canCreateDirectories];
    BOOL fullSize = ([p styleMask] & NSFullSizeContentViewWindowMask) != 0;
    CGFloat toolbar = fullSize ? FULLSIZE_TOOLBAR_HEIGHT : TOOLBAR_HEIGHT;
    CGFloat y = 0;

    [messageField setHidden:!hasMessage];
    if (hasMessage && !fullSize) {
        [messageField setFrame:NSMakeRect(20, 10, W - 40, 17)];
        y = 34;
    }
    else if (!open)
        y = 4;

    [nameLabel setHidden:open];
    [nameField setHidden:open];
    [disclosure setHidden:open];
    if (!open) {
        CGFloat fieldX = floor(W / 2) - 98;

        [nameLabel setFrame:NSMakeRect(fieldX - 108, y + 13, 100, 17)];
        [nameField setFrame:NSMakeRect(fieldX, y + 10, 234, 22)];
        [disclosure setFrame:NSMakeRect(fieldX + 242, y + 9, 28, 24)];
        y += 44;
    }

    [whereLabel setHidden:browser];
    [wherePopUp setHidden:browser];
    if (!browser) {
        CGFloat fieldX = floor(W / 2) - 98;

        [whereLabel setFrame:NSMakeRect(fieldX - 108, y + 4, 100, 17)];
        [wherePopUp setFrame:NSMakeRect(fieldX - 2, y, 238, 25)];
        y += 37;
        if (hasTypes) {
            [typeLabel setFrame:NSMakeRect(fieldX - 108, y + 4, 100, 17)];
            [typePopUp setFrame:NSMakeRect(fieldX - 2, y, 238, 25)];
            y += 37;
        }
    }

    CGFloat accessoryHeight = accessory != nil ? NSHeight([accessory frame]) + 16 : 0;
    CGFloat bottom = H - BOTTOM_HEIGHT - accessoryHeight;

    for (NSView *v in @[ backButton, forwardButton, pathPopUp, searchField, sidebarScroll, sidebarLine, listScroll ])
        [v setHidden:!browser];
    if (browser) {
        CGFloat x = SIDEBAR_WIDTH + 1, w = W - x;

        CGFloat bar = y + floor((toolbar - 24) / 2);
        CGFloat listY = y + toolbar;

        [sidebarScroll setFrame:NSMakeRect(0, y, SIDEBAR_WIDTH, bottom - y)];
        [sidebarLine setFrame:NSMakeRect(SIDEBAR_WIDTH, y, 1, bottom - y)];
        [[sidebar tableColumnWithIdentifier:@"name"] setWidth:SIDEBAR_WIDTH - 34];
        [backButton setFrame:NSMakeRect(x + 12, bar, 28, 24)];
        [forwardButton setFrame:NSMakeRect(x + 40, bar, 28, 24)];
        [searchField setFrame:NSMakeRect(W - 20 - 156, bar + 1, 156, 22)];
        CGFloat pathWidth = MIN(210, NSMinX([searchField frame]) - (x + 80) - 12);
        [pathPopUp setFrame:NSMakeRect(floor(x + (w - pathWidth) / 2), bar, pathWidth, 25)];
        // with the toolbar in the title bar the message sits under it, over the list
        if (hasMessage && fullSize) {
            [messageField setFrame:NSMakeRect(x + 20, listY + 8, w - 40, 17)];
            listY += 34;
        }
        [listScroll setFrame:NSMakeRect(x, listY, w, bottom - listY)];
        [[list tableColumnWithIdentifier:@"name"] setWidth:MAX(120, w - 18 - 150 - 70 - 120 - 30)];
        [list sizeLastColumnToFit];
    }

    if (accessory != nil) {
        NSRect frame = [accessory frame];

        frame.origin.x = floor((W - NSWidth(frame)) / 2);
        frame.origin.y = bottom + 8;
        [accessory setFrame:frame];
    }
    [bottomLine setFrame:NSMakeRect(0, H - BOTTOM_HEIGHT - accessoryHeight, W, 1)];
    [bottomLine setHidden:!browser && accessory == nil];

    CGFloat buttonY = H - 36;
    NSSize okSize = [[okButton cell] cellSize];
    CGFloat okWidth = MAX(80, ceil(okSize.width) + 8);
    CGFloat cancelWidth = MAX(80, ceil([[cancelButton cell] cellSize].width) + 8);

    [okButton setFrame:NSMakeRect(W - 20 - okWidth, buttonY, okWidth, 24)];
    [cancelButton setFrame:NSMakeRect(NSMinX([okButton frame]) - 12 - cancelWidth, buttonY, cancelWidth, 24)];

    CGFloat left = 20;
    [newFolderButton setHidden:!(canCreate && browser)];
    if (canCreate && browser) {
        CGFloat width = MAX(96, ceil([[newFolderButton cell] cellSize].width) + 8);

        [newFolderButton setFrame:NSMakeRect(left, buttonY, width, 24)];
        left += width + 16;
    }
    BOOL hideExtension = !open && [p canSelectHiddenExtension];
    [hideExtensionButton setHidden:!hideExtension];
    if (hideExtension) {
        [hideExtensionButton setFrame:NSMakeRect(left, buttonY + 3, 130, 18)];
        left += 138;
    }
    [typeLabel setHidden:!hasTypes];
    [typePopUp setHidden:!hasTypes];
    if (hasTypes && browser) {
        CGFloat room = NSMinX([cancelButton frame]) - 12 - left;
        CGFloat popWidth = MAX(100, MIN(220, room - 62));

        [typeLabel setFrame:NSMakeRect(left, buttonY + 4, 56, 17)];
        [typePopUp setFrame:NSMakeRect(left + 60, buttonY, popWidth, 25)];
    }
}

// the panel's size before it is shown, from the Mac's default open and save panels
- (NSSize)preferredContentSize
{
    BOOL hasMessage = [[panel message] length] > 0;
    CGFloat extra = (hasMessage ? 34 : 0) + ([panel accessoryView] ? NSHeight([[panel accessoryView] frame]) + 16 : 0);
    BOOL hasTypes = [[panel allowedFileTypes] count] > 1;

    if ([self isOpen])
        return NSMakeSize(800, ([panel styleMask] & NSFullSizeContentViewWindowMask ? 448 : 420) + extra);
    if (expanded)
        return NSMakeSize(800, 480 + extra);
    return NSMakeSize(COMPACT_WIDTH, 4 + 44 + 37 + BOTTOM_HEIGHT + (hasTypes ? 37 : 0) + extra);
}

- (void)fitPanel
{
    NSSize size = [self preferredContentSize];
    NSRect frame = [panel frameRectForContentRect:NSMakeRect(0, 0, size.width, size.height)];
    NSRect old = [panel frame];

    frame.origin.x = NSMidX(old) - NSWidth(frame) / 2;
    frame.origin.y = NSMaxY(old) - NSHeight(frame);
    [panel setFrame:frame display:NO];
    [panel setMinSize:[panel frameRectForContentRect:NSMakeRect(0, 0, [self showsBrowser] ? 560 : COMPACT_WIDTH, size.height < 300 ? size.height : 300)].size];
    if (![self showsBrowser])
        [panel setMaxSize:NSMakeSize(10000, NSHeight(frame))];
    else
        [panel setMaxSize:NSMakeSize(10000, 10000)];
    [root setFrame:[[panel contentView] frame]];
    [self layout];
}

#pragma mark places and directories

- (void)addPlace:(NSString *)path title:(NSString *)title
{
    BOOL isDirectory = NO;

    if (path != nil && (![[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&isDirectory] || !isDirectory))
        return;
    PDFileEntry *entry = [[PDFileEntry new] autorelease];

    entry->path = [path copy];
    entry->name = [title copy];
    entry->header = path == nil;
    entry->directory = YES;
    entry->enabled = YES;
    if (path != nil)
        entry->icon = [smallIcon([[NSWorkspace sharedWorkspace] iconForFile:path]) retain];
    [places addObject:entry];
}

- (void)reloadPlaces
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *home = NSHomeDirectory();

    [places removeAllObjects];
    [self addPlace:nil title:@"Favorites"];
    [self addPlace:home title:[home lastPathComponent]];
    for (NSString *name in @[ @"Desktop", @"Documents", @"Downloads" ])
        [self addPlace:[home stringByAppendingPathComponent:name] title:name];
    [self addPlace:nil title:@"Locations"];
    [self addPlace:@"/" title:rootVolumeName()];
    for (NSString *volume in [[fm contentsOfDirectoryAtPath:@"/Volumes" error:NULL] sortedArrayUsingSelector:@selector(compare:)]) {
        NSString *path = [@"/Volumes" stringByAppendingPathComponent:volume];

        if (![volume hasPrefix:@"."] && ![[path stringByResolvingSymlinksInPath] isEqualToString:@"/"])
            [self addPlace:path title:volume];
    }
    [sidebar reloadData];
}

- (BOOL)isPackage:(NSString *)path
{
    if ([panel treatsFilePackagesAsDirectories])
        return NO;
    return [[NSWorkspace sharedWorkspace] isFilePackageAtPath:path];
}

- (BOOL)isNavigable:(PDFileEntry *)entry
{
    return entry->directory && !entry->package;
}

- (BOOL)typeAllowed:(NSString *)path
{
    NSArray *types = [panel allowedFileTypes];
    NSString *extension = [path pathExtension];

    if ([types count] == 0)
        return YES;
    if ([self isOpen] && [types count] > 1 && typeIndex > 0)
        types = @[ types[typeIndex - 1] ];
    for (NSString *type in types) {
        if (extensionMatchesType(extension, type))
            return YES;
    }
    return NO;
}

- (BOOL)entryEnabled:(PDFileEntry *)entry
{
    id delegate = [self panelDelegate];
    BOOL enabled;

    if ([self isOpen]) {
        NSOpenPanel *o = [self openPanel];

        if ([self isNavigable:entry])
            enabled = YES;
        else if (entry->directory && !entry->package)
            enabled = [o canChooseDirectories];
        else
            enabled = [o canChooseFiles] && [self typeAllowed:entry->path];
    }
    else
        enabled = [self isNavigable:entry];

    if (enabled && [delegate respondsToSelector:@selector(panel:shouldEnableURL:)])
        enabled = [delegate panel:panel shouldEnableURL:[NSURL fileURLWithPath:entry->path isDirectory:entry->directory]];
    return enabled;
}

- (void)reload
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *filter = [searchField stringValue];
    BOOL hidden = [panel showsHiddenFiles];
    NSString *selected = [list selectedRow] >= 0 && [list selectedRow] < (NSInteger)[entries count] ? [((PDFileEntry *)entries[[list selectedRow]])->path retain] : nil;

    [entries removeAllObjects];
    for (NSString *name in [fm contentsOfDirectoryAtPath:directory error:NULL]) {
        NSString *path = [directory stringByAppendingPathComponent:name];
        NSDictionary *attributes = [fm attributesOfItemAtPath:path error:NULL];
        BOOL isDirectory = NO;

        if (!hidden && [name hasPrefix:@"."])
            continue;
        if ([filter length] && [name rangeOfString:filter options:NSCaseInsensitiveSearch].location == NSNotFound)
            continue;
        [fm fileExistsAtPath:path isDirectory:&isDirectory];

        PDFileEntry *entry = [[PDFileEntry new] autorelease];
        entry->path = [path copy];
        entry->directory = isDirectory;
        entry->package = isDirectory && [self isPackage:path];
        entry->name = [name copy];
        entry->date = [[attributes fileModificationDate] retain];
        entry->size = [attributes fileSize];
        if (isDirectory && !entry->package)
            entry->kind = @"Folder";
        else if ([[name pathExtension] isEqualToString:@"app"])
            entry->kind = @"Application";
        else if ([[name pathExtension] length])
            entry->kind = [typeDescription([name pathExtension]) copy];
        else
            entry->kind = (isDirectory ? @"Package" : @"Document");
        entry->enabled = [self entryEnabled:entry];
        [entries addObject:entry];
    }
    [self sortEntries];
    [list reloadData];
    [list deselectAll:nil];
    for (NSUInteger i = 0; selected && i < [entries count]; i++) {
        if ([((PDFileEntry *)entries[i])->path isEqualToString:selected])
            [list selectRowIndexes:[NSIndexSet indexSetWithIndex:i] byExtendingSelection:NO];
    }
    [selected release];
    [self updateButtons];
}

- (void)sortEntries
{
    NSString *key = sortKey;
    BOOL ascending = sortAscending;

    [entries sortUsingComparator:^NSComparisonResult(PDFileEntry *a, PDFileEntry *b) {
        NSComparisonResult r = NSOrderedSame;

        if ([key isEqualToString:@"date"])
            r = [a->date compare:b->date];
        else if ([key isEqualToString:@"size"])
            r = [[a size] compare:[b size]];
        else if ([key isEqualToString:@"kind"])
            r = finderCompare(a->kind, b->kind);
        if (r == NSOrderedSame)
            r = finderCompare(a->name, b->name);
        return ascending ? r : -r;
    }];
}

- (void)sortByColumn:(NSTableColumn *)column
{
    NSString *key = [column identifier];

    if ([key isEqualToString:@"icon"])
        return;
    if ([key isEqualToString:sortKey])
        sortAscending = !sortAscending;
    else {
        sortKey = [@{ @"name" : @"name", @"date" : @"date", @"size" : @"size", @"kind" : @"kind" } objectForKey:key];
        sortAscending = ![key isEqualToString:@"date"];
    }
    [list setSortDescriptors:@[ [NSSortDescriptor sortDescriptorWithKey:sortKey ascending:sortAscending] ]];
    if ([list respondsToSelector:@selector(setHighlightedTableColumn:)])
        [list setHighlightedTableColumn:column];
    [self reload];
}

- (void)updatePathPopUps
{
    NSWorkspace *ws = [NSWorkspace sharedWorkspace];
    NSString *path = directory;

    [pathPopUp removeAllItems];
    while (path != nil) {
        NSString *title = [path isEqualToString:@"/"] ? rootVolumeName() : [path lastPathComponent];
        NSMenuItem *item = [[[NSMenuItem alloc] initWithTitle:title action:NULL keyEquivalent:@""] autorelease];
        NSImage *icon = [[[ws iconForFile:path] copy] autorelease];

        [icon setSize:NSMakeSize(16, 16)];
        [item setImage:icon];
        [item setRepresentedObject:path];
        [[pathPopUp menu] addItem:item];
        path = [path isEqualToString:@"/"] ? nil : [path stringByDeletingLastPathComponent];
    }
    [pathPopUp selectItemAtIndex:0];

    [wherePopUp removeAllItems];
    NSMutableArray *seen = [NSMutableArray arrayWithObject:directory];
    [wherePopUp addItemWithTitle:[[pathPopUp itemAtIndex:0] title]];
    [[wherePopUp lastItem] setRepresentedObject:directory];
    [[wherePopUp lastItem] setImage:[[pathPopUp itemAtIndex:0] image]];
    [[wherePopUp menu] addItem:[NSMenuItem separatorItem]];
    for (PDFileEntry *place in places) {
        if (place->header || [seen containsObject:place->path])
            continue;
        [seen addObject:place->path];
        [wherePopUp addItemWithTitle:place->name];
        [[wherePopUp lastItem] setRepresentedObject:place->path];
    }
    [wherePopUp selectItemAtIndex:0];

    NSInteger row = -1;
    for (NSUInteger i = 0; i < [places count]; i++) {
        PDFileEntry *place = places[i];

        if (!place->header && [place->path isEqualToString:directory])
            row = i;
    }
    if (row >= 0)
        [sidebar selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];
    else
        [sidebar deselectAll:nil];
}

- (void)setDirectory:(NSString *)path record:(BOOL)record
{
    BOOL isDirectory = NO;
    id delegate = [self panelDelegate];

    path = standardDirectory(path ?: NSHomeDirectory());
    if (![[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&isDirectory] || !isDirectory)
        path = NSHomeDirectory();
    if (record && directory != nil && ![directory isEqualToString:path]) {
        [backStack addObject:directory];
        [forwardStack removeAllObjects];
    }
    BOOL changed = ![directory isEqualToString:path];

    [directory release];
    directory = [path copy];
    if (![[panel directory] isEqualToString:path])
        [panel setDirectory:path];
    [self updatePathPopUps];
    [self reload];
    [[list window] makeFirstResponder:[self isOpen] ? (NSResponder *)list : (NSResponder *)nameField];
    if (changed && record && [delegate respondsToSelector:@selector(panel:didChangeToDirectoryURL:)])
        [delegate panel:panel didChangeToDirectoryURL:[NSURL fileURLWithPath:path isDirectory:YES]];
}

- (void)updateButtons
{
    [backButton setEnabled:[backStack count] > 0];
    [forwardButton setEnabled:[forwardStack count] > 0];
    if ([self isOpen]) {
        NSIndexSet *rows = [list selectedRowIndexes];
        BOOL ok = [rows count] > 0;

        for (NSUInteger i = [rows firstIndex]; i != NSNotFound; i = [rows indexGreaterThanIndex:i]) {
            if (!((PDFileEntry *)entries[i])->enabled)
                ok = NO;
        }
        if ([rows count] == 0 && [[self openPanel] canChooseDirectories])
            ok = YES;
        [okButton setEnabled:ok];
    }
    else
        [okButton setEnabled:[[nameField stringValue] length] > 0];
}

#pragma mark actions

- (void)goBack:(id)sender
{
    if ([backStack count] == 0)
        return;
    [forwardStack addObject:directory];
    NSString *path = [[[backStack lastObject] retain] autorelease];

    [backStack removeLastObject];
    [self setDirectory:path record:NO];
    [self notifyDirectoryChange];
}

- (void)goForward:(id)sender
{
    if ([forwardStack count] == 0)
        return;
    [backStack addObject:directory];
    NSString *path = [[[forwardStack lastObject] retain] autorelease];

    [forwardStack removeLastObject];
    [self setDirectory:path record:NO];
    [self notifyDirectoryChange];
}

- (void)notifyDirectoryChange
{
    id delegate = [self panelDelegate];

    if ([delegate respondsToSelector:@selector(panel:didChangeToDirectoryURL:)])
        [delegate panel:panel didChangeToDirectoryURL:[NSURL fileURLWithPath:directory isDirectory:YES]];
}

- (void)goToParent:(id)sender
{
    if (![directory isEqualToString:@"/"])
        [self setDirectory:[directory stringByDeletingLastPathComponent] record:YES];
}

- (void)pathAction:(id)sender
{
    [self setDirectory:[[sender selectedItem] representedObject] record:YES];
}

- (void)whereAction:(id)sender
{
    NSString *path = [[sender selectedItem] representedObject];

    if (path != nil)
        [self setDirectory:path record:YES];
}

- (void)sidebarAction:(id)sender
{
    NSInteger row = [sidebar clickedRow] >= 0 ? [sidebar clickedRow] : [sidebar selectedRow];

    if (row >= 0 && row < (NSInteger)[places count] && !((PDFileEntry *)places[row])->header)
        [self setDirectory:((PDFileEntry *)places[row])->path record:YES];
}

- (void)searchAction:(id)sender
{
    [self reload];
}

- (void)typeAction:(id)sender
{
    typeIndex = [typePopUp indexOfSelectedItem];
    if (![self isOpen]) {
        NSString *extension = extensionForType([panel allowedFileTypes][typeIndex]);
        NSString *name = [nameField stringValue];

        if ([extension length] && [name length])
            [nameField setStringValue:[[name stringByDeletingPathExtension] stringByAppendingPathExtension:extension]];
    }
    [self reload];
}

- (void)hideExtensionAction:(id)sender
{
    [panel setExtensionHidden:[hideExtensionButton state] == NSOnState];
    [self reload];
}

- (void)toggleHidden:(id)sender
{
    [panel setShowsHiddenFiles:![panel showsHiddenFiles]];
    [self reload];
}

- (void)toggleExpanded:(id)sender
{
    expanded = !expanded;
    [[NSUserDefaults standardUserDefaults] setBool:expanded forKey:@"NSNavPanelExpandedStateForSaveMode"];
    [self fitPanel];
    [root setNeedsDisplay:YES];
}

- (NSString *)askForPath:(NSString *)message informative:(NSString *)informative value:(NSString *)value button:(NSString *)button
{
    NSAlert *alert = [[[NSAlert alloc] init] autorelease];
    NSTextField *field = [[[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 260, 22)] autorelease];

    [alert setMessageText:message];
    if (informative != nil)
        [alert setInformativeText:informative];
    [alert addButtonWithTitle:button];
    [alert addButtonWithTitle:@"Cancel"];
    [field setStringValue:value ?: @""];
    [alert setAccessoryView:field];
    [[alert window] setInitialFirstResponder:field];
    if ([alert runModal] != NSAlertFirstButtonReturn)
        return nil;
    return [field stringValue];
}

- (void)goToFolder:(id)sender
{
    NSString *path = [self askForPath:@"Go to the folder:" informative:nil value:directory button:@"Go"];
    BOOL isDirectory = NO;

    if (path == nil)
        return;
    if (![path hasPrefix:@"/"] && ![path hasPrefix:@"~"])
        path = [directory stringByAppendingPathComponent:path];
    path = standardDirectory(path);
    if (![[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&isDirectory]) {
        NSBeep();
        return;
    }
    if (!isDirectory) {
        NSString *file = [path lastPathComponent];

        [self setDirectory:[path stringByDeletingLastPathComponent] record:YES];
        [self selectName:file];
        if (![self isOpen])
            [nameField setStringValue:file];
        return;
    }
    [self setDirectory:path record:YES];
}

- (void)newFolder:(id)sender
{
    NSString *name = [self askForPath:@"New Folder" informative:@"Name of new folder:" value:@"untitled folder" button:@"Create"];
    NSError *error = nil;

    if ([name length] == 0)
        return;
    NSString *path = [directory stringByAppendingPathComponent:name];

    if (![[NSFileManager defaultManager] createDirectoryAtPath:path withIntermediateDirectories:NO attributes:nil error:&error]) {
        NSAlert *alert = [[[NSAlert alloc] init] autorelease];

        [alert setMessageText:[NSString stringWithFormat:@"The folder \"%@\" can't be created.", name]];
        [alert setInformativeText:[error localizedDescription] ?: @""];
        [alert runModal];
        return;
    }
    [self setDirectory:path record:YES];
}

- (void)selectName:(NSString *)name
{
    for (NSUInteger i = 0; i < [entries count]; i++) {
        PDFileEntry *entry = entries[i];

        if ([[entry->path lastPathComponent] isEqualToString:name]) {
            [list selectRowIndexes:[NSIndexSet indexSetWithIndex:i] byExtendingSelection:NO];
            [list scrollRowToVisible:i];
            return;
        }
    }
}

- (void)typeSelect:(NSString *)text
{
    for (NSUInteger i = 0; i < [entries count]; i++) {
        PDFileEntry *entry = entries[i];

        if ([entry->name rangeOfString:text options:NSCaseInsensitiveSearch | NSAnchoredSearch].location != NSNotFound) {
            [list selectRowIndexes:[NSIndexSet indexSetWithIndex:i] byExtendingSelection:NO];
            [list scrollRowToVisible:i];
            return;
        }
    }
    NSBeep();
}

- (void)openEntry:(PDFileEntry *)entry
{
    if ([self isNavigable:entry])
        [self setDirectory:entry->path record:YES];
    else if (entry->enabled)
        [panel ok:nil];
}

- (void)listDoubleClick:(id)sender
{
    NSInteger row = [list clickedRow];

    if (row >= 0 && row < (NSInteger)[entries count])
        [self openEntry:entries[row]];
}

- (void)listReturn
{
    [okButton performClick:nil];
}

- (void)nameFieldAction:(id)sender
{
    [okButton performClick:nil];
}

- (void)controlTextDidChange:(NSNotification *)note
{
    [self updateButtons];
}

- (void)okAction:(id)sender
{
    [panel ok:sender];
}

- (void)cancelAction:(id)sender
{
    [panel cancel:sender];
}

#pragma mark confirming

- (NSString *)nameWithExtension:(NSString *)name
{
    NSArray *types = [panel allowedFileTypes];
    NSString *extension = [name pathExtension];

    if ([types count] == 0)
        return name;
    for (NSString *type in types) {
        if (extensionMatchesType(extension, type))
            return name;
    }
    if ([panel allowsOtherFileTypes] && [extension length])
        return name;

    NSUInteger index = [types count] > 1 && typeIndex < [types count] ? typeIndex : 0;
    NSString *wanted = extensionForType(types[index]);

    return [wanted length] ? [name stringByAppendingPathExtension:wanted] : name;
}

- (BOOL)validate:(NSURL *)url
{
    id delegate = [self panelDelegate];
    NSError *error = nil;

    if (![delegate respondsToSelector:@selector(panel:validateURL:error:)])
        return YES;
    if ([delegate panel:panel validateURL:url error:&error])
        return YES;
    if (error != nil) {
        NSAlert *alert = [NSAlert alertWithError:error];

        [alert beginSheetModalForWindow:panel completionHandler:^(NSInteger r) {}];
    }
    return NO;
}

- (void)finishWithURLs:(NSArray *)urls
{
    NSURL *first = [urls firstObject];

    [chosen release];
    chosen = [urls copy];
    if ([self isOpen])
        [panel _setFilename:[first path]];
    else {
        [panel setDirectory:[[first path] stringByDeletingLastPathComponent]];
        [panel _setNameFieldValue:[[first path] lastPathComponent]];
    }
    [[NSUserDefaults standardUserDefaults] setObject:directory forKey:@"NSNavLastRootDirectory"];
    [self endWithCode:NSModalResponseOK];
}

- (void)confirm
{
    if ([self isOpen]) {
        NSIndexSet *rows = [list selectedRowIndexes];
        NSMutableArray *urls = [NSMutableArray array];

        if ([rows count] == 1) {
            PDFileEntry *entry = entries[[rows firstIndex]];

            // a folder that cannot be chosen opens instead
            if ([self isNavigable:entry] && ![[self openPanel] canChooseDirectories]) {
                [self setDirectory:entry->path record:YES];
                return;
            }
        }
        for (NSUInteger i = [rows firstIndex]; i != NSNotFound; i = [rows indexGreaterThanIndex:i]) {
            PDFileEntry *entry = entries[i];

            if (!entry->enabled) {
                NSBeep();
                return;
            }
            [urls addObject:[NSURL fileURLWithPath:entry->path isDirectory:entry->directory]];
        }
        if ([urls count] == 0) {
            if (![[self openPanel] canChooseDirectories]) {
                NSBeep();
                return;
            }
            [urls addObject:[NSURL fileURLWithPath:directory isDirectory:YES]];
        }
        for (NSURL *url in urls) {
            if (![self validate:url])
                return;
        }
        [self finishWithURLs:urls];
        return;
    }

    NSString *name = [nameField stringValue];
    id delegate = [self panelDelegate];

    if ([name length] == 0) {
        NSBeep();
        return;
    }
    if ([name hasPrefix:@"/"] || [name hasPrefix:@"~"]) {
        NSString *full = [[name stringByExpandingTildeInPath] stringByStandardizingPath];

        [self setDirectory:[full stringByDeletingLastPathComponent] record:YES];
        name = [full lastPathComponent];
        [nameField setStringValue:name];
    }
    name = [self nameWithExtension:name];
    if ([delegate respondsToSelector:@selector(panel:userEnteredFilename:confirmed:)]) {
        name = [delegate panel:panel userEnteredFilename:name confirmed:YES];
        if ([name length] == 0)
            return;
    }

    NSString *path = [directory stringByAppendingPathComponent:name];
    NSURL *url = [NSURL fileURLWithPath:path];
    BOOL isDirectory = NO;

    if ([[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&isDirectory]) {
        if (isDirectory && ![self isPackage:path]) {
            [self setDirectory:path record:YES];
            return;
        }
        NSAlert *alert = [[[NSAlert alloc] init] autorelease];

        [alert setAlertStyle:NSWarningAlertStyle];
        [alert setMessageText:[NSString stringWithFormat:@"\"%@\" already exists. Do you want to replace it?", name]];
        [alert setInformativeText:[NSString stringWithFormat:@"A file or folder with the same name already exists in the folder %@. Replacing it will overwrite its current contents.", [directory lastPathComponent]]];
        [alert addButtonWithTitle:@"Cancel"];
        [alert addButtonWithTitle:@"Replace"];
        [alert beginSheetModalForWindow:panel completionHandler:^(NSInteger r) {
            if (r == NSAlertSecondButtonReturn && [self validate:url])
                [self finishWithURLs:@[ url ]];
        }];
        return;
    }
    if (![self validate:url])
        return;
    [self finishWithURLs:@[ url ]];
}

- (void)endWithCode:(NSInteger)code
{
    NSInteger was = mode;

    mode = PDPanelIdle;
    if (was == PDPanelModal) {
        [NSApp stopModalWithCode:code];
        return;
    }
    if (was == PDPanelSheet) {
        [NSApp endSheet:panel returnCode:code];
        return;
    }
    [panel orderOut:nil];
    if (handler != nil) {
        void (^h)(NSInteger) = handler;

        handler = nil;
        h(code);
        [h release];
    }
}

- (void)sheetDidEnd:(NSWindow *)sheet returnCode:(NSInteger)code contextInfo:(void *)info
{
    void (^h)(NSInteger) = handler;

    handler = nil;
    [panel orderOut:nil];
    if (h != nil) {
        h(code);
        [h release];
    }
    [parent release];
    parent = nil;
}

#pragma mark running

- (void)prepare
{
    [self sync];
    [self fitPanel];
    [self updateButtons];
}

- (NSInteger)runModal
{
    NSInteger result;

    [self prepare];
    [panel center];
    mode = PDPanelModal;
    [panel makeKeyAndOrderFront:nil];
    [panel makeFirstResponder:[self isOpen] ? (NSResponder *)list : (NSResponder *)nameField];
    result = [NSApp runModalForWindow:panel];
    mode = PDPanelIdle;
    [panel orderOut:nil];
    if (result != NSModalResponseOK)
        return NSModalResponseCancel;
    return result;
}

- (void)beginForWindow:(NSWindow *)window handler:(void (^)(NSInteger))aHandler
{
    [self prepare];
    [handler release];
    handler = [aHandler copy];
    if (window != nil) {
        mode = PDPanelSheet;
        parent = [window retain];
        [NSApp beginSheet:panel modalForWindow:window modalDelegate:self
            didEndSelector:@selector(sheetDidEnd:returnCode:contextInfo:) contextInfo:NULL];
        [panel makeFirstResponder:[self isOpen] ? (NSResponder *)list : (NSResponder *)nameField];
        return;
    }
    mode = PDPanelModeless;
    [panel center];
    [panel makeKeyAndOrderFront:nil];
    [panel makeFirstResponder:[self isOpen] ? (NSResponder *)list : (NSResponder *)nameField];
}

- (void)cancel
{
    [self endWithCode:NSModalResponseCancel];
}

- (NSArray *)chosen
{
    return chosen;
}

#pragma mark table data

- (NSInteger)numberOfRowsInTableView:(NSTableView *)table
{
    return table == sidebar ? [places count] : [entries count];
}

- (id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    PDFileEntry *entry = (table == sidebar ? places : entries)[row];
    NSString *key = [column identifier];

    if ([key isEqualToString:@"icon"]) {
        if (entry->header)
            return nil;
        if (entry->icon == nil)
            entry->icon = [smallIcon([[NSWorkspace sharedWorkspace] iconForFile:entry->path]) retain];
        return entry->icon;
    }
    if ([key isEqualToString:@"name"])
        return entry->name;
    if ([key isEqualToString:@"date"]) {
        static NSDateFormatter *formatter;

        if (formatter == nil) {
            formatter = [NSDateFormatter new];
            [formatter setDateStyle:NSDateFormatterMediumStyle];
            [formatter setTimeStyle:NSDateFormatterShortStyle];
        }
        return entry->date ? [formatter stringFromDate:entry->date] : @"--";
    }
    if ([key isEqualToString:@"size"])
        return entry->directory && !entry->package ? @"--" : sizeString(entry->size);
    if ([key isEqualToString:@"kind"])
        return entry->kind;
    return nil;
}

- (void)tableView:(NSTableView *)table willDisplayCell:(id)cell forTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    PDFileEntry *entry = (table == sidebar ? places : entries)[row];

    if (table == sidebar && [cell isKindOfClass:[NSTextFieldCell class]]) {
        [cell setFont:entry->header ? [NSFont boldSystemFontOfSize:11] : [NSFont systemFontOfSize:13]];
        [cell setTextColor:entry->header ? [NSColor secondaryLabelColor] : [NSColor controlTextColor]];
        return;
    }
    if ([cell respondsToSelector:@selector(setEnabled:)])
        [cell setEnabled:entry->enabled];
    if ([cell isKindOfClass:[NSTextFieldCell class]]) {
        BOOL selected = [table isRowSelected:row];

        [cell setTextColor:!entry->enabled ? [NSColor disabledControlTextColor] :
            (selected ? [NSColor alternateSelectedControlTextColor] :
            ([[column identifier] isEqualToString:@"name"] ? [NSColor controlTextColor] : [NSColor secondaryLabelColor]))];
    }
}

- (BOOL)tableView:(NSTableView *)table shouldSelectRow:(NSInteger)row
{
    if (table == sidebar)
        return !((PDFileEntry *)places[row])->header;
    return YES;
}

- (BOOL)selectionShouldChangeInTableView:(NSTableView *)table
{
    return YES;
}

- (void)tableView:(NSTableView *)table mouseDownInHeaderOfTableColumn:(NSTableColumn *)column
{
    if (table == list)
        [self sortByColumn:column];
}

- (void)tableViewSelectionDidChange:(NSNotification *)note
{
    id delegate = [self panelDelegate];

    if ([note object] != list)
        return;
    if (![self isOpen]) {
        NSInteger row = [list selectedRow];

        if (row >= 0 && !((PDFileEntry *)entries[row])->directory)
            [nameField setStringValue:[((PDFileEntry *)entries[row])->path lastPathComponent]];
    }
    [self updateButtons];
    if ([delegate respondsToSelector:@selector(panelSelectionDidChange:)])
        [delegate panelSelectionDidChange:panel];
}

@end

static char PDFilePanelControllerKey, PDNameFieldValueKey;

@implementation NSSavePanel (NSFilePanelController)

- (id)_filePanelController
{
    NSFilePanelController *controller = objc_getAssociatedObject(self, &PDFilePanelControllerKey);

    if (controller == nil) {
        controller = [[[NSFilePanelController alloc] initWithPanel:self] autorelease];
        objc_setAssociatedObject(self, &PDFilePanelControllerKey, controller, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    return controller;
}

- (NSInteger)_runFilePanelModal
{
    return [(NSFilePanelController *)[self _filePanelController] runModal];
}

- (void)_beginFilePanelForWindow:(NSWindow *)window handler:(void (^)(NSInteger result))handler
{
    [[self _filePanelController] beginForWindow:window handler:handler];
}

- (void)_prepareFilePanelForDisplay
{
    [[self _filePanelController] prepare];
}

- (NSString *)_nameFieldValue
{
    NSString *value = objc_getAssociatedObject(self, &PDNameFieldValueKey);

    return value ?: @"Untitled";
}

- (void)_setNameFieldValue:(NSString *)name
{
    NSFilePanelController *controller = objc_getAssociatedObject(self, &PDFilePanelControllerKey);

    objc_setAssociatedObject(self, &PDNameFieldValueKey, name, OBJC_ASSOCIATION_COPY_NONATOMIC);
    if (controller != nil && controller->built)
        [controller->nameField setStringValue:name ?: @""];
}

- (NSArray *)_chosenURLs
{
    NSFilePanelController *controller = objc_getAssociatedObject(self, &PDFilePanelControllerKey);

    return [controller chosen] ?: [NSArray array];
}

// the field holds what the person typed, so reading it while the panel is up gives the live value
- (NSString *)_liveNameFieldValue
{
    NSFilePanelController *controller = objc_getAssociatedObject(self, &PDFilePanelControllerKey);

    if (controller != nil && controller->built && controller->mode != PDPanelIdle)
        return [controller->nameField stringValue];
    return [self _nameFieldValue];
}

- (void)ok:(id)sender
{
    NSFilePanelController *controller = objc_getAssociatedObject(self, &PDFilePanelControllerKey);

    if (controller == nil || controller->mode == PDPanelIdle)
        return;
    [controller confirm];
}

- (void)cancel:(id)sender
{
    NSFilePanelController *controller = objc_getAssociatedObject(self, &PDFilePanelControllerKey);

    if (controller == nil || controller->mode == PDPanelIdle)
        return;
    [controller cancel];
}

- (void)cancelOperation:(id)sender
{
    [self cancel:sender];
}

@end
