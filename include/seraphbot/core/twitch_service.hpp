#ifndef SBOT_CORE_TWITCH_SERVICE_HPP
#define SBOT_CORE_TWITCH_SERVICE_HPP

#include <boost/asio/awaitable.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "seraphbot/core/chat_message.hpp"
#include "seraphbot/core/connection_manager.hpp"
#include "seraphbot/tw/auth.hpp"
#include "seraphbot/tw/chat/read.hpp"
#include "seraphbot/tw/chat/send.hpp"
#include "seraphbot/tw/config.hpp"
#include "seraphbot/tw/eventsub.hpp"

namespace sbot::core {

class TwitchService {
public:
  using MessageCallback = std::function<void(const ChatMessage &)>;
  using StatusCallback  = std::function<void(const std::string &)>;

  enum class State : std::uint8_t {
    Disconnected,
    LoggingIn,
    LoggedIn,
    ConnectingToChat,
    ChatConnected,
    Error
  };

  explicit TwitchService(std::shared_ptr<ConnectionManager> connection,
                         tw::ClientConfig cfg);
  ~TwitchService();

  auto startLogin() -> void;
  auto connectToChat() -> boost::asio::awaitable<void>;
  auto sendMessage(const std::string &message) -> void;
  auto disconnect() -> void;

  [[nodiscard]] auto getState() const -> State { return m_state; }
  [[nodiscard]] auto canConnectToChat() const -> bool {
    return m_state == State::LoggedIn;
  }
  [[nodiscard]] auto canSendMessages() const -> bool {
    return m_state == State::ChatConnected;
  }
  [[nodiscard]] auto getCurrentUser() const -> std::string {
    return m_current_user;
  }

  auto setMessageCallback(MessageCallback callback) -> void {
    m_message_callback = std::move(callback);
  }
  auto setStatusCallback(StatusCallback callback) -> void {
    m_status_callback = std::move(callback);
  }

private:
  std::shared_ptr<ConnectionManager> m_connection;
  std::unique_ptr<tw::Auth> m_auth;
  std::shared_ptr<tw::EventSub> m_eventsub;
  std::unique_ptr<tw::chat::Read> m_chat_read;
  std::unique_ptr<tw::chat::Send> m_chat_send;
  tw::ClientConfig m_config;

  State m_state{State::Disconnected};
  std::string m_current_user;

  MessageCallback m_message_callback;
  StatusCallback m_status_callback;

  auto setState(State state, const std::string &status = "") -> void;
  auto handleEventSubMessage(const std::string &msg) -> void;
  auto processLoginResult() -> void;
  auto setupChatServices() -> void;
};

} // namespace sbot::core

#endif
