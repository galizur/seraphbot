#include "seraphbot/core/app_state.hpp"

#include <cstddef>
#include <mutex>
#include <utility>

#include "seraphbot/core/chat_message.hpp"
#include "seraphbot/core/logging.hpp"

sbot::core::AppState::AppState() {
  LOG_CONTEXT("AppState");
  LOG_INFO("Initializing");
}

sbot::core::AppState::~AppState() {
  LOG_CONTEXT("AppState");
  LOG_INFO("Shutting Down");
}

auto sbot::core::AppState::pushChatMessage(const ChatMessage &msg) -> void {
  std::lock_guard<std::mutex> lock{m_message_mutex};
  m_pending_messages.push(msg);
}

auto sbot::core::AppState::processPendingMessages() -> void {
  std::lock_guard<std::mutex> lock{m_message_mutex};

  while (!m_pending_messages.empty()) {
    chat_log.push_back(std::move(m_pending_messages.front()));
    m_pending_messages.pop();
  }
}

auto sbot::core::AppState::pendingMessageCount() const -> std::size_t {
  std::lock_guard<std::mutex> lock{m_message_mutex};
  return m_pending_messages.size();
}
