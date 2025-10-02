#include "seraphbot/application.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "seraphbot/core/app_state.hpp"
#include "seraphbot/core/chat_message.hpp"
#include "seraphbot/core/command_parser.hpp"
#include "seraphbot/core/connection_manager.hpp"
#include "seraphbot/core/logging.hpp"
#include "seraphbot/core/lua_command_engine.hpp"
#include "seraphbot/core/twitch_service.hpp"
#include "seraphbot/discord/notifications.hpp"
#include "seraphbot/obs/obsservice.hpp"
#include "seraphbot/ui/imgui_backend_opengl.hpp"
#include "seraphbot/ui/imgui_manager.hpp"
#include "seraphbot/viewmodels/auth_viewmodel.hpp"
#include "seraphbot/viewmodels/chat_viewmodel.hpp"
#include "seraphbot/viewmodels/discord_viewmodel.hpp"

sbot::Application::Application() {
  LOG_CONTEXT("Application");
  LOG_INFO("Initializing");
}

sbot::Application::Application(std::size_t thread_count)
    : m_thread_count{thread_count} {
  LOG_CONTEXT("Application");
  LOG_INFO("Initializing with {} threads", m_thread_count);
}

sbot::Application::~Application() {
  LOG_CONTEXT("Application");
  LOG_INFO("Shutting down");
}

auto sbot::Application::initialize() -> bool {
  initializeServices();
  initializeDiscord();
  initializeViewmodels();
  initializeUi();
  setupCallbacks();
  return true;
}

auto sbot::Application::initializeServices() -> bool {
  m_conn      = std::make_shared<sbot::core::ConnectionManager>(m_thread_count);
  m_app_state = std::make_unique<sbot::core::AppState>();
  m_command_parser = std::make_unique<sbot::core::CommandParser>();
  m_command_engine = std::make_unique<sbot::core::LuaCommandEngine>();
  m_tw_service     = std::make_unique<sbot::core::TwitchService>(m_conn, m_cfg);
  return true;
}

auto sbot::Application::initializeDiscord() -> bool {
  m_disc_not = std::make_unique<sbot::discord::Notifications>(m_conn, m_cfg);
  m_obs      = std::make_unique<sbot::obs::OBSService>("testpassword");
  return true;
}

auto sbot::Application::initializeViewmodels() -> bool {
  m_auth_vm =
      std::make_unique<sbot::viewmodels::AuthVM>(*m_tw_service, *m_conn);
  m_discord_vm =
      std::make_unique<sbot::viewmodels::DiscordVM>(*m_conn, *m_disc_not);
  m_chat_vm = std::make_unique<sbot::viewmodels::ChatVM>(*m_tw_service);
  return true;
}

auto sbot::Application::initializeUi() -> bool {
  m_ui_backend = std::make_unique<sbot::ui::ImGuiBackendOpenGL>();
  m_ui_manager = std::make_unique<sbot::ui::ImGuiManager>(
      std::move(m_ui_backend), *m_app_state);
  return true;
}

auto sbot::Application::setupCallbacks() -> void {
  m_tw_service->setMessageCallback([this](const sbot::core::ChatMessage &msg) {
    m_app_state->pushChatMessage(msg);
    auto reply_fn = [this](const std::string &text) {
      m_tw_service->sendMessage(text);
    };
    if (m_command_parser->parseAndExecute(msg, reply_fn)) {
      LOG_DEBUG("Message handled as command");
    }
  });
  m_tw_service->setStatusCallback(
      [this](const std::string &status) { m_app_state->last_status = status; });
}

auto sbot::Application::run() -> int {
  m_command_engine->initialize();
  std::filesystem::path commands_dir = "commands";
  if (std::filesystem::exists(commands_dir)) {
    int loaded = m_command_engine->loadCommandsFromDirectory(commands_dir);
    LOG_INFO("Loaded {} Lua commands", loaded);
  } else {
    LOG_WARN("Commands directory '{}' does not exist", commands_dir.string());
    std::filesystem::create_directories(commands_dir);
    LOG_INFO("Created commands directory: {}", commands_dir.string());
  }
  m_command_engine->registerWithParser(*m_command_parser);
  // Optional: Keep some native C++ commands for core functionality
  m_command_parser->registerCommand(
      "reload", [this](const sbot::core::CommandContext &ctx) {
        ctx.reply("Reloading Lua commands...");
        m_command_engine->reloadAllCommands();
        m_command_engine->registerWithParser(*m_command_parser);
        ctx.reply("Lua commands reloaded!");
      });
  m_command_parser->registerCommand(
      "commands", [this](const core::CommandContext &ctx) {
        auto loaded_commands = m_command_engine->getLoadedCommands();
        if (loaded_commands.empty()) {
          ctx.reply("No Lua commands loaded");
          return;
        }
        std::string response = "Loaded commands: ";
        for (std::size_t i = 0; i < loaded_commands.size(); ++i) {
          if (i > 0) {
            response += ", ";
          }
          response += loaded_commands[i];
        }
        ctx.reply(response);
      });

  while (!m_ui_manager->shouldClose()) {
    m_ui_manager->poll();
    m_ui_manager->beginFrame();

    m_ui_manager->manageDocking();
    m_ui_manager->manageFloating();

    m_ui_manager->manageAuth(*m_auth_vm);
    m_ui_manager->manageDiscord(*m_discord_vm);
    m_ui_manager->manageChat(*m_chat_vm);

    m_ui_manager->endFrame();
    m_ui_manager->render();
    m_ui_manager->swapBuffers();
  }
  return 0;
}

auto sbot::Application::shutdown() -> void {
  // TODO: Add checks.
  m_ui_manager.reset();
  m_ui_backend.reset();

  m_tw_service.reset();
  m_app_state.reset();
  m_conn.reset();
}
