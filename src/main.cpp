#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <cstdlib>
#include <imgui.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "seraphbot/core/app_state.hpp"
#include "seraphbot/core/connection_manager.hpp"
#include "seraphbot/core/logging.hpp"
#include "seraphbot/core/twitch_service.hpp"
#include "seraphbot/discord/notifications.hpp"
#include "seraphbot/tw/auth.hpp"
#include "seraphbot/tw/config.hpp"
#include "seraphbot/ui/imgui_backend_opengl.hpp"
#include "seraphbot/ui/imgui_manager.hpp"
#include "seraphbot/viewmodels/discord_viewmodel.hpp"

auto main() -> int {
  sbot::core::Logger::init({});
  sbot::core::Logger::instance().setLevel(sbot::core::LogLevel::Trace);
  LOG_CONTEXT("Main");
  LOG_INFO("SeraphBot starting...");
  LOG_INFO("Selecting OpenGL backend");
  auto backend{std::make_unique<sbot::ui::ImGuiBackendOpenGL>()};
  sbot::core::AppState app_state;
  sbot::ui::ImGuiManager imgui_manager(std::move(backend), app_state);

  LOG_INFO("Initializing GUI manager");

  auto connection{std::make_shared<sbot::core::ConnectionManager>(2)};
  sbot::tw::ClientConfig tw_config;
  auto twitch_service{
      std::make_unique<sbot::core::TwitchService>(connection, tw_config)};
  sbot::discord::Notifications notif(connection, tw_config);
  sbot::viewmodels::DiscordVM discord_vm(notif);

  // Setup callbacks
  twitch_service->setMessageCallback(
      [&app_state](const sbot::core::ChatMessage &msg) {
        app_state.pushChatMessage(msg);
      });
  twitch_service->setStatusCallback([&app_state](const std::string &status) {
    app_state.last_status = status;
  });

  LOG_INFO("Entering main loop");
  while (glfwWindowShouldClose(imgui_manager.getWindow()) == 0) {
    glfwPollEvents();
    glClearColor(0.1F, 0.1F, 0.1F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    imgui_manager.beginFrame();

    imgui_manager.manageDocking();
    imgui_manager.manageFloating();
    imgui_manager.manageAuth(*twitch_service, *connection);
    imgui_manager.manageChat(*twitch_service);
    imgui_manager.manageDiscord(discord_vm);

    imgui_manager.endFrame();
    imgui_manager.render();
    glfwSwapBuffers(imgui_manager.getWindow());
  }
  LOG_INFO("Main loop ending, shutting down gracefully");
  twitch_service->disconnect();
  //  connection->stop()  ;
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  return 0;
}
