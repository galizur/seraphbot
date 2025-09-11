#include "seraphbot/core/logging.hpp"
#include "spdlog/logger.h"

#include <cstddef>
#include <memory>
#include <mutex>
#include <spdlog/common.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <vector>

auto sbot::core::Logger::instance() -> Logger & {
  static Logger instance;
  return instance;
}

auto sbot::core::Logger::init(const Config &config) -> void {
  instance().setupSinks(config);
}

auto sbot::core::Logger::withContext(std::string_view context) -> Logger & {
  pushContext(context);
  return *this;
}

auto sbot::core::Logger::pushContext(std::string_view context) -> void {
  std::lock_guard lock{m_context_mutex};
  m_context_stack.emplace_back(context);
}

auto sbot::core::Logger::popContext() -> void {
  std::lock_guard lock{m_context_mutex};
  if (!m_context_stack.empty()) {
    m_context_stack.pop_back();
  }
}

auto sbot::core::Logger::log(sbot::core::LogLevel level,
                             const std::string &message) -> void {
  if (!m_logger || level < m_current_level) {
    return;
  }

  std::string full_message = message;

  // Add context if available
  {
    std::lock_guard lock{m_context_mutex};
    if (!m_context_stack.empty()) {
      std::string context_str = "[";
      for (std::size_t i = 0; i < m_context_stack.size(); ++i) {
        if (i > 0) {
          context_str += "::";
        }
        context_str += m_context_stack[i];
      }
      context_str += "] ";
      full_message = context_str + message;
    }
  }
  m_logger->log(toSpdlogLevel(level), full_message);
}

auto sbot::core::Logger::setupSinks(const Config &config) -> void {
  std::vector<spdlog::sink_ptr> sinks;

  // Console sink
  if (config.enable_console) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(toSpdlogLevel(config.console_level));
    sinks.push_back(console_sink);
  }

  // File sink
  if (config.enable_file) {
    std::string full_path = config.log_directory + "/" + config.log_filename;

    if (config.enable_rotation) {
      auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
          full_path, config.max_file_size, config.max_files);
      file_sink->set_level(toSpdlogLevel(config.file_level));
      sinks.push_back(file_sink);
    } else {
      auto file_sink =
          std::make_shared<spdlog::sinks::basic_file_sink_mt>(full_path, true);
      file_sink->set_level(toSpdlogLevel(config.file_level));
      sinks.push_back(file_sink);
    }
  }

  // Create logger
  m_logger =
      std::make_shared<spdlog::logger>("sbot", sinks.begin(), sinks.end());
  m_logger->set_level(spdlog::level::trace);
  m_logger->set_pattern(config.pattern);

  spdlog::set_default_logger(m_logger);
}

auto sbot::core::Logger::setLevel(LogLevel level) -> void {
  m_current_level = level;
}

auto sbot::core::Logger::getLevel() const -> LogLevel {
  return m_current_level;
}

auto sbot::core::Logger::flush() -> void {
  if (m_logger) {
    m_logger->flush();
  }
}

auto sbot::core::Logger::toSpdlogLevel(LogLevel level)
    -> spdlog::level::level_enum {
  switch (level) {
  case LogLevel::Trace:
    return spdlog::level::trace;
  case LogLevel::Debug:
    return spdlog::level::debug;
  case LogLevel::Info:
    return spdlog::level::info;
  case LogLevel::Warn:
    return spdlog::level::warn;
  case LogLevel::Error:
    return spdlog::level::err;
  case LogLevel::Critical:
    return spdlog::level::critical;
  case LogLevel::Off:
    return spdlog::level::off;
  }
  return spdlog::level::info;
}
