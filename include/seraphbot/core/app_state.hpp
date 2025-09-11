#ifndef APP_STATE_HPP
#define APP_STATE_HPP

#include <cstddef>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "seraphbot/core/chat_message.hpp"

namespace sbot::core {

class AppState {
public:
  std::vector<ChatMessage> chat_log;
  std::string last_status;

  bool show_debug_window{false};
  bool auto_scroll_chat{true};

  auto pushChatMessage(const ChatMessage &msg) -> void;
  auto processPendingMessages() -> void;

  [[nodiscard]] auto pendingMessageCount() const -> std::size_t;

private:
  std::queue<ChatMessage> m_pending_messages;
  mutable std::mutex m_message_mutex;
};

} // namespace sbot::core
#endif
