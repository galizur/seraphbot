#ifndef SBOT_CORE_LUA_COMMAND_ENGINE_HPP
#define SBOT_CORE_LUA_COMMAND_ENGINE_HPP

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <sol/forward.hpp>
#include <sol/optional_implementation.hpp>
#include <sol/sol.hpp>
#include <sol/state.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "seraphbot/core/chat_message.hpp"
#include "seraphbot/core/command_parser.hpp"

namespace sbot::core {

struct CommandMetadata {
  std::string name;
  std::string description;
  std::string usage;
  std::vector<std::string> aliases;

  int global_cooldown{0};
  int command_cooldown{0};
  int user_cooldown{0};

  bool mod_only{false};
  bool vip_only{false};
  bool subscriber_only{false};
  std::vector<std::string> allowed_users;

  bool enabled{true};
  int max_uses_per_stream{-1};
};

struct CooldownTracker {
  std::chrono::steady_clock::time_point last_global_command;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point>
      last_command_use;
  std::unordered_map<
      std::string,
      std::unordered_map<std::string, std::chrono::steady_clock::time_point>>
      user_command_cooldowns;
  std::unordered_map<std::string, int> command_use_count;

  auto isOnCooldown(const std::string &command, const std::string &user,
                    const CommandMetadata &meta) const -> bool;
  auto recordUsage(const std::string &command, const std::string &user) -> void;
  auto resetStreamCounters() -> void;
};

class LuaCommandContext {
public:
  explicit LuaCommandContext(const CommandContext &ctx) : m_ctx(ctx) {}

  [[nodiscard]] auto getUser() const -> std::string {
    return m_ctx.message.user;
  }
  [[nodiscard]] auto getCommand() const -> std::string { return m_ctx.command; }
  [[nodiscard]] auto getArgs() const -> std::vector<std::string> {
    return m_ctx.args;
  }
  [[nodiscard]] auto
  joinArgs(sol::optional<int> start_index = sol::nullopt) const -> std::string {
    int idx = start_index.value_or(0);
    return m_ctx.joinArgs(static_cast<std::size_t>(std::max(0, idx)));
  }

  auto reply(const std::string &message) const -> void { m_ctx.reply(message); }

  auto getMessageText() const -> std::string { return m_ctx.message.text; }

  [[nodiscard]] auto isBroadcaster() const -> bool {
    return std::ranges::contains(m_ctx.message.badges, "broadcaster");
  }

  [[nodiscard]] auto isModerator() const -> bool {
    return std::ranges::any_of(m_ctx.message.badges, [](auto const &badge) {
      return badge == "moderator" || badge == "broadcaster";
    });
  }

  [[nodiscard]] auto isVip() const -> bool {
    return std::ranges::any_of(m_ctx.message.badges, [](auto const &badge) {
      return badge == "vip" || badge == "moderator" || badge == "broadcaster";
    });
  }

  [[nodiscard]] auto isSubscriber() const -> bool {
    return std::ranges::any_of(m_ctx.message.badges, [](auto const &badge) {
      return badge == "subscriber" || badge == "vip" || badge == "moderator" ||
             badge == "broadcaster";
    });
  }

  [[nodiscard]] auto hasBadge(const std::string &badge_name) const -> bool {
    return std::ranges::any_of(
        m_ctx.message.badges,
        [&badge_name](auto const &badge) { return badge == badge_name; });
  }

private:
  const CommandContext &m_ctx;
};

class LuaCommandEngine {
public:
  LuaCommandEngine();
  ~LuaCommandEngine();

  auto initialize() -> void;
  auto loadLuaCommand(const std::filesystem::path &file_path) -> bool;
  auto loadCommandsFromDirectory(const std::filesystem::path &directory) -> int;
  auto registerWithParser(CommandParser &parser) -> void;
  auto reloadAllCommands() -> void;
  auto getLoadedCommands() const -> std::vector<std::string>;
  auto getCommandMetadata(const std::string &command_name) const
      -> const CommandMetadata *;
  auto resetStreamCounters() -> void {
    m_cooldown_tracker.resetStreamCounters();
  }

private:
  sol::state m_lua;
  std::unordered_map<std::string, std::filesystem::path> m_command_files;
  std::unordered_map<std::string, CommandMetadata> m_command_metadata;
  CooldownTracker m_cooldown_tracker;
  bool m_initialized{false};

  auto setupLuaEnvironment() -> void;

  static auto executeLuaCommand(const CommandContext &ctx,
                                sol::protected_function lua_func) -> void;

  auto parseCommandMetadata(const sol::table &command_table,
                            const std::string &default_name) -> CommandMetadata;

  auto hasPermission(const CommandContext &ctx,
                     const CommandMetadata &meta) const -> bool;
};

} // namespace sbot::core

#endif
