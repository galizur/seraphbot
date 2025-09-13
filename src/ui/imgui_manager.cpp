#include "seraphbot/ui/imgui_manager.hpp"

#include "seraphbot/core/app_state.hpp"
#include "seraphbot/core/logging.hpp"
#include "seraphbot/ui/imgui_backend.hpp"
#include "seraphbot/ui/firasans_font.hpp"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>
#include <memory>

sbot::ui::ImGuiManager::ImGuiManager(std::unique_ptr<ImGuiBackend> backend,
                                     core::AppState &appstate,
                                     GLFWwindow *window)
    : state{appstate}, m_backend{std::move(backend)}, m_window{window} {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui::StyleColorsDark();
  ImFontGlyphRangesBuilder builder;
  builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
  //builder.AddRanges(io.Fonts->GetGlyphRangesGreek());
  //builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
  ImVector<ImWchar> ranges;
  builder.BuildRanges(&ranges);
  io.Fonts->AddFontFromMemoryTTF(FiraSans_Regular_ttf, FiraSans_Regular_ttf_len,
                                 18.0F, nullptr, ranges.Data);
  m_backend->init(m_window);
}

sbot::ui::ImGuiManager::~ImGuiManager() {
  LOG_DEBUG("Shutting down ImGuiManager");
  m_backend->shutdown();
  ImGui::DestroyContext();
}

void sbot::ui::ImGuiManager::beginFrame() {
  m_backend->newFrame();
  ImGui::NewFrame();
}

void sbot::ui::ImGuiManager::endFrame() { ImGui::Render(); }

void sbot::ui::ImGuiManager::render() { m_backend->renderDrawData(); }
