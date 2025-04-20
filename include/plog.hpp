#pragma once

#include <boost/log/trivial.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/expressions.hpp>

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;

#define PLOG(severity) \
    BOOST_LOG_SEV(plogger::get(), logging::trivial::severity) \
    << "[" << __FUNCTION__ << ": " << __LINE__ << "] "
    // << "[" << __FILE__ << "." << __FUNCTION__ << ": " << __LINE__ << "] "

// #define LOGFILE "test.log"

typedef src::severity_logger_mt< logging::trivial::severity_level > plogger_mt;
// using plogger_mt = src::severity_logger_mt<logging::trivial::severity_level>;

BOOST_LOG_GLOBAL_LOGGER(plogger, plogger_mt)
