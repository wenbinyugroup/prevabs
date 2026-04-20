#pragma once

#include <sstream>
#include <string>
#include <spdlog/spdlog.h>

// Formats one dev-log entry as fixed-column text with message wrapping.
// Declared here, defined in plog.cpp.
std::string formatLogLine(
    const std::string& severity,
    const std::string& message,
    const std::string& file,
    const std::string& func,
    int line);

// Stream-wrapper that collects << output and logs on destruction.
// Usage: PLOG(info) << "msg " << var;
// Severity names match boost::log::trivial: trace, debug, info, warning, error, fatal
struct PLogStream {
  spdlog::level::level_enum level_;
  const char*               file_;
  int                       line_;
  const char*               func_;
  std::ostringstream        oss_;

  PLogStream(spdlog::level::level_enum lvl,
             const char* file, int line, const char* func)
    : level_(lvl), file_(file), line_(line), func_(func) {}

  ~PLogStream() {
    auto logger = spdlog::get("prevabs");
    if (!logger) return;

    // Map spdlog level to prevabs severity name
    std::string sev;
    switch (level_) {
      case spdlog::level::trace:    sev = "trace";   break;
      case spdlog::level::debug:    sev = "debug";   break;
      case spdlog::level::info:     sev = "info";    break;
      case spdlog::level::warn:     sev = "warning"; break;
      case spdlog::level::err:      sev = "error";   break;
      case spdlog::level::critical: sev = "fatal";   break;
      default:                      sev = "?";       break;
    }

    // Strip directory from __FILE__
    std::string fname(file_);
    auto pos = fname.find_last_of("/\\");
    if (pos != std::string::npos) fname = fname.substr(pos + 1);

    std::string formatted =
        formatLogLine(sev, oss_.str(), fname, func_, line_);
    logger->log(level_, "{}", formatted);
  }

  template <typename T>
  PLogStream& operator<<(const T& val) {
    oss_ << val;
    return *this;
  }
};

// Map boost::log::trivial severity names to spdlog levels.
// "warning" -> warn, "fatal" -> critical
#define _PLOG_LEVEL_trace    spdlog::level::trace
#define _PLOG_LEVEL_debug    spdlog::level::debug
#define _PLOG_LEVEL_info     spdlog::level::info
#define _PLOG_LEVEL_warning  spdlog::level::warn
#define _PLOG_LEVEL_error    spdlog::level::err
#define _PLOG_LEVEL_fatal    spdlog::level::critical

#define PLOG(severity) \
  PLogStream(_PLOG_LEVEL_##severity, \
             __FILE__, __LINE__, static_cast<const char*>(__func__))

// Call once after config is populated (sets up file + console sinks).
void initLog();
