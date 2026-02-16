#include "plog.hpp"
#include "globalVariables.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <vector>
#include <memory>

// Map config.log_severity_level (int, 0-5) to spdlog level.
// Matches boost::log::trivial ordering: trace=0, debug=1, info=2, warning=3, error=4, fatal=5
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
  std::vector<spdlog::sink_ptr> sinks;

  // Console sink (stderr, same as boost's std::clog)
  auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
  sinks.push_back(console_sink);

  // File sink
  if (!config.file_name_log.empty()) {
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        config.file_name_log, /*truncate=*/true);
    sinks.push_back(file_sink);
  }

  auto logger = std::make_shared<spdlog::logger>(
      "prevabs", sinks.begin(), sinks.end());

  // Format: [severity] message  — matches previous boost::log format
  logger->set_pattern("[%l] %v");

  logger->set_level(toSpdlogLevel(config.log_severity_level));
  logger->flush_on(spdlog::level::trace);

  spdlog::register_logger(logger);
}
