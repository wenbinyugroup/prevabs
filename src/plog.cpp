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


BOOST_LOG_ATTRIBUTE_KEYWORD(line_id, "LineID", unsigned int)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", logging::trivial::severity_level)


BOOST_LOG_GLOBAL_LOGGER_INIT(plogger, plogger_mt) {
  plogger_mt plogger;

  // Add attributes
  // logging::add_common_attributes();
  plogger.add_attribute("LineID", attrs::counter<unsigned int>(1));
  plogger.add_attribute("TimeStamp", attrs::local_clock());

  // Add a text sink
  typedef sinks::synchronous_sink<sinks::text_ostream_backend> text_sink;
  boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();

  // Add a log file stream to the sink
  sink->locked_backend()->add_stream(boost::make_shared<std::ofstream>(config.file_name_log));
  sink->locked_backend()->auto_flush(true);

  // Add a console output stream to the sink
  sink->locked_backend()->add_stream(boost::shared_ptr<std::ostream>(&std::clog, boost::null_deleter()));

  // Specify the format
  logging::formatter fmt = expr::stream
    // << std::setw(7) << std::setfill('0') << line_id << std::setfill(' ') << " | "
    // << expr::format_date_time(timestamp, "%Y-%m-%d %H:%M:%S") << " "
    << "[" << logging::trivial::severity << "]"
    << " " << expr::message;
  sink->set_formatter(fmt);

  // Set filter
  sink->set_filter(severity >= config.log_severity_level);

  // Register the sink
  logging::core::get()->add_sink(sink);

  // logging::add_console_log(
  //   std::cout,
  //   keywords::format = (
  //     expr::stream << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S")
  //     << " [" << expr::attr < logging::trivial::severity_level >("Severity") << "]: "
  //     << expr::message
  //   )
  // );

  // logging::add_file_log(
  //   keywords::file_name = config.file_name_log,
  //   keywords::format = (
  //     expr::stream << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S")
  //     << " [" << expr::attr < logging::trivial::severity_level >("Severity") << "]: "
  //     << expr::message
  //   )
  // );


  // logging::core::get()->set_filter(
  //   logging::trivial::severity >= config.log_severity_level
  //   // logging::trivial::severity >= logging::trivial::debug
  //   // logging::trivial::severity >= logging::trivial::info
  // );

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
