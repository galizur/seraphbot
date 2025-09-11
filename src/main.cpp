#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <array>
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
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "seraphbot/core/app_state.hpp"
#include "seraphbot/core/connection_manager.hpp"
#include "seraphbot/core/logging.hpp"
#include "seraphbot/core/twitch_service.hpp"
#include "seraphbot/tw/auth.hpp"
#include "seraphbot/ui/imgui_backend_opengl.hpp"
#include "seraphbot/ui/imgui_manager.hpp"

auto hexToImVec4 = [](const std::string &hex) -> ImVec4 {
  if (hex.size() < 7 || hex.at(0) != '#') {
    return {0.0F, 0.0F, 0.0F, 1.0F};
  }
  unsigned long value = std::stoul(hex.substr(1, 6), nullptr, 16);

  float red   = static_cast<float>((value >> 16) & 0xFF) / 255.0F;
  float green = static_cast<float>((value >> 8) & 0xFF) / 255.0F;
  float blue  = static_cast<float>(value & 0xFF) / 255.0F;
  float alpha = 1.0F;

  if (hex.size() >= 9) {
    unsigned long alpha_val = std::stoul(hex.substr(7, 2), nullptr, 16);
    alpha                   = static_cast<float>(alpha_val) / 255.0F;
  }
  return {red, green, blue, alpha};
};

auto initWindow(int width = 1820, int height = 720) -> GLFWwindow * {
  if (glfwInit() == 0) {
    throw std::runtime_error("Failed to init GLFW");
  }
#ifdef __WIN32
  glfwWindowHint(GLFW_PLATFORM, GLFW_PLATFORM_WIN32);
#elif __linux__
  glfwWindowHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
#else
  glfwWindowHint(GLFW_PLATFORM, GLFW_PLATFORM_COCOA);
#endif

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
  sbot::core::Logger::instance().setLevel(sbot::core::LogLevel::Trace);
  LOG_CONTEXT("Main");
  LOG_INFO("SeraphBot starting...");

  LOG_INFO("Initializing window");
  GLFWwindow *window = initWindow();
  LOG_INFO("Initializing app state");
  sbot::core::AppState app_state;

  LOG_INFO("Selecting OpenGL backend");
  auto backend{std::make_unique<sbot::ui::ImGuiBackendOpenGL>()};
  LOG_INFO("Initializing GUI manager");
  sbot::ui::ImGuiManager imgui_manager(std::move(backend), app_state, window);

  auto connection{std::make_shared<sbot::core::ConnectionManager>(2)};
  auto twitch_service{std::make_unique<sbot::core::TwitchService>(connection)};

  // Setup callbacks
  twitch_service->setMessageCallback(
      [&app_state](const sbot::core::ChatMessage &msg) {
        app_state.pushChatMessage(msg);
      });
  twitch_service->setStatusCallback([&app_state](const std::string &status) {
    app_state.last_status = status;
  });

  std::vector<char> message_input(265, '\0');

  LOG_INFO("Entering main loop");
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    glClearColor(0.1F, 0.1F, 0.1F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    imgui_manager.beginFrame();
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |=
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(2);

    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    // Menu bar
    if (ImGui::BeginMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        // if (ImGui::MenuItem("Settings", nullptr, &show_settings_)) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {
          glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("View")) {
        // ImGui::MenuItem("Event Log", nullptr, &show_event_log_);
        // ImGui::MenuItem("Statistics", nullptr, &show_statistics_);
        ImGui::EndMenu();
      }

      ImGui::EndMenuBar();
    }

    ImGui::End(); // End dockspace

    // Auth UI
    ImGui::Begin("Auth");
    auto state = twitch_service->getState();

    switch (state) {
    case sbot::core::TwitchService::State::Disconnected:
      if (ImGui::Button("Login to Twitch")) {
        twitch_service->startLogin();
      }
      break;
    case sbot::core::TwitchService::State::LoggingIn:
      ImGui::Text("Logging in...");
      break;
    case sbot::core::TwitchService::State::LoggedIn:
      ImGui::Text("Logged in as: %s", twitch_service->getCurrentUser().c_str());
      if (ImGui::Button("Connect to Chat")) {
        boost::asio::co_spawn(*connection->getIoContext(),
                              twitch_service->connectToChat(),
                              boost::asio::detached);
      }
      if (ImGui::Button("Logout")) {
        twitch_service->disconnect();
      }
      break;
    case sbot::core::TwitchService::State::ConnectingToChat:
      ImGui::Text("Connecting to chat...");
      break;
    case sbot::core::TwitchService::State::ChatConnected:
      ImGui::Text("Connected! User: %s",
                  twitch_service->getCurrentUser().c_str());
      if (ImGui::Button("Disconnect")) {
        twitch_service->disconnect();
      }
      break;
    case sbot::core::TwitchService::State::Error:
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error occured");
      if (ImGui::Button("Reset")) {
        twitch_service->disconnect();
      }
      break;
    }
    ImGui::End();

    // Chat UI
    ImGui::Begin("Chat");
    if (twitch_service->canSendMessages()) {
      // Process any pending messages
      app_state.processPendingMessages();

      ImGui::BeginChild("ScrollingRegion",
                        ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true,
                        ImGuiWindowFlags_HorizontalScrollbar);
      for (auto &msg : app_state.chat_log) {
        ImGui::TextColored(hexToImVec4(msg.color), "%s: ", msg.user.c_str());
        ImGui::SameLine();
        ImGui::TextUnformatted(msg.text.c_str());
      }
      if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0F);
      }
      ImGui::EndChild();

      ImGui::SetNextItemWidth(-80);
      if (ImGui::InputText("##message", message_input.data(),
                           message_input.size(),
                           ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string msg{message_input.data()};
        if (!msg.empty()) {
          twitch_service->sendMessage(msg);
          message_input[0] = '\0';
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Send")) {
        std::string msg{message_input.data()};
        if (!msg.empty()) {
          twitch_service->sendMessage(msg);
          message_input[0] = '\0';
        }
      }
    } else {
      ImGui::Text("Connect to chat to send messages");
    }
    ImGui::End();

    imgui_manager.endFrame();
    imgui_manager.render();
    glfwSwapBuffers(window);
  }
  LOG_INFO("Main loop ending, shutting down gracefully");
  twitch_service->disconnect();
  //  connection->stop()  ;
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
