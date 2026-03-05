// Copyright 2020 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_COMPONENT_APP_HPP
#define FTXUI_COMPONENT_APP_HPP

#include <atomic>      // for atomic
#include <functional>  // for function
#include <memory>      // for shared_ptr
#include <string>      // for string

#include "ftxui/component/animation.hpp"       // for TimePoint
#include "ftxui/component/captured_mouse.hpp"  // for CapturedMouse
#include "ftxui/component/event.hpp"           // for Event
#include "ftxui/component/task.hpp"            // for Task, Closure
#include "ftxui/dom/selection.hpp"             // for SelectionOption
#include "ftxui/screen/screen.hpp"             // for Screen

namespace ftxui {
class ComponentBase;
class Loop;
struct Event;

using Component = std::shared_ptr<ComponentBase>;

namespace task {
class TaskRunner;
}

/// @brief App is a `Screen` that can handle events, run a main
/// loop, and manage components.
///
/// @ingroup component
class App : public Screen {
 public:
  /// @ingroup component
  /// @brief Configure DEC mouse reporting modes used while the screen is
  /// active.
  ///
  /// Each flag maps to one DEC private mode. The defaults match FTXUI's
  /// historical behavior.
  ///
  /// - `x10`            -> `?9h`
  /// - `vt200`          -> `?1000h`
  /// - `vt200_highlight`-> `?1001h`
  /// - `button_event`   -> `?1002h`
  /// - `any_event`      -> `?1003h`
  /// - `focus_event`    -> `?1004h`
  /// - `utf8`           -> `?1005h`
  /// - `sgr`            -> `?1006h`
  /// - `urxvt`          -> `?1015h`
  /// - `sgr_pixels`     -> `?1016h`
  struct MouseTrackingMode {
    bool x10 = false;
    bool vt200 = true;
    bool vt200_highlight = false;
    bool button_event = false;
    bool any_event = true;
    bool focus_event = false;
    bool utf8 = false;
    bool sgr = true;
    bool urxvt = true;
    bool sgr_pixels = false;

    /// @brief Disable all mouse reporting modes.
    static MouseTrackingMode Disabled();
    /// @brief Enable hover-capable mouse reporting (default FTXUI behavior).
    static MouseTrackingMode Hover();
    /// @brief Enable click/drag reporting without hover motion.
    static MouseTrackingMode ButtonDrag();
  };

  /// @ingroup component
  /// @brief Configure cursor-shape querying/restoration behavior.
  ///
  /// This controls whether FTXUI requests the terminal current cursor shape
  /// (DECRQSS/DECSCUSR) and whether that shape is restored on exit.
  struct CursorShapeMode {
    bool query_current_shape = true;
    bool restore_shape_on_exit = true;

    /// @brief Query and restore cursor shape (default behavior).
    static CursorShapeMode Managed();
    /// @brief Only ensure the cursor is visible on exit, skip shape query and
    /// restoration.
    static CursorShapeMode VisibleOnly();
  };

  /// @ingroup component
  /// @brief Options for temporary terminal handoff when running native I/O.
  ///
  /// This is intended for functions that temporarily leave FTXUI control,
  /// execute terminal-native code (for example SSH or pager), then restore
  /// FTXUI.
  struct TerminalHandoffOptions {
    /// Mouse mode applied for the handoff window.
    MouseTrackingMode mouse_mode = MouseTrackingMode::Disabled();
    /// Cursor-shape mode applied for the handoff window.
    CursorShapeMode cursor_mode = CursorShapeMode::VisibleOnly();
    /// Restore previous mouse mode after handoff.
    bool restore_mouse_mode = true;
    /// Restore previous cursor-shape mode after handoff.
    bool restore_cursor_mode = true;
    /// Disable local stdin echo during the handoff body.
    bool suppress_stdin_echo = true;
    /// Flush stdin before invoking handoff body.
    bool flush_stdin_before = true;
    /// Flush stdin after invoking handoff body.
    bool flush_stdin_after = true;
  };

  // Constructors:
  static App FixedSize(int dimx, int dimy);
  static App Fullscreen();
  static App FullscreenPrimaryScreen();
  static App FullscreenAlternateScreen();
  static App FitComponent();
  static App TerminalOutput();

  // Destructor.
  ~App() override;

  // Options. Must be called before Loop().
  void TrackMouse(bool enable = true);
  /// @ingroup component
  /// @brief Set explicit mouse-reporting modes for the active screen.
  /// @note Call before `Loop()`, or from callbacks executed on the UI thread.
  void SetMouseTrackingMode(const MouseTrackingMode& mode);
  /// @ingroup component
  /// @brief Return the configured mouse-reporting mode.
  MouseTrackingMode GetMouseTrackingMode() const;
  /// @ingroup component
  /// @brief Temporarily apply mouse-reporting mode while executing `fn`.
  Closure WithMouseTrackingMode(MouseTrackingMode mode, Closure fn);
  /// @ingroup component
  /// @brief Set cursor-shape management mode.
  /// @note Call before `Loop()`, or from callbacks executed on the UI thread.
  void SetCursorShapeMode(const CursorShapeMode& mode);
  /// @ingroup component
  /// @brief Return the configured cursor-shape management mode.
  CursorShapeMode GetCursorShapeMode() const;
  /// @ingroup component
  /// @brief Temporarily apply cursor-shape mode while executing `fn`.
  Closure WithCursorShapeMode(CursorShapeMode mode, Closure fn);
  void HandlePipedInput(bool enable = true);

  // Return the currently active app, nullptr if none.
  static App* Active();

  // Start/Stop the main loop.
  void Loop(Component);
  void Exit();
  Closure ExitLoopClosure();

  // Post tasks to be executed by the loop.
  void Post(Task task);
  void PostEvent(Event event);
  void RequestAnimationFrame();

  CapturedMouse CaptureMouse();

  // Decorate a function. The outputted one will execute similarly to the
  // inputted one, but with the currently active app terminal hooks
  // temporarily uninstalled.
  Closure WithRestoredIO(Closure);
  /// @ingroup component
  /// @brief Run `fn` in a temporary terminal handoff with default options.
  Closure WithTerminalHandoff(Closure);
  /// @ingroup component
  /// @brief Run `fn` in a temporary terminal handoff with explicit options.
  ///
  /// During the handoff, terminal hooks are temporarily uninstalled, selected
  /// mouse/cursor modes are applied, and optional stdin echo/flush safeguards
  /// are used before restoring the interactive screen.
  Closure WithTerminalHandoff(TerminalHandoffOptions options, Closure);

  // FTXUI implements handlers for Ctrl-C and Ctrl-Z. By default, these handlers
  // are executed, even if the component catches the event. This avoid users
  // handling every event to be trapped in the application. However, in some
  // cases, the application may want to handle these events itself. In this
  // case, the application can force FTXUI to not handle these events by calling
  // the following functions with force=true.
  void ForceHandleCtrlC(bool force);
  void ForceHandleCtrlZ(bool force);

  // Selection API.
  std::string GetSelection();
  void SelectionChange(std::function<void()> callback);

 private:
  void ExitNow();

  void Install();
  void Uninstall();

  void PreMain();
  void PostMain();

  bool HasQuitted();
  void RunOnce(Component component);
  void RunOnceBlocking(Component component);

  void HandleTask(Component component, Task& task);
  bool HandleSelection(bool handled, Event event);
  void RefreshSelection();
  void Draw(Component component);
  std::string ResetCursorPosition();

  void TerminalSend(std::string_view);
  void TerminalFlush();

  void InstallPipedInputHandling();

  void Signal(int signal);

  void FetchTerminalEvents();

  void PostAnimationTask();

  App* suspended_screen_ = nullptr;
  enum class Dimension {
    FitComponent,
    Fixed,
    Fullscreen,
    TerminalOutput,
  };
  App(Dimension dimension, int dimx, int dimy, bool use_alternative_screen);

  const Dimension dimension_;
  const bool use_alternative_screen_;

  bool track_mouse_ = true;
  MouseTrackingMode mouse_tracking_mode_ = MouseTrackingMode::Hover();
  MouseTrackingMode active_mouse_tracking_mode_ = MouseTrackingMode::Disabled();
  CursorShapeMode cursor_shape_mode_ = CursorShapeMode::Managed();

  std::string set_cursor_position_;
  std::string reset_cursor_position_;

  std::atomic<bool> quit_{false};
  bool animation_requested_ = false;
  animation::TimePoint previous_animation_time_;

  int cursor_x_ = 1;
  int cursor_y_ = 1;

  std::uint64_t frame_count_ = 0;
  bool mouse_captured = false;
  bool previous_frame_resized_ = false;

  bool frame_valid_ = false;

  bool force_handle_ctrl_c_ = true;
  bool force_handle_ctrl_z_ = true;

  // Piped input handling state (POSIX only)
  bool handle_piped_input_ = true;
  // File descriptor for /dev/tty, used for piped input handling.
  int tty_fd_ = -1;

  // The style of the cursor to restore on exit.
  int cursor_reset_shape_ = 1;

  // Selection API:
  CapturedMouse selection_pending_;
  struct SelectionData {
    int start_x = -1;
    int start_y = -1;
    int end_x = -2;
    int end_y = -2;
    bool empty = true;
    bool operator==(const SelectionData& other) const;
    bool operator!=(const SelectionData& other) const;
  };
  SelectionData selection_data_;
  SelectionData selection_data_previous_;
  std::unique_ptr<Selection> selection_;
  std::function<void()> selection_on_change_;

  // PIMPL private implementation idiom (Pimpl).
  struct Internal;
  std::unique_ptr<Internal> internal_;

  friend class Loop;

  Component component_;

 public:
  class Private {
   public:
    static void Signal(App& s, int signal) { s.Signal(signal); }
  };
  friend Private;
};

}  // namespace ftxui

#endif /* end of include guard: FTXUI_COMPONENT_APP_HPP */
