#include "plog.hpp"
#include "globalVariables.hpp"
#include <fstream>
#include <ostream>

#include <boost/log/core.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/core/core.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
// #include <boost/log/sources/logger.hpp>
// #include <boost/log/sources/record_ostream.hpp>
// #include <boost/log/utility/setup.hpp>

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;


// BOOST_LOG_ATTRIBUTE_KEYWORD(line_id, "LineID", unsigned int)
// BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", logging::trivial::severity_level)
// BOOST_LOG_ATTRIBUTE_KEYWORD(function_name, "Function", std::string)


BOOST_LOG_GLOBAL_LOGGER_INIT(plogger, plogger_mt) {
  plogger_mt plogger;

  // Add attributes
  plogger.add_attribute("LineID", attrs::counter<unsigned int>(1));
  plogger.add_attribute("TimeStamp", attrs::local_clock());

  // Create file sink
  using text_sink = sinks::synchronous_sink<sinks::text_ostream_backend>;
  auto file_sink = boost::make_shared<text_sink>();
  
  // Add log file stream
  file_sink->locked_backend()->add_stream(
      boost::make_shared<std::ofstream>(config.file_name_log));
  file_sink->locked_backend()->auto_flush(true);

  // Create console sink
  auto console_sink = boost::make_shared<text_sink>();
  console_sink->locked_backend()->add_stream(
      boost::shared_ptr<std::ostream>(&std::clog, boost::null_deleter()));

  // Define the custom formatter
  logging::formatter fmt = expr::stream
      << "[" << severity << "] "
      << expr::smessage;

  // Set formatters
  file_sink->set_formatter(fmt);
  console_sink->set_formatter(fmt);

  // Set different filters for file and console
  // File gets all messages (trace level)
  file_sink->set_filter(severity >= config.log_severity_level_file);
  
  // Console gets messages at or above the configured level
  console_sink->set_filter(severity >= config.log_severity_level);

  // Register the sinks
  logging::core::get()->add_sink(file_sink);
  logging::core::get()->add_sink(console_sink);

  return plogger;
}


// void initLog() {
//   // Construct the sink
//   typedef sinks::synchronous_sink< sinks::text_ostream_backend > text_sink;
//   boost::shared_ptr< text_sink > sink = boost::make_shared< text_sink >();

//   // logging::add_file_log(
//   //   logging::keywords::file_name = "test.log",
//   //   logging::keywords::format = "[%TimeStamp%]: %Message%"
//   // );

//   // Add a stream to write log to
//   sink->locked_backend()->add_stream(
//     boost::make_shared< std::ofstream >("test.log")
//   );

//   // Register the sink in the logging core
//   logging::core::get()->add_sink(sink);

//   // Attributes
//   logging::add_common_attributes();

//   // logging::core::get()->set_filter(
//   //   logging::trivial::severity >= boost::log::trivial::info
//   // );
// }
