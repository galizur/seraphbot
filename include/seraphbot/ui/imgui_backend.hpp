#ifndef SBOT_UI_IMGUI_BACKEND_HPP
#define SBOT_UI_IMGUI_BACKEND_HPP

struct GLFWwindow;

namespace sbot::ui {

class ImGuiBackend {
public:
  virtual ~ImGuiBackend()               = default;
  virtual void init(GLFWwindow *window) = 0;
  virtual void newFrame()               = 0;
  virtual void renderDrawData()         = 0;
  virtual void shutdown()               = 0;
};

} // namespace sbot::ui

#endif
