#include "plog.hpp"
#include "globalVariables.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <sstream>
#include <vector>
#include <memory>

// Column widths for the dev-log fixed-column format.
static constexpr int COL_SEV = 11;  // "[warning]  " — longest label + padding
static constexpr int COL_MSG = 60;  // message column before source location

// Word-wrap `text` into chunks of at most `width` chars, splitting at spaces.
static std::vector<std::string> wordWrap(const std::string& text, int width) {
  std::vector<std::string> chunks;
  std::istringstream iss(text);
  std::string word, cur;
  while (iss >> word) {
    if (!cur.empty() && (int)(cur.size() + 1 + word.size()) > width) {
      chunks.push_back(cur);
      cur = word;
    } else {
      if (!cur.empty()) cur += ' ';
      cur += word;
    }
  }
  if (!cur.empty()) chunks.push_back(cur);
  if (chunks.empty()) chunks.push_back("");
  return chunks;
}

// Formats one dev-log entry as fixed-column text with message wrapping.
// Output layout:
//   [severity]  <message, up to COL_MSG chars>  file:func:line
//               <continuation lines if message wraps>
std::string formatLogLine(
    const std::string& severity,
    const std::string& message,
    const std::string& file,
    const std::string& func,
    int line) {

  // Severity column: "[warning]  " padded to COL_SEV chars
  std::string sev_col = "[" + severity + "]";
  if ((int)sev_col.size() < COL_SEV)
    sev_col += std::string(COL_SEV - sev_col.size(), ' ');

  std::string source = file + ":" + func + ":" + std::to_string(line);

  std::vector<std::string> chunks = wordWrap(message, COL_MSG);

  std::string out;
  for (int i = 0; i < (int)chunks.size(); ++i) {
    if (i > 0) out += '\n';
    if (i == 0) {
      // Pad message chunk to COL_MSG so source column is aligned
      std::string msg_col = chunks[i];
      if ((int)msg_col.size() < COL_MSG)
        msg_col += std::string(COL_MSG - msg_col.size(), ' ');
      out += sev_col + msg_col + "  " + source;
    } else {
      out += std::string(COL_SEV, ' ') + chunks[i];
    }
  }
  return out;
}

// Map AppConfig.log_level (int, 0-5) to spdlog level.
// Matches LOG_LEVEL_* constants in globalConstants.hpp: trace=0, debug=1, info=2, warning=3, error=4, fatal=5
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

  // Console sink (stderr) — color per level wraps the entire pre-formatted line
  auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
  console_sink->set_pattern("%^%v%$");
  sinks.push_back(console_sink);

  // File sink — plain text, no color escapes
  if (!config.file_name_log.empty()) {
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        config.file_name_log, /*truncate=*/true);
    file_sink->set_pattern("%v");
    sinks.push_back(file_sink);
  }

  auto logger = std::make_shared<spdlog::logger>(
      "prevabs", sinks.begin(), sinks.end());

  logger->set_level(toSpdlogLevel(config.app.log_level));
  // Flush immediately on warn/error so critical messages are never lost.
  // Below-warn levels (debug/trace) are buffered and flushed periodically
  // to prevent per-entry disk IO from dominating long traversals.
  logger->flush_on(spdlog::level::warn);

  spdlog::register_logger(logger);

  // In debug mode, flush at most once per second so debug/trace messages
  // still land on disk quickly without triggering a flush on every entry.
  if (config.app.log_level <= LOG_LEVEL_DEBUG) {
    spdlog::flush_every(std::chrono::seconds(1));
  }
}
