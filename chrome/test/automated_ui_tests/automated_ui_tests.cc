// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <fstream>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/libxml_utils.h"
#include "chrome/common/rand_util.h"
#include "chrome/common/win_util.h"
#include "chrome/test/automated_ui_tests/automated_ui_tests.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/views/view.h"
#include "googleurl/src/gurl.h"

namespace {

const wchar_t* const kReproSwitch = L"key";

const wchar_t* const kReproRepeatSwitch = L"num-reproductions";

const wchar_t* const kInputFilePathSwitch = L"input";

const wchar_t* const kOutputFilePathSwitch = L"output";

const wchar_t* const kDebugModeSwitch = L"debug";

const wchar_t* const kDefaultInputFilePath = L"C:\\automated_ui_tests.txt";

const wchar_t* const kDefaultOutputFilePath
  = L"C:\\automated_ui_tests_error_report.txt";

const int kDebuggingTimeoutMsec = 5000;

// How many commands to run when testing a dialog box.
const int kTestDialogActionsToRun = 7;

}  // namespace

// This subset of commands is used to test dialog boxes, which aren't likely
// to respond to most other commands.
std::string AutomatedUITest::test_dialog_possible_actions_[] = {
  "PressTabKey",
  "PressEnterKey",
  "PressSpaceBar",
  "DownArrow"
};

AutomatedUITest::AutomatedUITest()
    : total_crashes_(0),
      debug_logging_enabled_(false) {
  show_window_ = true;
  GetSystemTimeAsFileTime(&test_start_time_);
  CommandLine parsed_command_line;
  if (parsed_command_line.HasSwitch(kDebugModeSwitch))
    debug_logging_enabled_ = true;
}

AutomatedUITest::~AutomatedUITest() {}

void AutomatedUITest::RunReproduction() {
  CommandLine parsed_command_line;
  xml_writer_.StartWriting();
  xml_writer_.StartElement("Report");
  std::string action_string =
      WideToASCII(parsed_command_line.GetSwitchValue(kReproSwitch));

  int64 num_reproductions = 1;
  if (parsed_command_line.HasSwitch(kReproRepeatSwitch)) {
    std::wstring num_reproductions_string =
        parsed_command_line.GetSwitchValue(kReproRepeatSwitch);
    std::string test = WideToASCII(num_reproductions_string);
    num_reproductions = StringToInt64(num_reproductions_string);
  }
  std::vector<std::string> actions;
  SplitString(action_string, L',', &actions);
  bool did_crash = false;
  bool command_complete = false;

  for (int64 i = 0; i < num_reproductions && !did_crash; ++i) {
    bool did_teardown = false;
    xml_writer_.StartElement("Executed");
    for (size_t j = 0; j < actions.size(); ++j) {
      DoAction(actions[j]);
      if (DidCrash(true)) {
        did_crash = true;
        if (j >= (actions.size() - 1))
          command_complete = true;
        break;
      }
      if (LowerCaseEqualsASCII(actions[j], "teardown"))
        did_teardown = true;
    }

    // Force proper teardown after each run, if it didn't already happen. But
    // don't teardown after crashes.
    if (!did_teardown && !did_crash)
      DoAction("TearDown");

    xml_writer_.EndElement();  // End "Executed" element.
  }

  if (did_crash) {
    std::string crash_dump = WideToASCII(GetMostRecentCrashDump());
    std::string result =
        "*** Crash dump produced. See result file for more details. Dump = ";
    result.append(crash_dump);
    result.append(" ***\n");
    printf("%s", result.c_str());
    LogCrashResult(crash_dump, command_complete);
    EXPECT_TRUE(false) << "Crash detected.";
  } else {
    printf("*** No crashes. See result file for more details. ***\n");
    LogSuccessResult();
  }

  WriteReportToFile();
}


void AutomatedUITest::RunAutomatedUITest() {
  ASSERT_TRUE(InitXMLReader()) << "Error initializing XMLReader";
  xml_writer_.StartWriting();
  xml_writer_.StartElement("Report");

  while (init_reader_.Read()) {
    init_reader_.SkipToElement();
    std::string node_name = init_reader_.NodeName();
    if (LowerCaseEqualsASCII(node_name, "command")) {
      bool no_errors = true;
      xml_writer_.StartElement("Executed");
      std::string command_number;
      if (init_reader_.NodeAttribute("number", &command_number)) {
        xml_writer_.AddAttribute("command_number", command_number);
      }
      xml_writer_.StopIndenting();

      // Starts the browser, logging it as an action.
      DoAction("SetUp");

      // Record the depth of the root of the command subtree, then advance to
      // the first element in preperation for parsing.
      int start_depth = init_reader_.Depth();
      ASSERT_TRUE(init_reader_.Read()) << "Malformed XML file.";
      init_reader_.SkipToElement();

      // Check for a crash right after startup.
      if (DidCrash(true)) {
        LogCrashResult(WideToASCII(GetMostRecentCrashDump()), false);
        // Try and start up again.
        CloseBrowserAndServer();
        LaunchBrowserAndServer();
        if (DidCrash(true)) {
          no_errors = false;
          // We crashed again, so skip to the end of the this command.
          while (init_reader_.Depth() != start_depth) {
            ASSERT_TRUE(init_reader_.Read()) << "Malformed XML file.";
          }
        } else {
          // We didn't crash, so end the old element, logging a crash for that.
          // Then start a new element to log this command.
          xml_writer_.StartIndenting();
          xml_writer_.EndElement();
          xml_writer_.StartElement("Executed");
          xml_writer_.AddAttribute("command_number", command_number);
          xml_writer_.StopIndenting();
          xml_writer_.StartElement("SetUp");
          xml_writer_.EndElement();
        }
      }
      // Parse the command, performing the specified actions and checking
      // for a crash after each one.
      while (init_reader_.Depth() != start_depth) {
        node_name = init_reader_.NodeName();

        DoAction(node_name);

        // Advance to the next element
        ASSERT_TRUE(init_reader_.Read()) << "Malformed XML file.";
        init_reader_.SkipToElement();
        if (DidCrash(true)) {
          no_errors = false;
          // This was the last action if we've returned to the initial depth
          // of the command subtree.
          bool wasLastAction = init_reader_.Depth() == start_depth;
          LogCrashResult(WideToASCII(GetMostRecentCrashDump()), wasLastAction);
          // Skip to the beginning of the next command.
          while (init_reader_.Depth() != start_depth) {
            ASSERT_TRUE(init_reader_.Read()) << "Malformed XML file.";
          }
        }
      }

      if (no_errors) {
        // If there were no previous crashes, log our tear down and check for
        // a crash, log success for the entire command if this doesn't crash.
        DoAction("TearDown");
        if (DidCrash(true))
          LogCrashResult(WideToASCII(GetMostRecentCrashDump()), true);
        else
          LogSuccessResult();
      } else {
        // If there was a previous crash, just tear down without logging, so
        // that we know what the last command was before we crashed.
        CloseBrowserAndServer();
      }

      xml_writer_.StartIndenting();
      xml_writer_.EndElement();  // End "Executed" element.
    }
  }
  // The test is finished so write our report.
  WriteReportToFile();
}

bool AutomatedUITest::DoAction(const std::string & action) {
  bool did_complete_action = false;
  xml_writer_.StartElement(action);
  if (debug_logging_enabled_)
    AppendToOutputFile(action);

  if (LowerCaseEqualsASCII(action, "navigate")) {
    did_complete_action = Navigate();
  } else if (LowerCaseEqualsASCII(action, "newtab")) {
    did_complete_action = NewTab();
  } else if (LowerCaseEqualsASCII(action, "back")) {
    did_complete_action = BackButton();
  } else if (LowerCaseEqualsASCII(action, "forward")) {
    did_complete_action = ForwardButton();
  } else if (LowerCaseEqualsASCII(action, "closetab")) {
    did_complete_action = CloseActiveTab();
  } else if (LowerCaseEqualsASCII(action, "openwindow")) {
    did_complete_action = OpenAndActivateNewBrowserWindow();
  } else if (LowerCaseEqualsASCII(action, "reload")) {
    did_complete_action = ReloadPage();
  } else if (LowerCaseEqualsASCII(action, "star")) {
    did_complete_action = StarPage();
  } else if (LowerCaseEqualsASCII(action, "findinpage")) {
    did_complete_action = FindInPage();
  } else if (LowerCaseEqualsASCII(action, "selectnexttab")) {
    did_complete_action = SelectNextTab();
  } else if (LowerCaseEqualsASCII(action, "selectprevtab")) {
    did_complete_action = SelectPreviousTab();
  } else if (LowerCaseEqualsASCII(action, "zoomplus")) {
    did_complete_action = ZoomPlus();
  } else if (LowerCaseEqualsASCII(action, "zoomminus")) {
    did_complete_action = ZoomMinus();
  } else if (LowerCaseEqualsASCII(action, "history")) {
    did_complete_action = ShowHistory();
  } else if (LowerCaseEqualsASCII(action, "downloads")) {
    did_complete_action = ShowDownloads();
  } else if (LowerCaseEqualsASCII(action, "importsettings")) {
    did_complete_action = ImportSettings();
  } else if (LowerCaseEqualsASCII(action, "viewpasswords")) {
    did_complete_action = ViewPasswords();
  } else if (LowerCaseEqualsASCII(action, "clearbrowserdata")) {
    did_complete_action = ClearBrowserData();
  } else if (LowerCaseEqualsASCII(action, "taskmanager")) {
    did_complete_action = TaskManager();
  } else if (LowerCaseEqualsASCII(action, "goofftherecord")) {
    did_complete_action = GoOffTheRecord();
  } else if (LowerCaseEqualsASCII(action, "pressescapekey")) {
    did_complete_action = PressEscapeKey();
  } else if (LowerCaseEqualsASCII(action, "presstabkey")) {
    did_complete_action = PressTabKey();
  } else if (LowerCaseEqualsASCII(action, "pressenterkey")) {
    did_complete_action = PressEnterKey();
  } else if (LowerCaseEqualsASCII(action, "pressspacebar")) {
    did_complete_action = PressSpaceBar();
  } else if (LowerCaseEqualsASCII(action, "pagedown")) {
    did_complete_action = PageDown();
  } else if (LowerCaseEqualsASCII(action, "pageup")) {
    did_complete_action = PageUp();
  } else if (LowerCaseEqualsASCII(action, "dragtabright")) {
    did_complete_action = DragActiveTab(true, false);
  } else if (LowerCaseEqualsASCII(action, "dragtableft")) {
    did_complete_action = DragActiveTab(false, false);
  } else if (LowerCaseEqualsASCII(action, "dragtabout")) {
    did_complete_action = DragActiveTab(false, true);
  } else if (LowerCaseEqualsASCII(action, "uparrow")) {
    did_complete_action = UpArrow();
  } else if (LowerCaseEqualsASCII(action, "downarrow")) {
    did_complete_action = DownArrow();
  } else if (LowerCaseEqualsASCII(action, "testeditkeywords")) {
    did_complete_action = TestEditKeywords();
  } else if (LowerCaseEqualsASCII(action, "testtaskmanager")) {
    did_complete_action = TestTaskManager();
  } else if (LowerCaseEqualsASCII(action, "testviewpasswords")) {
    did_complete_action = TestViewPasswords();
  } else if (LowerCaseEqualsASCII(action, "testclearbrowserdata")) {
    did_complete_action = TestClearBrowserData();
  } else if (LowerCaseEqualsASCII(action, "testimportsettings")) {
    did_complete_action = TestImportSettings();
  } else if (LowerCaseEqualsASCII(action, "crash")) {
    did_complete_action = ForceCrash();
  } else if (LowerCaseEqualsASCII(action, "sleep")) {
    // This is for debugging, it probably shouldn't be used real tests.
    Sleep(kDebuggingTimeoutMsec);
    did_complete_action = true;
  } else if (LowerCaseEqualsASCII(action, "setup")) {
    LaunchBrowserAndServer();
    did_complete_action = true;
  } else if (LowerCaseEqualsASCII(action, "teardown")) {
    CloseBrowserAndServer();
    did_complete_action = true;
  }

  if (!did_complete_action)
    xml_writer_.AddAttribute("failed_to_complete", "yes");
  xml_writer_.EndElement();

  return did_complete_action;
}

bool AutomatedUITest::Navigate() {
  scoped_ptr<BrowserProxy> browser(automation()->GetLastActiveBrowserWindow());
  if (browser.get() == NULL) {
    AddErrorAttribute("browser_window_not_found");
    return false;
  }
  bool did_timeout;
  scoped_ptr<TabProxy> tab(
      browser->GetActiveTabWithTimeout(kWaitForActionMaxMsec, &did_timeout));
  // TODO(devint): This might be masking a bug. I can't think of many
  // valid cases where we would get a browser window, but not be able
  // to return an active tab. Yet this has happened and has triggered crashes.
  // Investigate this.
  if (tab.get() == NULL) {
    AddErrorAttribute("active_tab_not_found");
    return false;
  }
  std::string url = "about:blank";
  if (init_reader_.NodeAttribute("url", &url)) {
    xml_writer_.AddAttribute("url", url);
  }
  GURL test_url(url);
  did_timeout = false;
  tab->NavigateToURLWithTimeout(test_url, kMaxTestExecutionTime, &did_timeout);

  if (did_timeout) {
    AddWarningAttribute("timeout");
    return false;
  }
  return true;
}

bool AutomatedUITest::NewTab() {
  scoped_ptr<BrowserProxy> browser(automation()->GetLastActiveBrowserWindow());
  if (browser.get() == NULL) {
    AddErrorAttribute("browser_window_not_found");
    return false;
  }
  int old_tab_count;
  int new_tab_count;
  bool is_timeout;
  browser->GetTabCountWithTimeout(&old_tab_count, kWaitForActionMaxMsec,
                                  &is_timeout);
  // Apply accelerator and wait for a new tab to open, if either
  // fails, return false. Apply Accelerator takes care of logging its failure.
  bool return_value = ApplyAccelerator(IDC_NEWTAB);
  if (!browser->WaitForTabCountToChange(
      old_tab_count, &new_tab_count, kWaitForActionMaxMsec)) {
    AddWarningAttribute("tab_count_failed_to_change");
    return false;
  }
  return return_value;
}

bool AutomatedUITest::BackButton() {
  return ApplyAccelerator(IDC_BACK);
}

bool AutomatedUITest::ForwardButton() {
  return ApplyAccelerator(IDC_FORWARD);
}

bool AutomatedUITest::CloseActiveTab() {
  bool return_value = false;
  scoped_ptr<BrowserProxy> browser(automation()->GetLastActiveBrowserWindow());
  if (browser.get() == NULL) {
    AddErrorAttribute("browser_window_not_found");
    return false;
  }
  int browser_windows_count;
  int tab_count;
  bool is_timeout;
  browser->GetTabCountWithTimeout(&tab_count, kWaitForActionMaxMsec,
                                  &is_timeout);
  automation()->GetBrowserWindowCount(&browser_windows_count);
  // Avoid quitting the application by not closing the last window.
  if (tab_count > 1) {
    int new_tab_count;
    return_value = browser->ApplyAccelerator(IDC_CLOSETAB);
    // Wait for the tab to close before we continue.
    if (!browser->WaitForTabCountToChange(
        tab_count, &new_tab_count, kWaitForActionMaxMsec)) {
      AddWarningAttribute("tab_count_failed_to_change");
      return false;
    }
  } else if (tab_count == 1 && browser_windows_count > 1) {
    int new_window_count;
    return_value = browser->ApplyAccelerator(IDC_CLOSETAB);
    // Wait for the window to close before we continue.
    if (!automation()->WaitForWindowCountToChange(
        browser_windows_count, &new_window_count, kWaitForActionMaxMsec)) {
      AddWarningAttribute("window_count_failed_to_change");
      return false;
    }
  } else {
    AddInfoAttribute("would_have_exited_application");
    return false;
  }
  return return_value;
}

bool AutomatedUITest::OpenAndActivateNewBrowserWindow() {
  if (!automation()->OpenNewBrowserWindow(SW_SHOWNORMAL)) {
    AddWarningAttribute("failed_to_open_new_browser_window");
    return false;
  }
  int num_browser_windows;
  automation()->GetBrowserWindowCount(&num_browser_windows);
  // Get the most recently opened browser window and activate the tab
  // in order to activate this browser window.
  scoped_ptr<BrowserProxy> browser(
    automation()->GetBrowserWindow(num_browser_windows - 1));
  if (browser.get() == NULL) {
    AddErrorAttribute("browser_window_not_found");
    return false;
  }
  bool is_timeout;
  if (!browser->ActivateTabWithTimeout(0, kWaitForActionMaxMsec,
                                       &is_timeout)) {
    AddWarningAttribute("failed_to_activate_tab");
    return false;
  }
  return true;
}

bool AutomatedUITest::ReloadPage() {
  return ApplyAccelerator(IDC_RELOAD);
}

bool AutomatedUITest::StarPage() {
  return ApplyAccelerator(IDC_STAR);
}

bool AutomatedUITest::FindInPage() {
  return ApplyAccelerator(IDC_FIND);
}

bool AutomatedUITest::SelectNextTab() {
  return ApplyAccelerator(IDC_SELECT_NEXT_TAB);
}

bool AutomatedUITest::SelectPreviousTab() {
  return ApplyAccelerator(IDC_SELECT_PREV_TAB);
}

bool AutomatedUITest::ZoomPlus() {
  return ApplyAccelerator(IDC_ZOOM_PLUS);
}

bool AutomatedUITest::ZoomMinus() {
  return ApplyAccelerator(IDC_ZOOM_MINUS);
}

bool AutomatedUITest::ShowHistory() {
  return ApplyAccelerator(IDC_SHOW_HISTORY);
}

bool AutomatedUITest::ShowDownloads() {
  return ApplyAccelerator(IDC_SHOW_DOWNLOADS);
}

bool AutomatedUITest::ImportSettings() {
  return ApplyAccelerator(IDC_IMPORT_SETTINGS);
}

bool AutomatedUITest::ViewPasswords() {
  return ApplyAccelerator(IDC_VIEW_PASSWORDS);
}

bool AutomatedUITest::ClearBrowserData() {
  return ApplyAccelerator(IDC_CLEAR_BROWSING_DATA);
}

bool AutomatedUITest::TaskManager() {
  return ApplyAccelerator(IDC_TASKMANAGER);
}

bool AutomatedUITest::GoOffTheRecord() {
  return ApplyAccelerator(IDC_GOOFFTHERECORD);
}

bool AutomatedUITest::PressEscapeKey() {
  return SimulateKeyPressInActiveWindow(VK_ESCAPE, 0);
}

bool AutomatedUITest::PressTabKey() {
  return SimulateKeyPressInActiveWindow(VK_TAB, 0);
}

bool AutomatedUITest::PressEnterKey() {
  return SimulateKeyPressInActiveWindow(VK_RETURN, 0);
}

bool AutomatedUITest::PressSpaceBar() {
  return SimulateKeyPressInActiveWindow(VK_SPACE, 0);
}

bool AutomatedUITest::PageDown() {
  return SimulateKeyPressInActiveWindow(VK_PRIOR, 0);
}


bool AutomatedUITest::PageUp() {
  return SimulateKeyPressInActiveWindow(VK_NEXT, 0);
}

bool AutomatedUITest::UpArrow() {
  return SimulateKeyPressInActiveWindow(VK_UP, 0);
}

bool AutomatedUITest::DownArrow() {
  return SimulateKeyPressInActiveWindow(VK_DOWN, 0);
}

bool AutomatedUITest::TestEditKeywords() {
  DoAction("EditKeywords");
  return TestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::TestTaskManager() {
  DoAction("TaskManager");
  return TestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::TestViewPasswords() {
  DoAction("ViewPasswords");
  return TestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::TestClearBrowserData() {
  DoAction("ClearBrowserData");
  return TestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::TestImportSettings() {
  DoAction("ImportSettings");
  return TestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::TestDialog(int num_actions) {
  bool return_value = true;

  for (int i = 0; i < num_actions; i++) {
    int action_index = rand_util::RandInt(0, kNumTestDialogActions - 1);
    return_value = return_value &&
                   DoAction(test_dialog_possible_actions_[action_index]);
    if (DidCrash(false))
      break;
  }
  return DoAction("PressEscapeKey") && return_value;
}

bool AutomatedUITest::ForceCrash() {
  scoped_ptr<BrowserProxy> browser(automation()->GetLastActiveBrowserWindow());
  if (browser.get() == NULL) {
    AddErrorAttribute("browser_window_not_found");
    return false;
  }
  scoped_ptr<TabProxy> tab(browser->GetActiveTab());
  GURL test_url("about:crash");
  bool did_timeout;
  tab->NavigateToURLWithTimeout(test_url, kDebuggingTimeoutMsec, &did_timeout);
  if (!did_timeout) {
    AddInfoAttribute("expected_crash");
    return false;
  }
  return true;
}

bool AutomatedUITest::DragActiveTab(bool drag_right, bool drag_out) {
  scoped_ptr<BrowserProxy> browser(automation()->GetLastActiveBrowserWindow());
  scoped_ptr<WindowProxy> window(
      GetAndActivateWindowForBrowser(browser.get()));
  if (window.get() == NULL) {
    AddErrorAttribute("active_window_not_found");
    return false;
  }
  bool is_timeout;

  if (browser.get() == NULL) {
    AddErrorAttribute("browser_window_not_found");
    return false;
  }
  int tab_count;
  browser->GetTabCountWithTimeout(&tab_count, kWaitForActionMaxMsec,
                                  &is_timeout);
  // As far as we're concerned, if we can't get a view for a tab, it doesn't
  // exist, so cap tab_count at the number of tab view ids there are.
  tab_count = std::min(tab_count, VIEW_ID_TAB_LAST - VIEW_ID_TAB_0);

  int tab_index;
  if (!browser->GetActiveTabIndexWithTimeout(&tab_index,
                                             kWaitForActionMaxMsec,
                                             &is_timeout)) {
    AddWarningAttribute("no_active_tab");
    return false;
  }

  gfx::Rect dragged_tab_bounds;
  if (!window->GetViewBoundsWithTimeout(VIEW_ID_TAB_0 + tab_index,
                                        &dragged_tab_bounds, false,
                                        kWaitForActionMaxMsec,
                                        &is_timeout)) {
    AddWarningAttribute("no_tab_view_found");
    return false;
  }

  // Click on the center of the tab, and drag it to the left or the right.
  POINT dragged_tab_point(dragged_tab_bounds.CenterPoint().ToPOINT());
  POINT destination_point(dragged_tab_point);

  int window_count;
  if (drag_out) {
    destination_point.y += 3*dragged_tab_bounds.height();
    automation()->GetBrowserWindowCount(&window_count);
  } else if (drag_right) {
    if (tab_index >= (tab_count-1)) {
      AddInfoAttribute("index_cant_be_moved");
      return false;
    }
    destination_point.x += 2*dragged_tab_bounds.width()/3;
  } else {
    if (tab_index <= 0) {
      AddInfoAttribute("index_cant_be_moved");
      return false;
    }
    destination_point.x -= 2*dragged_tab_bounds.width()/3;
  }

  if (!browser->SimulateDragWithTimeout(dragged_tab_point,
                                        destination_point,
                                        ChromeViews::Event::EF_LEFT_BUTTON_DOWN,
                                        kWaitForActionMaxMsec,
                                        &is_timeout)) {
    AddWarningAttribute("failed_to_simulate_drag");
    return false;
  }

  // If we try to drag the tab out and the window we drag from contains more
  // than just the dragged tab, we would expect the window count to increase
  // because the dragged tab should open in a new window. If not, we probably
  // just dragged into another tabstrip.
  if (drag_out && tab_count > 1) {
      int new_window_count;
      automation()->GetBrowserWindowCount(&new_window_count);
      if (new_window_count == window_count) {
        AddInfoAttribute("no_new_browser_window");
        return false;
      }
  }
  return true;
}

WindowProxy* AutomatedUITest::GetAndActivateWindowForBrowser(
    BrowserProxy* browser) {
  WindowProxy* window = automation()->GetWindowForBrowser(browser);

  bool did_timeout;
  if (!browser->BringToFrontWithTimeout(kWaitForActionMaxMsec, &did_timeout)) {
    AddWarningAttribute("failed_to_bring_window_to_front");
    return NULL;
  }
  return window;
}

bool AutomatedUITest::ApplyAccelerator(int id) {
  scoped_ptr<BrowserProxy> browser(automation()->GetLastActiveBrowserWindow());
  if (browser.get() == NULL) {
    AddErrorAttribute("browser_window_not_found");
    return false;
  }
  if (!browser->ApplyAccelerator(id)) {
    AddWarningAttribute("failure_applying_accelerator");
    return false;
  }
  return true;
}

bool AutomatedUITest::SimulateKeyPressInActiveWindow(wchar_t key, int flags) {
  scoped_ptr<BrowserProxy> browser(automation()->GetLastActiveBrowserWindow());
  scoped_ptr<WindowProxy> window(
      GetAndActivateWindowForBrowser(browser.get()));
  if (window.get() == NULL) {
    AddErrorAttribute("active_window_not_found");
    return false;
  }
  if (!window->SimulateOSKeyPress(key, flags)) {
    AddWarningAttribute("failure_simulating_key_press");
    return false;
  }
  return true;
}

bool AutomatedUITest::InitXMLReader() {
  std::wstring input_path;
  CommandLine parsed_command_line;
  if (parsed_command_line.HasSwitch(kInputFilePathSwitch))
    input_path = parsed_command_line.GetSwitchValue(kInputFilePathSwitch);
  else
    input_path = kDefaultInputFilePath;

  if (!file_util::ReadFileToString(input_path, &xml_init_file_))
    return false;
  return init_reader_.Load(xml_init_file_);
}

bool AutomatedUITest::WriteReportToFile() {
  std::ofstream error_file;
  std::wstring path;
  CommandLine parsed_command_line;
  if (parsed_command_line.HasSwitch(kOutputFilePathSwitch))
    path = parsed_command_line.GetSwitchValue(kOutputFilePathSwitch);
  else
    path = kDefaultOutputFilePath;

  if (!path.empty())
    error_file.open(path.c_str(), std::ios::out);

  // Closes all open elements and free the writer. This is required
  // in order to retrieve the contents of the buffer.
  xml_writer_.StopWriting();
  error_file << xml_writer_.GetWrittenString();
  error_file.close();
  return true;
}

void AutomatedUITest::AppendToOutputFile(const std::string &append_string) {
  std::ofstream error_file;
  std::wstring path;
  CommandLine parsed_command_line;
  if (parsed_command_line.HasSwitch(kOutputFilePathSwitch))
    path = parsed_command_line.GetSwitchValue(kOutputFilePathSwitch);
  else
    path = kDefaultOutputFilePath;

  if (!path.empty())
    error_file.open(path.c_str(), std::ios::out | std::ios_base::app);

  error_file << append_string << " ";
  error_file.close();
}

void AutomatedUITest::LogCrashResult(const std::string &crash_dump,
                                     bool command_completed) {
  xml_writer_.StartElement("result");
  xml_writer_.StartElement("crash");
  xml_writer_.AddAttribute("crash_dump", crash_dump);
  if (command_completed)
    xml_writer_.AddAttribute("command_completed", "yes");
  else
    xml_writer_.AddAttribute("command_completed", "no");
  xml_writer_.EndElement();
  xml_writer_.EndElement();
}

void AutomatedUITest::LogSuccessResult() {
  xml_writer_.StartElement("result");
  xml_writer_.StartElement("success");
  xml_writer_.EndElement();
  xml_writer_.EndElement();
}

void AutomatedUITest::AddInfoAttribute(const std::string &info) {
  xml_writer_.AddAttribute("info", info);
}

void AutomatedUITest::AddWarningAttribute(const std::string &warning) {
  xml_writer_.AddAttribute("warning", warning);
}

void AutomatedUITest::AddErrorAttribute(const std::string &error) {
  xml_writer_.AddAttribute("error", error);
}

std::wstring AutomatedUITest::GetMostRecentCrashDump() {
  std::wstring crash_dump_path;
  int file_count = 0;
  FILETIME most_recent_file_time;
  std::wstring most_recent_file_name;
  WIN32_FIND_DATA find_file_data;

  PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dump_path);
  // All files in the given directory.
  std::wstring filename_spec = crash_dump_path + L"\\*";
  HANDLE find_handle = FindFirstFile(filename_spec.c_str(), &find_file_data);
  if (find_handle != INVALID_HANDLE_VALUE) {
    most_recent_file_time = find_file_data.ftCreationTime;
    most_recent_file_name = find_file_data.cFileName;
    do {
      // Don't count current or parent directories.
      if ((wcscmp(find_file_data.cFileName, L"..") == 0) ||
          (wcscmp(find_file_data.cFileName, L".") == 0))
        continue;

      long result = CompareFileTime(&find_file_data.ftCreationTime,
                                    &most_recent_file_time);

      // File was created on or after the current most recent file.
      if ((result == 1) || (result == 0)) {
        most_recent_file_time = find_file_data.ftCreationTime;
        most_recent_file_name = find_file_data.cFileName;
      }
    } while (FindNextFile(find_handle, &find_file_data));
    FindClose(find_handle);
  }

  if (most_recent_file_name.empty()) {
    return L"";
  } else {
    file_util::AppendToPath(&crash_dump_path, most_recent_file_name);
    return crash_dump_path;
  }
}

bool AutomatedUITest::DidCrash(bool update_total_crashes) {
  std::wstring crash_dump_path;
  PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dump_path);
  // Each crash creates two dump files, so we divide by two here.
  int actual_crashes = file_util::CountFilesCreatedAfter(
    crash_dump_path, test_start_time_) / 2;

  // If there are more crash dumps than the total dumps which we have recorded
  // then this is a new crash.
  if (actual_crashes > total_crashes_) {
    if (update_total_crashes)
      total_crashes_ = actual_crashes;
    return true;
  } else {
    return false;
  }
}

TEST_F(AutomatedUITest, TheOneAndOnlyTest) {
  CommandLine parsed_command_line;
  if (parsed_command_line.HasSwitch(kReproSwitch))
    RunReproduction();
  else
    RunAutomatedUITest();
}
