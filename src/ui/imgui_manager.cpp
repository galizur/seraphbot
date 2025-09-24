#include "seraphbot/ui/imgui_manager.hpp"

#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <imgui.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "seraphbot/core/app_state.hpp"
#include "seraphbot/core/logging.hpp"
#include "seraphbot/core/twitch_service.hpp"
#include "seraphbot/ui/firasans_font.hpp"
#include "seraphbot/ui/imgui_backend.hpp"
#include "seraphbot/viewmodels/auth_viewmodel.hpp"
#include "seraphbot/viewmodels/chat_viewmodel.hpp"
#include "seraphbot/viewmodels/discord_viewmodel.hpp"

sbot::ui::ImGuiManager::ImGuiManager(std::unique_ptr<ImGuiBackend> backend,
                                     core::AppState &appstate)
    : state{appstate}, m_backend{std::move(backend)},
      m_message_input(256, '\0') {
  LOG_CONTEXT("ImGuiManager");
  LOG_INFO("Initializing");
  initWindow();
  IMGUI_CHECKVERSION();
  m_context   = ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui::StyleColorsDark();
  ImFontGlyphRangesBuilder builder;
  builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
  ImVector<ImWchar> ranges;
  builder.BuildRanges(&ranges);
  ImFontConfig cfg;
  cfg.FontDataOwnedByAtlas = false;
  io.Fonts->AddFontFromMemoryTTF(FiraSans_Regular_ttf, FiraSans_Regular_ttf_len,
                                 18.0F, &cfg, ranges.Data);
  m_backend->init(m_window);
}

sbot::ui::ImGuiManager::~ImGuiManager() {
  LOG_CONTEXT("ImGuiManager");
  LOG_INFO("Shutting down");
  if (m_backend) {
    m_backend->shutdown();
    m_backend.reset();
  }
  if (m_context) {
    ImGui::DestroyContext(m_context);
    m_context = nullptr;
  }
  if (m_window) {
    glfwDestroyWindow(m_window);
    m_window = nullptr;
  }
  glfwTerminate();
}

void sbot::ui::ImGuiManager::beginFrame() {
  m_backend->newFrame();
  ImGui::NewFrame();
}

void sbot::ui::ImGuiManager::endFrame() { ImGui::Render(); }

void sbot::ui::ImGuiManager::render() { m_backend->renderDrawData(); }

void sbot::ui::ImGuiManager::poll() {
  glfwPollEvents();
  glClearColor(0.1F, 0.1F, 0.1F, 1.0F);
  glClear(GL_COLOR_BUFFER_BIT);
}

auto sbot::ui::ImGuiManager::shouldClose() -> bool {
  return glfwWindowShouldClose(m_window) != 0;
}

auto sbot::ui::ImGuiManager::swapBuffers() -> void {
  glfwSwapBuffers(m_window);
}

auto sbot::ui::ImGuiManager::initWindow(int width, int height) -> void {
  if (glfwInit() == 0) {
    LOG_ERROR("Failed to initialize GLFW");
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

  m_window = glfwCreateWindow(width, height, "SeraphBot", nullptr, nullptr);
  if (m_window == nullptr) {
    glfwTerminate();
    LOG_ERROR("Failed to create window");
    throw std::runtime_error("Failed to create window");
  }
  glfwMakeContextCurrent(m_window);
  glfwSwapInterval(1);
}

// For later
auto sbot::ui::ImGuiManager::initWindowVulkan(int width, int height) -> void {
  if (glfwInit() == 0) {
    LOG_ERROR("Failed to initialize GLFW");
    throw std::runtime_error("Failed to init GLFW");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Important for Vulkan

  m_window = glfwCreateWindow(width, height, "SeraphBot", nullptr, nullptr);
  if (!m_window) {
    glfwTerminate();
    LOG_ERROR("Failed to create window");
    throw std::runtime_error("Failed to create window");
  }
}

auto sbot::ui::ImGuiManager::hexToImVec4(const std::string &hex) -> ImVec4 {
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

auto sbot::ui::ImGuiManager::manageDocking() -> void {
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
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
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
}

auto sbot::ui::ImGuiManager::manageFloating() -> void {
  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImVec2 window_size(200, 20);
  float padding = 10.0F;
  ImVec2 pos(viewport->Pos.x + viewport->Size.x - window_size.x - padding,
             viewport->Pos.y + viewport->Size.y - window_size.y - padding);
  ImGui::SetNextWindowPos(pos);
  ImGui::SetNextWindowSize(window_size);
  ImGui::Begin("StatusBar", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoFocusOnAppearing |
                   ImGuiWindowFlags_NoDocking);
  ImGui::Text("FPS: %.1F", ImGui::GetIO().Framerate);
  ImGui::End();
}

auto sbot::ui::ImGuiManager::manageAuth(sbot::viewmodels::AuthVM &auth_vm)
    -> void {
  // Auth UI
  ImGui::Begin("Auth");
  auth_vm.syncFrom();

  switch (auth_vm.getState()) {
  case sbot::core::TwitchService::State::Disconnected:
    if (ImGui::Button("Login to Twitch")) {
      auth_vm.login();
    }
    break;
  case sbot::core::TwitchService::State::LoggingIn:
    ImGui::Text("Logging in...");
    break;
  case sbot::core::TwitchService::State::LoggedIn:
    ImGui::Text("Logged in as: %s", auth_vm.currentUser().c_str());
    if (ImGui::Button("Connect to Chat")) {
      auth_vm.connect();
    }
    if (ImGui::Button("Logout")) {
      auth_vm.disconnect();
    }
    break;
  case sbot::core::TwitchService::State::ConnectingToChat:
    ImGui::Text("Connecting to chat...");
    break;
  case sbot::core::TwitchService::State::ChatConnected:
    ImGui::Text("Connected! User: %s", auth_vm.currentUser().c_str());
    if (ImGui::Button("Disconnect")) {
      auth_vm.disconnect();
    }
    break;
  case sbot::core::TwitchService::State::Error:
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error occurred");
    if (ImGui::Button("Reset")) {
      auth_vm.disconnect();
    }
    break;
  }
  ImGui::End();
}

auto sbot::ui::ImGuiManager::manageChat(sbot::viewmodels::ChatVM &chat_vm)
    -> void {
  // Chat UI
  ImGui::Begin("Chat");
  if (chat_vm.canSendMessages()) {
    // Process any pending messages
    state.processPendingMessages();

    ImGui::BeginChild("ScrollingRegion",
                      ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushTextWrapPos(0.0F);
    for (auto &msg : state.chat_log) {
      ImGui::TextColored(hexToImVec4(msg.color), "%s: ", msg.user.c_str());
      ImGui::SameLine();
      ImGui::TextWrapped(msg.text.c_str());
    }
    ImGui::PopTextWrapPos();
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
      ImGui::SetScrollHereY(1.0F);
    }
    ImGui::EndChild();

    ImGui::SetNextItemWidth(-80);
    if (ImGui::InputText("##message", m_message_input.data(),
                         m_message_input.size(),
                         ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine)) {
      std::string msg{m_message_input.data()};
      if (!msg.empty()) {
        chat_vm.sendMessage(msg);
        m_message_input[0] = '\0';
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Send")) {
      std::string msg{m_message_input.data()};
      if (!msg.empty()) {
        chat_vm.sendMessage(msg);
        m_message_input[0] = '\0';
      }
    }
  } else {
    ImGui::Text("Connect to chat to send messages");
  }
  ImGui::End();
}

auto sbot::ui::ImGuiManager::manageDiscord(
    sbot::viewmodels::DiscordVM &discord_vm) -> void {
  ImGui::Begin("Discord Settings");
  ImGui::Text("Discord WebHook URL");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(-1.0F);
  ImGui::InputText("##webhookurl", discord_vm.webhook_url,
                   sizeof(discord_vm.webhook_url));
  if (ImGui::Button("Apply")) {
    discord_vm.syncTo(discord_vm.webhook_url);
  }
  ImGui::SameLine();
  ImGui::Text(discord_vm.syncFrom().c_str());
  ImGui::InputText("##notification_message", discord_vm.notification_text,
                   sizeof(discord_vm.notification_text));
  ImGui::SameLine();
  if (ImGui::Button("Test")) {
    discord_vm.testMessage(discord_vm.notification_text);
  }
  ImGui::End();
}
