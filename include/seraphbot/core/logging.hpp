#ifndef SBOT_CORE_LOGGING_HPP
#define SBOT_CORE_LOGGING_HPP

#include <cstddef>
#include <format>
#include <memory>
#include <mutex>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <vector>

namespace sbot::core {

enum class LogLevel {
  Trace    = 0,
  Debug    = 1,
  Info     = 2,
  Warn     = 3,
  Error    = 4,
  Critical = 5,
  Off      = 6
};

class Logger {
public:
  struct Config {
    LogLevel console_level{LogLevel::Info};
    LogLevel file_level{LogLevel::Debug};
    std::string log_directory{"logs"};
    std::string log_filename{"seraphbot.log"};
    bool enable_console{true};
    bool enable_file{true};
    bool enable_rotation{true};
    std::size_t max_file_size{10 * 1024 * 1024};
    std::size_t max_files{5};
    std::string pattern{"[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [%t] %v"};
  };

  static auto instance() -> Logger &;
  static auto init(const Config &config) -> void;
  // Template methods for efficient logging
  template <typename... Args>
  auto trace(std::format_string<Args...> fmt, Args &&...args) -> void {
    log(LogLevel::Trace, std::format(fmt, std::forward<Args>(args)...));
  }
  template <typename... Args>
  auto debug(std::format_string<Args...> fmt, Args &&...args) -> void {
    log(LogLevel::Debug, std::format(fmt, std::forward<Args>(args)...));
  }
  template <typename... Args>
  auto info(std::format_string<Args...> fmt, Args &&...args) -> void {
    log(LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
  }
  template <typename... Args>
  auto warn(std::format_string<Args...> fmt, Args &&...args) -> void {
    log(LogLevel::Warn, std::format(fmt, std::forward<Args>(args)...));
  }
  template <typename... Args>
  auto error(std::format_string<Args...> fmt, Args &&...args) -> void {
    log(LogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
  }
  template <typename... Args>
  auto critical(std::format_string<Args...> fmt, Args &&...args) -> void {
    log(LogLevel::Critical, std::format(fmt, std::forward<Args>(args)...));
  }

  // Context-aware logging
  auto withContext(std::string_view context) -> Logger &;
  auto pushContext(std::string_view context) -> void;
  auto popContext() -> void;

  // Level control
  auto setLevel(LogLevel level) -> void;
  auto getLevel() const -> LogLevel;

  // Flush logs
  auto flush() -> void;

private:
  Logger() = default;

  auto log(LogLevel level, const std::string &message) -> void;
  auto setupSinks(const Config &config) -> void;
  static auto toSpdlogLevel(LogLevel level) -> spdlog::level::level_enum;

  std::shared_ptr<spdlog::logger> m_logger;
  std::vector<std::string> m_context_stack;
  std::mutex m_context_mutex;
  LogLevel m_current_level = LogLevel::Info;
};

// Scoped context helper
class LogContext {
public:
  explicit LogContext(std::string_view context) {
    Logger::instance().pushContext(context);
  }

  ~LogContext() { Logger::instance().popContext(); }

  LogContext(const LogContext &)                     = delete;
  auto operator=(const LogContext &) -> LogContext & = delete;
  LogContext(LogContext &&)                          = delete;
  auto operator=(LogContext &&) -> LogContext &      = delete;
};

// Convenience macros
#define LOG_TRACE(...) sbot::core::Logger::instance().trace(__VA_ARGS__)
#define LOG_DEBUG(...) sbot::core::Logger::instance().debug(__VA_ARGS__)
#define LOG_INFO(...) sbot::core::Logger::instance().info(__VA_ARGS__)
#define LOG_WARN(...) sbot::core::Logger::instance().warn(__VA_ARGS__)
#define LOG_ERROR(...) sbot::core::Logger::instance().error(__VA_ARGS__)
#define LOG_CRITICAL(...) sbot::core::Logger::instance().critical(__VA_ARGS__)

#define LOG_CONTEXT(name) sbot::core::LogContext c_log_ctx(name)

} // namespace sbot::core

#endif
