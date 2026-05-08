#pragma once

// pui — PreVABS user-facing UI channel.
//
// Use pui::* for messages the end user should see at the terminal: stage
// progress, success markers, user-level warnings/errors. Messages also
// get mirrored to the user log file (*.log) without color codes.
//
// Use PLOG(...) for developer/debug logs (source location, *.debug.log).
// The two channels are independent. As a rule of thumb:
//   - "user wants to know what the program is doing" -> pui::info
//   - "developer wants to know internal state"       -> PLOG(debug)

#include <sstream>
#include <string>

namespace pui {

enum class Level {
  info,     // step progress, highlighted with cyan "==>"
  success,  // key success node, green "OK"
  warn,     // user-facing warning, yellow "!!"
  error,    // user-facing error, red "xx"
  title,    // banner / heading, bold
};

// Initialize stdout TTY detection and (optionally) attach a user log file.
// Pass an empty path to disable file mirroring. Safe to call once at startup.
void init(const std::string &log_file_path = "");

// Close the log file (flushes pending output). Optional; called automatically
// at program exit via static destruction.
void shutdown();

// Override automatic TTY-based color detection. Useful for --no-color flags
// and for tests that want deterministic output.
void setColorEnabled(bool enabled);
bool isColorEnabled();

// Direct calls. Each call emits a single line to stdout and to the log file
// (if attached), with a fixed prefix matching its level.
void info   (const std::string &msg);
void success(const std::string &msg);
void warn   (const std::string &msg);
void error  (const std::string &msg);
void title  (const std::string &msg);

// Stream-style wrapper. Collects << output and emits on destruction.
//   PUI_INFO << "loading " << n << " points";
class Stream {
 public:
  explicit Stream(Level level) : _level(level) {}
  ~Stream();

  Stream(const Stream &) = delete;
  Stream &operator=(const Stream &) = delete;

  template <typename T>
  Stream &operator<<(const T &value) {
    _oss << value;
    return *this;
  }

 private:
  Level              _level;
  std::ostringstream _oss;
};

}  // namespace pui

#define PUI_INFO    pui::Stream(pui::Level::info)
#define PUI_SUCCESS pui::Stream(pui::Level::success)
#define PUI_WARN    pui::Stream(pui::Level::warn)
#define PUI_ERROR   pui::Stream(pui::Level::error)
#define PUI_TITLE   pui::Stream(pui::Level::title)
