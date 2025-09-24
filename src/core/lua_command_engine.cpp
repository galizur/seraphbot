#include "seraphbot/core/lua_command_engine.hpp"
#include "seraphbot/core/command_parser.hpp"
#include "seraphbot/core/logging.hpp"
#include <chrono>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <sol/error.hpp>
#include <sol/forward.hpp>
#include <sol/types.hpp>
#include <string>
#include <utility>
#include <vector>

auto sbot::core::CooldownTracker::isOnCooldown(
    const std::string &command, const std::string &user,
    const CommandMetadata &meta) const -> bool {
  auto now = std::chrono::steady_clock::now();

  if (meta.global_cooldown > 0) {
    auto time_since_last = std::chrono::duration_cast<std::chrono::seconds>(
                               now - last_global_command)
                               .count();
    if (time_since_last < meta.global_cooldown) {
      return true;
    }
  }
  if (meta.command_cooldown > 0) {
    auto it = last_command_use.find(command);
    if (it != last_command_use.end()) {
      auto time_since_last =
          std::chrono::duration_cast<std::chrono::seconds>(now - it->second)
              .count();
      if (time_since_last < meta.command_cooldown) {
        return true;
      }
    }
  }

  if (meta.user_cooldown > 0) {
    auto cmd_it = user_command_cooldowns.find(command);
    if (cmd_it != user_command_cooldowns.end()) {
      auto user_it = cmd_it->second.find(user);
      if (user_it != cmd_it->second.end()) {
        auto time_since_last = std::chrono::duration_cast<std::chrono::seconds>(
                                   now - user_it->second)
                                   .count();
        if (time_since_last < meta.user_cooldown) {
          return true;
        }
      }
    }
  }
  return false;
}

auto sbot::core::CooldownTracker::recordUsage(const std::string &command,
                                              const std::string &user) -> void {
  auto now = std::chrono::steady_clock::now();

  last_global_command                   = now;
  last_command_use[command]             = now;
  user_command_cooldowns[command][user] = now;
  command_use_count[command]++;
}

auto sbot::core::CooldownTracker::resetStreamCounters() -> void {
  command_use_count.clear();
}

sbot::core::LuaCommandEngine::LuaCommandEngine() {
  LOG_CONTEXT("LuaCommmandEngine");
  LOG_INFO("Initializing");
}

sbot::core::LuaCommandEngine::~LuaCommandEngine() {
  LOG_CONTEXT("LuaCommmandEngine");
  LOG_INFO("Shutting Down");
}

auto sbot::core::LuaCommandEngine::initialize() -> void {
  if (m_initialized) {
    LOG_WARN("Lua command engine already initialized");
    return;
  }
  LOG_INFO("Initializing Lua command engine");

  m_lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math,
                       sol::lib::table, sol::lib::utf8);

  setupLuaEnvironment();
  m_initialized = true;

  LOG_INFO("Lua command engine initialized successfully");
}

auto sbot::core::LuaCommandEngine::setupLuaEnvironment() -> void {
  m_lua.new_usertype<LuaCommandContext>(
      "CommandContext", "getUser", &LuaCommandContext::getUser, "getCommand",
      &LuaCommandContext::getCommand, "getArgs", &LuaCommandContext::getArgs,
      "joinArgs", &LuaCommandContext::joinArgs, "reply",
      &LuaCommandContext::reply, "getMessageText", "isBroadcaster",
      &LuaCommandContext::isBroadcaster, &LuaCommandContext::getMessageText,
      "isModerator", &LuaCommandContext::isModerator, "isVip",
      &LuaCommandContext::isVip, "isSubscriber",
      &LuaCommandContext::isSubscriber, "hasBadge",
      &LuaCommandContext::hasBadge);

  m_lua["log"] = [](const std::string &message) {
    LOG_INFO("[Lua] {}", message);
  };

  LOG_DEBUG("Lua environment setup complete");
}

auto sbot::core::LuaCommandEngine::loadLuaCommand(
    const std::filesystem::path &file_path) -> bool {
  if (!m_initialized) {
    LOG_ERROR("Lua engine not initialized");
    return false;
  }
  if (!std::filesystem::exists(file_path)) {
    LOG_ERROR("Lua command file does not exist: {}", file_path.string());
    return false;
  }

  LOG_INFO("Loading Lua command from: {}", file_path.string());

  try {
    auto result = m_lua.script_file(file_path.string());

    if (!result.valid()) {
      sol::error err = result;
      LOG_ERROR("Error loading Lua command {}: {}", file_path.string(),
                err.what());
      return false;
    }

    if (result.get_type() != sol::type::table) {
      LOG_ERROR("Lua command file {} must return a table", file_path.string());
      return false;
    }

    sol::table command_table = result;

    if (!command_table["name"].valid()) {
      LOG_ERROR("Lua command {} missing required 'name' field",
                file_path.string());
      return false;
    }

    if (!command_table["execute"].valid()) {
      LOG_ERROR("Lua command {} missing required 'execute' function",
                file_path.string());
      return false;
    }

    std::string command_name = command_table["name"];

    if (command_name.empty()) {
      LOG_ERROR("Lua command {} has empty name", file_path.string());
      return false;
    }

    CommandMetadata metadata =
        parseCommandMetadata(command_table, command_name);

    m_command_files[command_name]    = file_path;
    m_command_metadata[command_name] = metadata;

    for (const auto &alias : metadata.aliases) {
      m_command_metadata[alias] = metadata;
      m_command_files[alias]    = file_path;
    }

    LOG_INFO("Successfully loaded Lua command: '{}'", command_name);
    return true;
  } catch (const sol::error &err) {
    LOG_ERROR("Sol2 error loading {}: {}", file_path.string(), err.what());
    return false;
  } catch (const std::exception &err) {
    LOG_ERROR("Exception loading {}: {}", file_path.string(), err.what());
    return false;
  }
}

auto sbot::core::LuaCommandEngine::loadCommandsFromDirectory(
    const std::filesystem::path &directory) -> int {
  if (!std::filesystem::exists(directory) ||
      !std::filesystem::is_directory(directory)) {
    LOG_ERROR("Directory does not exist or is not a directory: {}",
              directory.string());
    return 0;
  }

  LOG_INFO("Loading Lua commands from directory: {}", directory.string());

  int loaded_count = 0;

  try {
    for (const auto &entry : std::filesystem::directory_iterator(directory)) {
      if (entry.is_regular_file() && entry.path().extension() == ".lua") {
        if (loadLuaCommand(entry.path())) {
          loaded_count++;
        }
      }
    }
  } catch (const std::exception &err) {
    LOG_ERROR("Error scanning directory {}: {}", directory.string(),
              err.what());
  }

  LOG_INFO("Loaded {} Lua commands from {}", loaded_count, directory.string());
  return loaded_count;
}

auto sbot::core::LuaCommandEngine::registerWithParser(
    sbot::core::CommandParser &parser) -> void {
  if (!m_initialized) {
    LOG_ERROR("Cannot register commands - Lua engine not initialized");
    return;
  }

  LOG_INFO("Registering {} Lua commands with command parser",
           m_command_files.size());

  for (const auto &[command_name, file_path] : m_command_files) {
    try {
      auto result = m_lua.script_file(file_path.string());
      if (!result.valid()) {
        LOG_ERROR("Failed to re-load command {} for registration",
                  command_name);
        continue;
      }
      sol::table command_table             = result;
      sol::protected_function execute_func = command_table["execute"];

      if (!execute_func.valid()) {
        LOG_ERROR("Command {} missing execute function during registration",
                  command_name);
        continue;
      }

      CommandHandler handler = [this, execute_func,
                                command_name](const CommandContext &ctx) {
        auto meta_it = m_command_metadata.find(command_name);
        if (meta_it == m_command_metadata.end()) {
          LOG_ERROR("No metadata found for command: {}", command_name);
          ctx.reply("Command configuration error");
          return;
        }
        const CommandMetadata &meta = meta_it->second;
        if (!meta.enabled) {
          return;
        }
        if (!hasPermission(ctx, meta)) {
          ctx.reply("You don't have permission to use this command.");
          return;
        }
        if (m_cooldown_tracker.isOnCooldown(command_name, ctx.message.user,
                                            meta)) {
          ctx.reply("Command is on cooldown.");
          return;
        }
        if (meta.max_uses_per_stream > 0) {
          auto use_count_it =
              m_cooldown_tracker.command_use_count.find(command_name);
          int current_uses =
              (use_count_it != m_cooldown_tracker.command_use_count.end())
                  ? use_count_it->second
                  : 0;
          if (current_uses >= meta.max_uses_per_stream) {
            ctx.reply("Command has reached its usage limit for this stream.");
            return;
          }
        }
        executeLuaCommand(ctx, execute_func);
        m_cooldown_tracker.recordUsage(command_name, ctx.message.user);
      };

      parser.registerCommand(command_name, std::move(handler));
      LOG_DEBUG("Registered Lua command: {}", command_name);
    } catch (const std::exception &err) {
      LOG_ERROR("Error registering Lua command {}: {}", command_name,
                err.what());
    }
  }
  LOG_INFO("Finished registering Lua commands");
}

auto sbot::core::LuaCommandEngine::reloadAllCommands() -> void {
  LOG_INFO("Reloading all Lua commands");

  auto files_to_reload = m_command_files;
  m_command_files.clear();

  int reloaded = 0;
  for (const auto &[command_name, file_path] : files_to_reload) {
    if (loadLuaCommand(file_path)) {
      reloaded++;
    }
  }
  LOG_INFO("Reloaded {}/{} Lua commands", reloaded, files_to_reload.size());
}

auto sbot::core::LuaCommandEngine::getLoadedCommands() const
    -> std::vector<std::string> {
  std::vector<std::string> commands;
  commands.reserve(m_command_files.size());

  for (const auto &[command_name, _] : m_command_files) {
    commands.push_back(command_name);
  }
  return commands;
}

auto sbot::core::LuaCommandEngine::executeLuaCommand(
    const CommandContext &ctx, sol::protected_function lua_func) -> void {
  try {
    LuaCommandContext lua_ctx(ctx);

    auto result = lua_func(lua_ctx, ctx.args);

    if (!result.valid()) {
      sol::error err = result;
      LOG_ERROR("Error executing Lua command '{}': {}", ctx.command,
                err.what());
      ctx.reply("Command error occured - check logs");
    }
  } catch (const std::exception &err) {
    LOG_ERROR("Exception executing Lua command '{}': {}", ctx.command,
              err.what());
    ctx.reply("Command execution failed");
  }
}

auto sbot::core::LuaCommandEngine::parseCommandMetadata(
    const sol::table &command_table, const std::string &default_name)
    -> CommandMetadata {
  CommandMetadata meta;

  meta.name = command_table.get_or<std::string>("name", default_name);

  meta.description = command_table.get_or<std::string>("description", "");
  meta.usage       = command_table.get_or("usage", "!" + meta.name);

  if (command_table["aliases"].valid()) {
    sol::table aliases_table = command_table["aliases"];
    for (std::size_t i = 1; i <= aliases_table.size(); ++i) {
      if (aliases_table[i].valid()) {
        meta.aliases.push_back(aliases_table[i]);
      }
    }
  }
  meta.global_cooldown  = command_table.get_or("global_cooldown", 0);
  meta.command_cooldown = command_table.get_or("command_cooldown", 0);
  meta.user_cooldown    = command_table.get_or("user_cooldown", 0);

  meta.mod_only        = command_table.get_or("mod_only", false);
  meta.vip_only        = command_table.get_or("vip_only", false);
  meta.subscriber_only = command_table.get_or("subscriber_only", false);

  if (command_table["allowed_users"].valid()) {
    sol::table users_table = command_table["allowed_users"];
    for (std::size_t i = 1; i <= users_table.size(); ++i) {
      if (users_table[i].valid()) {
        meta.allowed_users.push_back(users_table[i]);
      }
    }
  }
  meta.enabled             = command_table.get_or("enabled", true);
  meta.max_uses_per_stream = command_table.get_or("max_uses_per_stream", -1);

  return meta;
}

auto sbot::core::LuaCommandEngine::hasPermission(
    const CommandContext &ctx, const CommandMetadata &meta) const -> bool {
  if (!meta.allowed_users.empty()) {
    auto it = std::find(meta.allowed_users.begin(), meta.allowed_users.end(),
                        ctx.message.user);
    if (it != meta.allowed_users.end()) {
      return true;
    }
  }
  LuaCommandContext lua_ctx(ctx);
  if (meta.mod_only && !lua_ctx.isModerator()) {
    return false;
  }
  if (meta.vip_only && !lua_ctx.isVip()) {
    return false;
  }
  if (meta.subscriber_only && !lua_ctx.isSubscriber()) {
    return false;
  }
  return true;
}

auto sbot::core::LuaCommandEngine::getCommandMetadata(
    const std::string &command_name) const -> const CommandMetadata * {
  auto it = m_command_metadata.find(command_name);
  return (it != m_command_metadata.end()) ? &it->second : nullptr;
}
