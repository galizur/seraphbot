#ifndef SBOT_APPLICATION_HPP
#define SBOT_APPLICATION_HPP

#include <cstddef>
#include <memory>
#include <string>

#include "seraphbot/core/command_parser.hpp"
#include "seraphbot/obs/obsservice.hpp"
#include "seraphbot/tw/config.hpp"

namespace sbot::core {
class ConnectionManager;
class AppState;
class CommandParser;
class TwitchService;
class LuaCommandEngine;
} // namespace sbot::core
namespace sbot::ui {
class ImGuiBackend;
class ImGuiManager;
} // namespace sbot::ui
namespace sbot::viewmodels {
struct AuthVM;
struct ChatVM;
struct DiscordVM;
} // namespace sbot::viewmodels
namespace sbot::discord {
class Notifications;
} // namespace sbot::discord

namespace sbot {

class Application {
public:
  Application();
  Application(std::size_t thread_count);
  ~Application();

  auto initialize() -> bool;
  auto run() -> int;
  auto shutdown() -> void;

private:
  std::size_t m_thread_count{3};
  // Configs
  tw::ClientConfig m_cfg;
  // Core
  std::shared_ptr<core::ConnectionManager> m_conn;
  std::unique_ptr<core::AppState> m_app_state;
  std::unique_ptr<core::CommandParser> m_command_parser;
  std::unique_ptr<core::LuaCommandEngine> m_command_engine;
  std::unique_ptr<core::TwitchService> m_tw_service;
  // Integrations
  std::unique_ptr<discord::Notifications> m_disc_not;
  std::unique_ptr<obs::OBSService> m_obs;
  // UI
  std::unique_ptr<ui::ImGuiBackend> m_ui_backend;
  std::unique_ptr<ui::ImGuiManager> m_ui_manager;
  // Viewmodels
  std::unique_ptr<viewmodels::AuthVM> m_auth_vm;
  std::unique_ptr<viewmodels::DiscordVM> m_discord_vm;
  std::unique_ptr<viewmodels::ChatVM> m_chat_vm;

  auto initializeServices() -> bool;
  auto initializeDiscord() -> bool;
  auto initializeViewmodels() -> bool;
  auto initializeUi() -> bool;
  auto setupCallbacks() -> void;

  // TODO: Implement temp hack properly
  auto handleSoundCommands(const std::string &text) -> void;
  auto playSound(const std::string& filepath) -> void;
  auto playRandomSound(const std::string& directory) -> void;
};

} // namespace sbot

#endif
