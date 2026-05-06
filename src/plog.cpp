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

void initLog() {
  spdlog::level::level_enum lvl = toSpdlogLevel(config.app.log_level);

  std::vector<spdlog::sink_ptr> sinks;

  // Console sink (stderr, colored) — gated by log_level
  auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
  console_sink->set_pattern("%^%v%$");
  console_sink->set_level(lvl);
  sinks.push_back(console_sink);

  // User log: always at info or above, no source location
  if (!config.file_name_log.empty()) {
    auto user_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        config.file_name_log, /*truncate=*/true);
    user_sink->set_pattern("%v");
    user_sink->set_level(spdlog::level::info);
    sinks.push_back(user_sink);
  }

  // Dev log: mirrors log_level, appends source location
  if (!config.file_name_log_dev.empty()) {
    auto dev_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        config.file_name_log_dev, /*truncate=*/true);
    dev_sink->set_pattern("%v  %s:%!:%#");
    dev_sink->set_level(lvl);
    sinks.push_back(dev_sink);
  }

  auto logger = std::make_shared<spdlog::logger>(
      "prevabs", sinks.begin(), sinks.end());

  // Logger passes everything; sinks filter by their own level.
  logger->set_level(spdlog::level::trace);
  logger->flush_on(spdlog::level::warn);

  spdlog::register_logger(logger);

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
