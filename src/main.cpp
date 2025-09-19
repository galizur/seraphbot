#include <cstdlib>
#include <stdexcept>
#include <string_view>

#include "seraphbot/application.hpp"
#include "seraphbot/core/logging.hpp"

auto main() -> int {
  sbot::core::Logger::init({});
  sbot::core::Logger::instance().setLevel(sbot::core::LogLevel::Trace);
  LOG_CONTEXT("Main");

  sbot::Application app;
  try {
    if (!app.initialize()) {
      LOG_ERROR("Failed to initialize application");
      return -1;
    }
    return app.run();
  } catch (std::runtime_error &err) {
    LOG_ERROR("{}", err.what());
  }
  // app.shutdown();

  // Setup callbacks
  // twitch_service->setMessageCallback(
  //     [&app_state](const sbot::core::ChatMessage &msg) {
  //       app_state.pushChatMessage(msg);
  //     });
  // twitch_service->setStatusCallback([&app_state](const std::string &status) {
  //   app_state.last_status = status;
  // });

  // while (glfwWindowShouldClose(imgui_manager.getWindow()) == 0) {
  //   imgui_manager.manageChat(*twitch_service);

  //   glfwSwapBuffers(imgui_manager.getWindow());
  // }
  // LOG_INFO("Main loop ending, shutting down gracefully");
  // twitch_service->disconnect();
  // //  connection->stop()  ;
  // std::this_thread::sleep_for(std::chrono::milliseconds(100));

  return 0;
}
