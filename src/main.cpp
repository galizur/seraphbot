#include <cstdlib>
#include <stdexcept>
#include <string_view>

#include "seraphbot/application.hpp"
#include "seraphbot/core/logging.hpp"

auto main() -> int {
  sbot::core::Logger::init({});
  sbot::core::Logger::instance().setLevel(sbot::core::LogLevel::Trace);
  LOG_CONTEXT("Main");
  LOG_TRACE("Check");

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
  return 0;
}
