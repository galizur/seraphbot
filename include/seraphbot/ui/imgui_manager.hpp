#ifndef SBOT_UI_IMGUI_MANAGER_HPP
#define SBOT_UI_IMGUI_MANAGER_HPP

#include "seraphbot/core/app_state.hpp"
#include "seraphbot/ui/imgui_backend.hpp"

#include <memory>

struct GLFWwindow;

namespace sbot::ui {

class ImGuiManager {
public:
  explicit ImGuiManager(std::unique_ptr<ImGuiBackend> backend,
                        core::AppState &appstate, GLFWwindow *window);
  ~ImGuiManager();

  void beginFrame();
  void endFrame();
  void render();

  core::AppState &state;

private:
  std::unique_ptr<ImGuiBackend> m_backend;
  GLFWwindow *m_window;
};

} // namespace sbot::ui

#endif
