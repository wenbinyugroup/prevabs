#pragma once

#include "globalConstants.hpp"
#include "globalVariables.hpp"

#include <sstream>
#include <string>
#include <spdlog/spdlog.h>

// Returns "[info]    ", "[warning] ", etc., padded to a fixed width.
// Declared here, defined in plog.cpp.
std::string paddedSeverityBracket(spdlog::level::level_enum level);
std::string formatProgressContext();

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
    std::string padded = paddedSeverityBracket(level_);
    const std::string context = formatProgressContext();
    if (context.empty()) {
      logger->log(
          spdlog::source_loc{file_, line_, func_},
          level_, "{} {}", padded, oss_.str());
    } else {
      logger->log(
          spdlog::source_loc{file_, line_, func_},
          level_, "{} [{}] {}", padded, context, oss_.str());
    }
  }

  template <typename T>
  PLogStream& operator<<(const T& val) {
    oss_ << val;
    return *this;
  }
};

class PLogContext {
public:
  explicit PLogContext(const std::string &context);
  ~PLogContext();

  PLogContext(const PLogContext &) = delete;
  PLogContext &operator=(const PLogContext &) = delete;

  void dismiss();

private:
  bool _active;
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

inline bool shouldLogDebugAt(DebugLevel min_level) {
  return config.debug_level >= min_level;
}

#define PLOG_DEBUG_AT(level) \
  if (shouldLogDebugAt(DebugLevel::level)) PLOG(debug)

// Call once after config is populated (sets up file + console sinks).
void initLog();

// Lightweight runtime progress stack used by top-level fatal handlers.
// PLogContext is the preferred scoped API. The raw push/pop helpers remain
// available for legacy call sites and tests.
void pushProgressContext(const std::string &context);
void popProgressContext();
void clearProgressContext();
