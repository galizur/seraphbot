#include "seraphbot/core/command_parser.hpp"

#include "seraphbot/core/chat_message.hpp"
#include "seraphbot/core/logging.hpp"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <exception>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

sbot::core::CommandParser::CommandParser() {
  LOG_CONTEXT("CommandParser");
  LOG_INFO("Initializing");
}

sbot::core::CommandParser::~CommandParser() {
  LOG_CONTEXT("CommandParser");
  LOG_INFO("Shutting down");
}

auto sbot::core::CommandParser::registerCommand(const std::string &name,
                                                CommandHandler handler)
    -> void {
  if (name.empty()) {
    LOG_WARN("Attempted to register command with empty name");
    return;
  }

  if (!handler) {
    LOG_WARN("Attempted to register command '{}' with null handler", name);
    return;
  }

  std::string lower_name = name;
  std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                 ::tolower);

  m_commands[lower_name] = std::move(handler);
  LOG_INFO("Registered command: '{}'", name);
}

auto sbot::core::CommandParser::isCommand(const std::string &text) const
    -> bool {
  return !text.empty() && text.starts_with(m_prefix);
}

auto sbot::core::CommandParser::parseAndExecute(
    const ChatMessage &message,
    std::function<void(const std::string &)> reply_fn) -> bool {
  if (!isCommand(message.text)) {
    return false;
  }

  auto [command, args] = parseCommand(message.text);

  if (command.empty()) {
    LOG_DEBUG("Empty command extracted from: '{}'", message.text);
    return false;
  }

  std::string lower_command = command;
  std::transform(lower_command.begin(), lower_command.end(),
                 lower_command.begin(), ::tolower);

  auto it = m_commands.find(lower_command);
  if (it == m_commands.end()) {
    LOG_DEBUG("Unknown command: '{}' from user: {}", command, message.user);
    return false;
  }

  LOG_INFO("Executing command '{}' from user '{}' with {} args", command,
           message.user, args.size());

  try {
    CommandContext ctx(message, command, std::move(args), std::move(reply_fn));
    it->second(ctx);
    return true;
  } catch (const std::exception &err) {
    LOG_ERROR("Error executing command '{}': {}", command, err.what());
    return false;
  }
}

auto sbot::core::CommandParser::parseCommand(const std::string &text) const
    -> std::pair<std::string, std::vector<std::string>> {
  if (!isCommand(text)) {
    return {"", {}};
  }

  std::string without_prefix = text.substr(m_prefix.length());

  auto tokens = tokenize(without_prefix);

  if (tokens.empty()) {
    return {"", {}};
  }

  std::string command = tokens[0];
  std::vector<std::string> args;

  if (tokens.size() > 1) {
    args.assign(tokens.begin() + 1, tokens.end());
  }

  return {std::move(command), std::move(args)};
}

auto sbot::core::CommandParser::tokenize(const std::string &text) const
    -> std::vector<std::string> {
  std::vector<std::string> tokens;
  std::string current_token;
  bool in_quotes{false};
  bool escape_next{false};

  for (char c : text) {
    if (escape_next) {
      current_token += c;
      escape_next = false;
      continue;
    }

    if (c == '\\') {
      escape_next = true;
      continue;
    }

    if (c == '"') {
      in_quotes = !in_quotes;
      continue;
    }

    if (std::isspace(c) && !in_quotes) {
      if (!current_token.empty()) {
        tokens.push_back(std::move(current_token));
        current_token.clear();
      }
    } else {
      current_token += c;
    }
  }

  if (!current_token.empty()) {
    tokens.push_back(std::move(current_token));
  }

  return tokens;
}

auto sbot::core::CommandContext::joinArgs(std::size_t start_index) const
    -> std::string {
  std::string result;
  for (std::size_t i = start_index; i < args.size(); ++i) {
    if (i > start_index) {
      result += " ";
    }
    result += args[i];
  }
  return result;
}
