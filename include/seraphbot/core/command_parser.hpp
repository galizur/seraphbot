#ifndef SBOT_CORE_COMMAND_PARSER_HPP
#define SBOT_CORE_COMMAND_PARSER_HPP

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sbot::core {

struct ChatMessage;

struct CommandContext {
  const ChatMessage &message;
  std::string command;
  std::vector<std::string> args;
  std::function<void(const std::string &)> reply;

  CommandContext(const ChatMessage &msg, std::string cmd,
                 std::vector<std::string> arguments,
                 std::function<void(const std::string &)> reply_fn)
      : message{msg}, command{std::move(cmd)}, args{std::move(arguments)},
        reply{std::move(reply_fn)} {}

  [[nodiscard]] auto joinArgs(std::size_t start_index = 0) const -> std::string;
};

using CommandHandler = std::function<void(const CommandContext &)>;

class CommandParser {
public:
  CommandParser();
  ~CommandParser();

  auto registerCommand(const std::string &name, CommandHandler handler) -> void;
  auto setPrefix(const std::string &prefix) -> void;
  auto parseAndExecute(const ChatMessage &message,
                       std::function<void(const std::string &)> reply_fn)
      -> bool;
  [[nodiscard]] auto isCommand(const std::string &text) const -> bool;

private:
  std::string m_prefix{"!"};
  std::unordered_map<std::string, CommandHandler> m_commands;

  // Parse command and arguments from text
  [[nodiscard]] auto parseCommand(const std::string &text) const
      -> std::pair<std::string, std::vector<std::string>>;
  // Split string by spaces, respecting quotes
  [[nodiscard]] auto tokenize(const std::string &text) const
      -> std::vector<std::string>;
};

} // namespace sbot::core

#endif
