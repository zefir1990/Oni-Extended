#import <Cocoa/Cocoa.h>

extern "C" {
#include "BFW.h"
#include "Oni_QuitShortcuts_macOS.h"
}

static id sOniQuitShortcutMonitor;

static BOOL ONiIsQuitShortcutKeyDown(NSEvent *inEvent)
{
	if ((inEvent.modifierFlags & NSEventModifierFlagCommand) == 0) {
		return NO;
	}

	NSString *key = inEvent.charactersIgnoringModifiers;
	if (([key caseInsensitiveCompare:@"w"] != NSOrderedSame) &&
		([key caseInsensitiveCompare:@"q"] != NSOrderedSame)) {
		return NO;
	}

	return YES;
}

static BOOL ONiMenuItemIsQuitShortcut(NSMenuItem *inMenuItem)
{
	SEL action = inMenuItem.action;
	if ((action == @selector(performClose:)) || (action == @selector(terminate:))) {
		return YES;
	}

	if ((inMenuItem.keyEquivalentModifierMask & NSEventModifierFlagCommand) == 0) {
		return NO;
	}

	NSString *keyEquivalent = inMenuItem.keyEquivalent;
	return ([keyEquivalent caseInsensitiveCompare:@"w"] == NSOrderedSame) ||
		([keyEquivalent caseInsensitiveCompare:@"q"] == NSOrderedSame);
}

static void ONiDisableQuitShortcutMenuItems(void)
{
	NSMenu *mainMenu = [NSApp mainMenu];
	if (mainMenu == nil) {
		return;
	}

	for (NSMenuItem *topLevelItem in [mainMenu itemArray]) {
		for (NSMenuItem *item in [topLevelItem.submenu itemArray]) {
			if (ONiMenuItemIsQuitShortcut(item)) {
				item.keyEquivalent = @"";
			}
		}
	}
}

void OniMac_DisableQuitShortcuts(void)
{
	const char *envRestoreQuitShortcuts = getenv("ONI_QUIT_SHORTCUTS");
	if (envRestoreQuitShortcuts != NULL &&
		envRestoreQuitShortcuts[0] != '\0' &&
		envRestoreQuitShortcuts[0] != '0') {
		UUrStartupMessage("[window] ONI_QUIT_SHORTCUTS set — Cmd-W/Cmd-Q quit shortcuts left enabled");
		return;
	}

	ONiDisableQuitShortcutMenuItems();

	if (sOniQuitShortcutMonitor != nil) {
		return;
	}

	sOniQuitShortcutMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown
		handler:^NSEvent *(NSEvent *event) {
			return ONiIsQuitShortcutKeyDown(event) ? nil : event;
		}];

	UUrStartupMessage("[window] Cmd-W/Cmd-Q quit shortcuts disabled");
}
