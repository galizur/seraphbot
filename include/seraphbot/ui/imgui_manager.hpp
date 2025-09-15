#ifndef SBOT_UI_IMGUI_MANAGER_HPP
#define SBOT_UI_IMGUI_MANAGER_HPP

#include <imgui.h>

#include "seraphbot/core/app_state.hpp"
#include "seraphbot/core/connection_manager.hpp"
#include "seraphbot/core/twitch_service.hpp"
#include "seraphbot/ui/imgui_backend.hpp"
#include "seraphbot/viewmodels/discord_viewmodel.hpp"

#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

namespace sbot::ui {

class ImGuiManager {
public:
  explicit ImGuiManager(std::unique_ptr<ImGuiBackend> backend,
                        core::AppState &appstate);
  ~ImGuiManager();

  void beginFrame();
  void endFrame();
  void render();

  auto initWindow(int width = 1280, int height = 720) -> void;
  auto initWindowVulkan(int width = 1280, int height = 720) -> void;
  auto hexToImVec4(const std::string &hex) -> ImVec4;

  auto getWindow() -> GLFWwindow * { return m_window; }

  core::AppState &state;

  auto manageDocking() -> void;
  auto manageFloating() -> void;
  auto manageAuth(sbot::core::TwitchService &twitch_service,
                  sbot::core::ConnectionManager &connection) -> void;
  auto manageChat(sbot::core::TwitchService &twitch_service) -> void;
  auto manageDiscord(sbot::viewmodels::DiscordVM &discord_vm) -> void;

private:
  std::unique_ptr<ImGuiBackend> m_backend;
  GLFWwindow *m_window;
  ImGuiContext *m_context;
  std::vector<char> m_message_input /*(256, '\0')*/;
};

} // namespace sbot::ui

#endif
