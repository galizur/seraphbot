#ifndef SBOT_UI_IMGUI_BACKEND_OPENGL_HPP
#define SBOT_UI_IMGUI_BACKEND_OPENGL_HPP

#include "seraphbot/core/logging.hpp"
#include "seraphbot/ui/imgui_backend.hpp"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>

namespace sbot::ui {

class ImGuiBackendOpenGL : public ImGuiBackend {
public:
  ImGuiBackendOpenGL() {
    LOG_CONTEXT("ImGuiBackendOpenGL");
    LOG_INFO("Initializing");
  }

  ~ImGuiBackendOpenGL() override {
    LOG_CONTEXT("ImGuiBbackendOpenGL");
    LOG_INFO("Shutting down");
  }

  void init(GLFWwindow *window) override {
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
  }

  void newFrame() override {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
  }

  void renderDrawData() override {
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  }

  void shutdown() override {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
  }
};

}; // namespace sbot::ui

#endif
