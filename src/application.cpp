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
    handleSoundCommands(msg.text);
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

auto sbot::Application::handleSoundCommands(const std::string &text) -> void {
  if (text == "!adele") {
    playSound("/mnt/data/OBS/Sounds/adele.m4a");
  } else if (text == "!adios") {
    playSound("/mnt/data/OBS/Sounds/adios.m4a");
  } else if (text == "!ashley1") {
    playSound("/mnt/data/OBS/Sounds/ashley1.m4a");
  } else if (text == "!ashley2") {
    playSound("/mnt/data/OBS/Sounds/ashley2.m4a");
  } else if (text == "!beep1") {
    playSound("/mnt/data/OBS/Sounds/beep1.m4a");
  } else if (text == "!beep2") {
    playSound("/mnt/data/OBS/Sounds/beep2.m4a");
  } else if (text == "!bonk2") {
    playSound("/mnt/data/OBS/Sounds/bonk2.m4a");
  } else if (text == "!bonk") {
    playSound("/mnt/data/OBS/Sounds/bonk.m4a");
  } else if (text == "!breasts") {
    playSound("/mnt/data/OBS/Sounds/breasts.m4a");
  } else if (text == "!britain") {
    playSound("/mnt/data/OBS/Sounds/britain.m4a");
  } else if (text == "!bullshit") {
    playSound("/mnt/data/OBS/Sounds/bullshit.m4a");
  } else if (text == "!columbus") {
    playSound("/mnt/data/OBS/Sounds/columbus.m4a");
  } else if (text == "!damage") {
    playSound("/mnt/data/OBS/Sounds/damage.m4a");
  } else if (text == "!donotredeem") {
    playSound("/mnt/data/OBS/Sounds/donotredeem.m4a");
  } else if (text == "!flop") {
    playSound("/mnt/data/OBS/Sounds/flop.m4a");
  } else if (text == "!gneurshk") {
    playSound("/mnt/data/OBS/Sounds/gneurshk.m4a");
  } else if (text == "!haha") {
    playSound("/mnt/data/OBS/Sounds/haha.m4a");
  } else if (text == "!how") {
    playSound("/mnt/data/OBS/Sounds/how.m4a");
  } else if (text == "!huge") {
    playSound("/mnt/data/OBS/Sounds/huge.m4a");
  } else if (text == "!lazer") {
    playSound("/mnt/data/OBS/Sounds/lazer.m4a");
  } else if (text == "!mistake") {
    playSound("/mnt/data/OBS/Sounds/mistake.m4a");
  } else if (text == "!modem") {
    playSound("/mnt/data/OBS/Sounds/modem.m4a");
  } else if (text == "!movebitch") {
    playSound("/mnt/data/OBS/Sounds/movebitch.m4a");
  } else if (text == "!mrxclose") {
    playSound("/mnt/data/OBS/Sounds/mrxclose.m4a");
  } else if (text == "!mrxfar") {
    playSound("/mnt/data/OBS/Sounds/mrxfar.m4a");
  } else if (text == "!mrx") {
    playSound("/mnt/data/OBS/Sounds/mrx.m4a");
  } else if (text == "!muffin") {
    playSound("/mnt/data/OBS/Sounds/muffin.m4a");
  } else if (text == "!no") {
    playSound("/mnt/data/OBS/Sounds/no.m4a");
  } else if (text == "!oneofus") {
    playSound("/mnt/data/OBS/Sounds/oneofus.m4a");
  } else if (text == "!pacman") {
    playSound("/mnt/data/OBS/Sounds/pacman.m4a");
  } else if (text == "!ra") {
    playSound("/mnt/data/OBS/Sounds/ra.m4a");
  } else if (text == "!rebecca1") {
    playSound("/mnt/data/OBS/Sounds/rebecca1.m4a");
  } else if (text == "!religion1") {
    playSound("/mnt/data/OBS/Sounds/religion1.m4a");
  } else if (text == "!religion2") {
    playSound("/mnt/data/OBS/Sounds/religion2.m4a");
  } else if (text == "!taste") {
    playSound("/mnt/data/OBS/Sounds/taste.m4a");
  } else if (text == "!timotei") {
    playSound("/mnt/data/OBS/Sounds/timotei.m4a");
  } else if (text == "!trains") {
    playSound("/mnt/data/OBS/Sounds/trains.m4a");
  } else if (text == "!zombies") {
    playSound("/mnt/data/OBS/Sounds/zombies.m4a");
  } else if (text == "!randomsfx") {
    playRandomSound("/mnt/data/OBS/Sounds");
  } else if (text == "!sfx") {
    m_tw_service->sendMessage(
        "!adele !adios !ashley1 !ashley2 !beep1 !beep2 !bonk2 !bonk "
        "!breasts "
        "!britain !bullshit !columbus !damage !donotredeem !flop !gneurshk "
        "!haha !how !huge !lazer !mistake !modem !movebitch !mrxclose "
        "!mrxfar "
        "!mrx !muffin !no !oneofus");
    m_tw_service->sendMessage("!pacman !ra !rebecca1 !religion1 !religion2 "
                              "!taste !timotei !trains !zombies, !randomsfx");
  } else if (text == "!bot") {
    m_tw_service->sendMessage(
        "The previous bot I was using was crashing certain games. So I've "
        "switched to my own custom bot (which is highly experimental). The "
        "downside is a lot of functionality is still missing (e.g. video "
        "redemptions), so please bear with this inconvenience for now!");
  }

  if (text == "!placeholder") {
    boost::asio::co_spawn(
        *m_conn->getIoContext(),
        m_obs->playVideo("test.mp4", "TestScene", "TestSource"),
        boost::asio::detached);
  }
}

auto sbot::Application::playSound(const std::string &filepath) -> void {
  std::string cmd = "mpv --audio-client-name='SeraphBot' --no-video "
                    "--volume=70 --really-quiet " +
                    filepath + " &";
  std::system(cmd.c_str());
}

auto sbot::Application::playRandomSound(const std::string &directory) -> void {
  std::vector<std::string> sound_files;
  // Collect all audio files from directory
  for (const auto &entry : std::filesystem::directory_iterator(directory)) {
    if (entry.is_regular_file()) {
      auto ext = entry.path().extension().string();
      if (ext == ".m4a" || ext == ".wav" || ext == ".mp3" || ext == ".ogg") {
        sound_files.push_back(entry.path().string());
      }
    }
  }
  if (sound_files.empty()) {
    LOG_WARN("No sound files found in {}", directory);
    return;
  }

  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, sound_files.size() - 1);

  int random_index = dis(gen);
  playSound(sound_files[random_index]);

  LOG_INFO("Playing random sound: {}", sound_files[random_index]);
}
