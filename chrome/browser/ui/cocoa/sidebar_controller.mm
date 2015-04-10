// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/sidebar_controller.h"

#include <Cocoa/Cocoa.h>

#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/sidebar/sidebar_manager.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"

namespace {

// By default sidebar width is 1/7th of the current page content width.
const CGFloat kDefaultSidebarWidthRatio = 1.0f / 7.0f;
const CGFloat kMaximumSidebarWidthRatio = 1.0f / 2.0f;

}  // end namespace

@interface SidebarController (Private)
- (void)showSidebarContents:(content::WebContents*)sidebarContents;
- (void)resizeSidebarToNewWidth:(CGFloat)width;
@end

@interface SidebarSplitView : NSSplitView
@end

@implementation SidebarSplitView
- (NSColor*)dividerColor {
  return [NSColor controlColor];
}
@end

@implementation SidebarController

- (id)initWithParentViewController:(id)parentController
             andContentsController:(id)contentsController {
  DCHECK(parentController);

  if (self = [super init]) {
    splitView_.reset([[SidebarSplitView alloc]
        initWithFrame:[[parentController view] bounds]]);
    [splitView_ setDelegate:self];
    [splitView_ setVertical:YES];
    [splitView_ setDividerStyle:NSSplitViewDividerStyleThin];
    [splitView_ setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [[parentController view] addSubview:splitView_];

    [splitView_ addSubview:[contentsController view]];
    [splitView_ adjustSubviews];
  }
  return self;
}

- (void)dealloc {
  [splitView_ setDelegate:nil];
  [super dealloc];
}

- (NSSplitView*)view {
  return splitView_.get();
}

- (void)updateSidebarForTabContents:(content::WebContents*)contents {
  // Get the active sidebar content.
  if (SidebarManager::GetInstance() == NULL)  // Happens in tests.
    return;

  content::WebContents* sidebarContents = NULL;
  if (contents) {
    SidebarContainer* activeSidebar =
        SidebarManager::GetInstance()->GetActiveSidebarContainerFor(contents);
    if (!activeSidebar)
      activeSidebar = SidebarManager::GetInstance()->MigrateSidebarTo(contents);
    if (activeSidebar)
      sidebarContents = activeSidebar->host_contents();
  }

  if (!contentsController_.get())
    contentsController_.reset(
        [[TabContentsController alloc] initWithContents:contents]);

  content::WebContents* oldSidebarContents =
      static_cast<content::WebContents*>([contentsController_ webContents]);
  if (oldSidebarContents == sidebarContents)
    return;

  // Adjust sidebar view.
  [self showSidebarContents:sidebarContents];

  // Notify extensions.
  SidebarManager::GetInstance()->NotifyStateChanges(oldSidebarContents,
                                                    sidebarContents);
}

- (void)ensureContentsVisible {
  [contentsController_ ensureContentsVisible];
}

- (void)showSidebarContents:(content::WebContents*)sidebarContents {
  [contentsController_ ensureContentsSizeDoesNotChange];

  NSArray* subviews = [splitView_ subviews];
  if (sidebarContents) {
    DCHECK_GE([subviews count], 1u);

    // Native view is a TabContentsViewCocoa object, whose ViewID was
    // set to VIEW_ID_TAB_CONTAINER initially, so change it to
    // VIEW_ID_SIDE_BAR_CONTAINER here.
    view_id_util::SetID(sidebarContents->GetNativeView(),
                        VIEW_ID_SIDE_BAR_CONTAINER);

    CGFloat sidebarWidth = 0;
    if ([subviews count] == 1) {
      // Load the default split offset.
      sidebarWidth = g_browser_process->local_state()->GetInteger(
          prefs::kExtensionSidebarWidth);
      if (sidebarWidth < 0) {
        // Initial load, set to default value.
        sidebarWidth = NSWidth([splitView_ frame]) * kDefaultSidebarWidthRatio;
      }

      [splitView_ addSubview:[contentsController_ view]];
    } else {
      DCHECK_EQ([subviews count], 2u);
      sidebarWidth = NSWidth([[subviews objectAtIndex:1] frame]);
    }

    sidebarWidth = std::max(static_cast<CGFloat>(0), sidebarWidth);

    [self resizeSidebarToNewWidth:sidebarWidth];
  } else {
    if ([subviews count] > 1) {
      NSView* oldSidebarContentsView = [subviews objectAtIndex:1];
      // Store split offset when hiding sidebar window only.
      int sidebarWidth = NSWidth([oldSidebarContentsView frame]);
      g_browser_process->local_state()->SetInteger(
          prefs::kExtensionSidebarWidth, sidebarWidth);
      [oldSidebarContentsView removeFromSuperview];
      [splitView_ adjustSubviews];
    }
  }

  [contentsController_ changeWebContents:sidebarContents];
}

- (void)resizeSidebarToNewWidth:(CGFloat)width {
  NSArray* subviews = [splitView_ subviews];

  NSView* sidebarView = [subviews objectAtIndex:1];
  NSRect sidebarFrame = [sidebarView frame];
  sidebarFrame.size.width = width;
  [sidebarView setFrame:sidebarFrame];

  NSView* webView = [subviews objectAtIndex:0];
  NSRect webFrame = [webView frame];
  webFrame.size.width =
      NSWidth([splitView_ frame]) - ([splitView_ dividerThickness] + width);
  [webView setFrame:webFrame];

  [splitView_ adjustSubviews];
}

/* NSSplitViewDelegate Support
 *
 * Sidebar behavior:
 * - initial sidebar is kDefaultSidebarWidthRatio * width
 *   of the split-view's frame
 * - sidebar width is not allowed to be greater than 50% of width of the
 *   the split-view's frame
 *
 */

- (BOOL)splitView:(NSSplitView*)splitView
    shouldHideDividerAtIndex:(NSInteger)dividerIndex {
  return NO;
}

- (BOOL)splitView:(NSSplitView*)splitView canCollapseSubview:(NSView*)subview {
  return NO;
}

- (BOOL)splitView:(NSSplitView*)splitView
             shouldCollapseSubview:(NSView*)subview
    forDoubleClickOnDividerAtIndex:(NSInteger)dividerIndex {
  return NO;
}

- (CGFloat)splitView:(NSSplitView*)splitView
    constrainMinCoordinate:(CGFloat)proposedMinimumPosition
               ofSubviewAt:(NSInteger)dividerIndex {
  return std::max(proposedMinimumPosition,
                  kMaximumSidebarWidthRatio * NSWidth([splitView_ frame]));
}

- (CGFloat)splitView:(NSSplitView*)splitView
    constrainMaxCoordinate:(CGFloat)proposedMaximumPosition
               ofSubviewAt:(NSInteger)dividerIndex {
  return std::min(proposedMaximumPosition,
                  NSWidth([splitView_ frame]) - [splitView_ dividerThickness]);
}

@end
