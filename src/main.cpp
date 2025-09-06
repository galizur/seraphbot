#include "seraphbot/core/app_state.hpp"
#include "seraphbot/core/logging.hpp"
#include "seraphbot/tw/auth.hpp"
#include "seraphbot/tw/chat.hpp"
#include "seraphbot/ui/imgui_backend_opengl.hpp"
#include "seraphbot/ui/imgui_manager.hpp"

#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <imgui.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <utility>

auto hexToImVec4 = [](const std::string &hex) -> ImVec4 {
  unsigned int red   = 0;
  unsigned int green = 0;
  unsigned int blue  = 0;
  unsigned int alpha = 255;
  if (hex.size() == 7 || hex.size() == 9) {
    sscanf(hex.c_str(), "#%02x%02x%02x", &red, &green, &blue);
    if (hex.size() == 9) {
      unsigned int alpha_temp{};
      sscanf(hex.c_str() + 7, "%02x", &alpha_temp);
      alpha = alpha_temp;
    }
  }
  return {red / 255.0F, green / 255.0F, blue / 255.0F, alpha / 255.0F};
};

auto initWindow(int width = 1820, int height = 720) -> GLFWwindow * {
  if (glfwInit() == 0) {
    throw std::runtime_error("Failed to init GLFW");
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window =
      glfwCreateWindow(width, height, "SeraphBot", nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    throw std::runtime_error("Failed to create window");
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
  return window;
}

// For later
auto initWindowVulkan(int width = 1280, int height = 720) -> GLFWwindow * {
  if (glfwInit() == 0) {
    throw std::runtime_error("Failed to init GLFW");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Important for Vulkan

  GLFWwindow *window =
      glfwCreateWindow(width, height, "SeraphBot", nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    throw std::runtime_error("Failed to create window");
  }

  return window;
}

auto main() -> int {
  sbot::core::Logger::init({});
  LOG_INFO("SeraphBot starting...");

  LOG_INFO("Initializing window");
  GLFWwindow *window = initWindow();
  LOG_INFO("Initializing app state");
  sbot::core::AppState app_state;

  LOG_INFO("Selecting OpenGL backend");
  auto backend = std::make_unique<sbot::ui::ImGuiBackendOpenGL>();
  LOG_INFO("Initializing GUI manager");
  sbot::ui::ImGuiManager imgui_manager(std::move(backend), app_state, window);

  auto connection{std::make_shared<sbot::core::ConnectionManager>(2)};

  sbot::tw::Auth twitch_auth(connection, "seraphbot-oauth-server.onrender.com");
  sbot::tw::ClientConfig twitch_config;
  std::unique_ptr<sbot::tw::Chat> twitch_client;
  // Main loop
  LOG_INFO("Entering main loop");
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    glClearColor(0.1F, 0.1F, 0.1F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    imgui_manager.beginFrame();

    ImGui::Begin("Auth");
    if (!app_state.is_logged_in) {
      if (ImGui::Button("Login to Twitch")) {
        app_state.wants_login = true;
      }
    } else {
      ImGui::Text("Authenticated as %s", app_state.twitchAuthState.c_str());
      if (ImGui::Button("Logout")) {
        app_state.wants_logout = true;
      }
    }
    ImGui::End();

    if (app_state.eventsub_active) {
      auto pending_count = app_state.pendingMessageCount();
      if (pending_count > 0) {
        LOG_INFO("[Main] Processing {} pending messages", pending_count);
      }
      app_state.processPendingMessages();
    }
    ImGui::Begin("Chat");
    if (app_state.is_logged_in && app_state.eventsub_active) {
      ImGui::BeginChild("ScrollingRegion",
                        ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true,
                        ImGuiWindowFlags_HorizontalScrollbar);
      for (auto &msg : app_state.chat_log) {
        ImGui::TextColored(hexToImVec4(msg.color), "%s:", msg.user.c_str());
        ImGui::SameLine();
        ImGui::TextUnformatted(msg.text.c_str());
      }
      if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0F);
      }
      ImGui::EndChild();
      ImGui::BeginChild("MessageSend");
      char buffer[512];
      ImGui::InputText("Send", buffer, 512);
      ImGui::EndChild();
    } else if (app_state.is_logged_in && !app_state.eventsub_active) {
      if (ImGui::Button("Connect to Chat")) {
        app_state.wants_eventsub = true;
      }
    } else {
      ImGui::Text("Not logged in");
    }
    ImGui::End();

    imgui_manager.endFrame();
    imgui_manager.render();

    if (app_state.wants_login) {
      boost::asio::co_spawn(
          *connection->getIoContext(),
          [&]() -> boost::asio::awaitable<void> {
            try {
              co_await twitch_auth.loginAsync();
              std::string token = twitch_auth.accessToken();
              auto user_info    = co_await twitch_auth.fetchUserInfoAsync();

              app_state.twitchAuthState = user_info.display_name;
              app_state.is_logged_in    = true;

              twitch_config.access_token   = twitch_auth.accessToken();
              twitch_config.broadcaster_id = user_info.id;
              twitch_config.client_id      = twitch_auth.clientId();
            } catch (const std::exception &e) {
              LOG_ERROR("Twitch login failed: {}", e.what());
              app_state.is_logged_in = false;
            }
          },
          boost::asio::detached);
      app_state.wants_login = false;
    }

    if (app_state.wants_logout) {
    }
    if (app_state.wants_eventsub && app_state.is_logged_in &&
        !app_state.eventsub_active) {
      try {
        twitch_client =
            std::make_unique<sbot::tw::Chat>(connection, twitch_config);
        auto app_state_ptr =
            std::make_shared<sbot::core::AppState *>(&app_state);
        // start async connection *runs on thread pool)
        boost::asio::co_spawn(
            *connection->getIoContext(),
            twitch_client->start([app_state_ptr](std::string const &msg) {
              LOG_INFO("[Callback] Received message on thread, length: {}",
                       msg.length());
              try {
                nlohmann::json raw_msg = nlohmann::json::parse(msg);
                LOG_INFO("[Callback] Prased JSON successfully");
                if (raw_msg.contains("metadata")) {
                  LOG_INFO("[Callback] Has metatada");
                  if (raw_msg["metadata"]["message_type"] == "notification") {
                    LOG_INFO("[Callback] Is notification");
                    std::string type = raw_msg["metadata"]["subscription_type"];
                    LOG_INFO("[Callback] Type: {}", type);
                    if (type == "channel.chat.message") {
                      LOG_INFO("[Callback] Processing chat message");
                      std::string chatter =
                          raw_msg["payload"]["event"]["chatter_user_name"];
                      std::string text =
                          raw_msg["payload"]["event"]["message"]["text"];
                      std::string color = raw_msg["payload"]["event"]["color"];
                      LOG_INFO("[Callback] Chat from {}: {}", chatter, text);
                      // Thread-safe push (this runs on background thread)
                      (*app_state_ptr)
                          ->pushChatMessage(sbot::core::ChatMessage{
                              .user  = std::move(chatter),
                              .text  = std::move(text),
                              .color = std::move(color)});
                      LOG_INFO("[Callback] Message pushed, pending count: {}",
                               (*app_state_ptr)->pendingMessageCount());
                    }
                  }
                } else {
                  LOG_INFO("[Callback] Message type: {}", raw_msg.dump());
                }
              } catch (const std::exception &e) {
                LOG_ERROR("[Callback] Failed to parse EventSub message: {}",
                          e.what());
                LOG_ERROR("[Callback] Raw message: {}", msg);
              }
            }),
            boost::asio::detached);
        app_state.eventsub_active = true;
        app_state.wants_eventsub  = false;
      } catch (const std::exception &e) {
        LOG_ERROR("Twitch eventsub failed: {}", e.what());
      }
    }

    // if (app_state.eventsub_active && twitch_client) {
    //   twitch_client->pollIo();
    // }

    glfwSwapBuffers(window);
  }
  LOG_INFO("Main loop ending, shutting down gracefully");
  // Shutdown twitch client gracefully
  if (twitch_client && app_state.eventsub_active) {
    try {
      LOG_INFO("Shutting down Twitch client");
      auto shutdown_task = twitch_client->shutdown();
      auto timeout_timer = boost::asio::steady_timer(
          *connection->getIoContext(), std::chrono::seconds(3));
      bool shutdown_completed = false;
      boost::asio::co_spawn(
          *connection->getIoContext(),
          [&]() -> boost::asio::awaitable<void> {
            try {
              co_await twitch_client->shutdown();
              shutdown_completed = true;
              LOG_INFO("Twitch client shutdown completed");
            } catch (const std::exception &err) {
              LOG_ERROR("Error during graceful shutdown: {}", err.what());
            }
          },
          boost::asio::detached);
      // Wait for completion or timeout
      auto start_time = std::chrono::steady_clock::now();
      while (!shutdown_completed &&
             std::chrono::steady_clock::now() - start_time <
                 std::chrono::seconds(3)) {
        connection->getIoContext()->poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      if (!shutdown_completed) {
        LOG_WARN("Twitch client shutdown timed out");
      }
    } catch (const std::exception &err) {
      LOG_ERROR("Error during Twitch client shutdown: {}", err.what());
    }
  }
  twitch_client.reset();
  app_state.eventsub_active = false;
  LOG_INFO("Starting connection manager shutdown");

  // Cleanup
  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
