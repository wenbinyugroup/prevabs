#pragma once

#include <sstream>
#include <spdlog/spdlog.h>

// Stream-wrapper that collects << output and logs on destruction.
// Usage: PLOG(info) << "msg " << var;
// Severity names match boost::log::trivial: trace, debug, info, warning, error, fatal
struct PLogStream {
  spdlog::level::level_enum level_;
  spdlog::source_loc        loc_;
  std::ostringstream        oss_;

  PLogStream(spdlog::level::level_enum lvl, spdlog::source_loc loc)
    : level_(lvl), loc_(loc) {}

  ~PLogStream() {
    auto logger = spdlog::get("prevabs");
    if (logger) logger->log(loc_, level_, "{}", oss_.str());
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
             spdlog::source_loc{__FILE__, __LINE__, static_cast<const char*>(__func__)})

// Call once after config is populated (sets up file + console sinks).
void initLog();
