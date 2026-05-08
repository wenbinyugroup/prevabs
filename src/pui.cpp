#include "pui.hpp"

#include "cpp-terminal/color.hpp"
#include "cpp-terminal/style.hpp"
#include "cpp-terminal/tty.hpp"

#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>

namespace {

std::mutex                     g_mutex;
bool                           g_initialized = false;
bool                           g_color_enabled = false;
std::unique_ptr<std::ofstream> g_log_file;

struct LevelSpec {
  const char*       prefix;        // 3-char ASCII prefix, e.g. "==>"
  Term::Color::Name color;
  bool              bold_message;   // bold the whole line (used for title)
};

LevelSpec specFor(pui::Level level) {
  switch (level) {
    case pui::Level::info:    return {"==>", Term::Color::Name::Cyan,    false};
    case pui::Level::success: return {"OK ", Term::Color::Name::Green,   false};
    case pui::Level::warn:    return {"!! ", Term::Color::Name::Yellow,  false};
    case pui::Level::error:   return {"xx ", Term::Color::Name::Red,     false};
    case pui::Level::title:   return {"   ", Term::Color::Name::Default, true};
  }
  return {"   ", Term::Color::Name::Default, false};
}

// Build the colored line for stdout.
std::string formatColored(const LevelSpec &spec, const std::string &msg) {
  const std::string reset = Term::style(Term::Style::Reset);
  std::string out;
  if (spec.bold_message) {
    // title: bold the whole line, no prefix
    out += Term::style(Term::Style::Bold);
    out += msg;
    out += reset;
  } else {
    // colored bold prefix, plain message
    out += Term::color_fg(spec.color);
    out += Term::style(Term::Style::Bold);
    out += spec.prefix;
    out += reset;
    out += ' ';
    out += msg;
  }
  return out;
}

// Build the plain line for log file (no color codes).
std::string formatPlain(const LevelSpec &spec, const std::string &msg) {
  if (spec.bold_message) {
    return msg;
  }
  std::string out = spec.prefix;
  out += ' ';
  out += msg;
  return out;
}

void emit(pui::Level level, const std::string &msg) {
  const LevelSpec spec = specFor(level);

  std::lock_guard<std::mutex> lock(g_mutex);

  if (g_color_enabled) {
    std::cout << formatColored(spec, msg) << '\n';
  } else {
    std::cout << formatPlain(spec, msg) << '\n';
  }
  std::cout.flush();

  if (g_log_file && g_log_file->is_open()) {
    (*g_log_file) << formatPlain(spec, msg) << '\n';
    g_log_file->flush();
  }
}

}  // namespace


namespace pui {

void init(const std::string &log_file_path) {
  std::lock_guard<std::mutex> lock(g_mutex);

  // Auto-detect color support based on stdout being a TTY.
  g_color_enabled = Term::is_stdout_a_tty();

  if (!log_file_path.empty()) {
    g_log_file = std::make_unique<std::ofstream>(
        log_file_path, std::ios::out | std::ios::trunc);
    if (!g_log_file->is_open()) {
      g_log_file.reset();
    }
  }

  g_initialized = true;
}

void shutdown() {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (g_log_file) {
    g_log_file->flush();
    g_log_file->close();
    g_log_file.reset();
  }
  g_initialized = false;
}

void setColorEnabled(bool enabled) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_color_enabled = enabled;
}

bool isColorEnabled() {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_color_enabled;
}

void info   (const std::string &msg) { emit(Level::info,    msg); }
void success(const std::string &msg) { emit(Level::success, msg); }
void warn   (const std::string &msg) { emit(Level::warn,    msg); }
void error  (const std::string &msg) { emit(Level::error,   msg); }
void title  (const std::string &msg) { emit(Level::title,   msg); }

Stream::~Stream() {
  emit(_level, _oss.str());
}

}  // namespace pui
