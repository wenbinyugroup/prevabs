#include "plog.hpp"
#include "globalVariables.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <mutex>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

const char *kConsoleLoggerName = "prevabs";
const char *kDevLoggerName = "prevabs_dev";

std::mutex &progressContextMutex() {
  static std::mutex mutex;
  return mutex;
}

std::vector<std::string> &progressContextStack() {
  static std::vector<std::string> stack;
  return stack;
}

std::string &lastExceptionProgressContext() {
  static std::string context;
  return context;
}

std::string joinProgressContext(const std::vector<std::string> &stack) {
  std::ostringstream oss;
  for (std::size_t i = 0; i < stack.size(); ++i) {
    if (i > 0) {
      oss << " > ";
    }
    oss << stack[i];
  }
  return oss.str();
}

} // namespace

// Returns "[info]    ", "[warning] ", "[error]   ", etc., padded to a fixed
// width so the message column aligns across all severity levels.
std::string paddedSeverityBracket(spdlog::level::level_enum level) {
  std::string label;
  switch (level) {
    case spdlog::level::trace:    label = "trace";   break;
    case spdlog::level::debug:    label = "debug";   break;
    case spdlog::level::info:     label = "info";    break;
    case spdlog::level::warn:     label = "warning"; break;
    case spdlog::level::err:      label = "error";   break;
    case spdlog::level::critical: label = "fatal";   break;
    default:                      label = "?";       break;
  }
  // "[warning]" is 9 chars; pad all brackets to 9 + 1 space = 10 chars total
  std::string bracket = "[" + label + "]";
  constexpr int WIDTH = 10;
  if ((int)bracket.size() < WIDTH)
    bracket += std::string(WIDTH - bracket.size(), ' ');
  return bracket;
}

// Map AppConfig.log_level (int, 0-5) to spdlog level.
static spdlog::level::level_enum toSpdlogLevel(int level) {
  switch (level) {
    case 0:  return spdlog::level::trace;
    case 1:  return spdlog::level::debug;
    case 2:  return spdlog::level::info;
    case 3:  return spdlog::level::warn;
    case 4:  return spdlog::level::err;
    default: return spdlog::level::critical;
  }
}

std::string formatConsoleMessage(
    spdlog::level::level_enum level, const std::string &message) {
  return paddedSeverityBracket(level) + " " + message;
}

std::string formatDevMessage(
    spdlog::level::level_enum level, const std::string &message) {
  const std::string padded = paddedSeverityBracket(level);
  const std::string context = formatProgressContext();
  if (context.empty()) {
    return padded + " " + message;
  }
  return padded + " [" + context + "] " + message;
}

void initLog() {
  spdlog::level::level_enum lvl = toSpdlogLevel(config.app.log_level);

  // Console sink (stderr, colored). User-facing output is now pui's job;
  // spdlog only surfaces warn+ on the console unless the developer raises
  // the log_level via --debug, in which case the requested level applies.
  auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
  console_sink->set_pattern("%^%v%$");
  spdlog::level::level_enum console_lvl =
      (lvl <= spdlog::level::debug) ? lvl : spdlog::level::warn;
  console_sink->set_level(console_lvl);

  auto console_logger = std::make_shared<spdlog::logger>(
      kConsoleLoggerName, console_sink);
  console_logger->set_level(spdlog::level::trace);
  console_logger->flush_on(spdlog::level::warn);
  spdlog::register_logger(console_logger);

  // The user log file (*.log) is owned by pui; spdlog only writes to
  // *.debug.log via the dev sink below.

  // Dev log: mirrors log_level, appends source location
  if (!config.file_name_log_dev.empty()) {
    auto dev_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        config.file_name_log_dev, /*truncate=*/true);
    dev_sink->set_pattern("%v  %s:%!:%#");
    dev_sink->set_level(lvl);
    auto dev_logger = std::make_shared<spdlog::logger>(
        kDevLoggerName, dev_sink);
    dev_logger->set_level(spdlog::level::trace);
    dev_logger->flush_on(spdlog::level::warn);
    spdlog::register_logger(dev_logger);
  }

  if (config.app.log_level <= LOG_LEVEL_DEBUG) {
    spdlog::flush_every(std::chrono::seconds(1));
  }
}

void pushProgressContext(const std::string &context) {
  std::lock_guard<std::mutex> lock(progressContextMutex());
  progressContextStack().push_back(context);
}

void popProgressContext() {
  std::lock_guard<std::mutex> lock(progressContextMutex());
  if (!progressContextStack().empty()) {
    progressContextStack().pop_back();
  }
}

void clearProgressContext() {
  std::lock_guard<std::mutex> lock(progressContextMutex());
  progressContextStack().clear();
  lastExceptionProgressContext().clear();
}

std::string formatProgressContext() {
  std::lock_guard<std::mutex> lock(progressContextMutex());
  if (!progressContextStack().empty()) {
    return joinProgressContext(progressContextStack());
  }
  return lastExceptionProgressContext();
}

bool hasPrevabsLogger() {
  return static_cast<bool>(spdlog::get(kConsoleLoggerName)) ||
         static_cast<bool>(spdlog::get(kDevLoggerName));
}

void flushPrevabsLoggers() {
  auto console_logger = spdlog::get(kConsoleLoggerName);
  if (console_logger) {
    console_logger->flush();
  }

  auto dev_logger = spdlog::get(kDevLoggerName);
  if (dev_logger) {
    dev_logger->flush();
  }
}

void logPrevabsMessage(spdlog::level::level_enum level,
                       const char* file, int line, const char* func,
                       const std::string &message) {
  const spdlog::source_loc loc{file, line, func};

  auto console_logger = spdlog::get(kConsoleLoggerName);
  if (console_logger) {
    console_logger->log(loc, level, formatConsoleMessage(level, message));
  }

  auto dev_logger = spdlog::get(kDevLoggerName);
  if (dev_logger) {
    dev_logger->log(loc, level, formatDevMessage(level, message));
  }
}

PLogContext::PLogContext(const std::string &context)
    : _active(true) {
  pushProgressContext(context);
}

PLogContext::~PLogContext() {
  if (!_active) {
    return;
  }

  const bool unwinding = std::uncaught_exception();
  if (unwinding) {
    std::lock_guard<std::mutex> lock(progressContextMutex());
    lastExceptionProgressContext() = joinProgressContext(progressContextStack());
    if (!progressContextStack().empty()) {
      progressContextStack().pop_back();
    }
    return;
  }

  popProgressContext();
}

void PLogContext::dismiss() {
  if (!_active) {
    return;
  }
  popProgressContext();
  _active = false;
}
