#pragma once

#include <sstream>
#include <spdlog/spdlog.h>

// Stream-wrapper that collects << output and logs on destruction.
// Usage: PLOG(info) << "msg " << var;
// Severity names match boost::log::trivial: trace, debug, info, warning, error, fatal
struct PLogStream {
  spdlog::level::level_enum level_;
  std::ostringstream oss_;

  explicit PLogStream(spdlog::level::level_enum lvl) : level_(lvl) {}

  ~PLogStream() {
    auto logger = spdlog::get("prevabs");
    if (logger) logger->log(level_, "{}", oss_.str());
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

#define PLOG(severity) PLogStream(_PLOG_LEVEL_##severity)

// Call once after config is populated (sets up file + console sinks).
void initLog();
