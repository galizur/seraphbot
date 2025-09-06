#ifndef APP_STATE_HPP
#define APP_STATE_HPP

#include <cstddef>
#include <deque>
#include <iterator>
#include <mutex>
#include <queue>
#include <string>

namespace sbot::core {

struct ChatMessage {
  std::string user;
  std::string text;
  std::string color;
};

struct AppState {
  // sbot::auth::twitch::Auth twitch_auth;
  //  std::unique_ptr<sbot::eventsub::twitch::Client> eventsub;
  std::string twitchAuthState;

  bool wants_login{false};
  bool wants_eventsub{false};
  bool wants_logout{false};

  bool is_logged_in{false};
  bool eventsub_active{false};

  std::deque<ChatMessage> chat_log;

  // Thread-safe communication
  mutable std::mutex chat_mutex;
  std::queue<ChatMessage> pending_chat_messages;

  // Thread-safe methods
  auto pushChatMessage(const ChatMessage &msg) -> void {
    std::lock_guard lock{chat_mutex};
    pending_chat_messages.push(msg);
  }

  auto pushChatMessage(ChatMessage &&msg) -> void {
    std::lock_guard lock{chat_mutex};
    pending_chat_messages.push(std::move(msg));
  }

  // Call before rendering
  auto processPendingMessages() -> void {
    std::lock_guard lock{chat_mutex};
    while (!pending_chat_messages.empty()) {
      chat_log.push_back(std::move(pending_chat_messages.front()));
      pending_chat_messages.pop();
    }
  }

  // Optional: get pending message count without processing
  [[nodiscard]] auto pendingMessageCount() const -> std::size_t {
    std::lock_guard lock{chat_mutex};
    return pending_chat_messages.size();
  }
};

} // namespace sbot::core
#endif
