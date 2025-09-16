#include "seraphbot/core/twitch_service.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <chrono>
#include <exception>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

#include "seraphbot/core/chat_message.hpp"
#include "seraphbot/core/connection_manager.hpp"
#include "seraphbot/core/logging.hpp"
#include "seraphbot/tw/auth.hpp"
#include "seraphbot/tw/chat/read.hpp"
#include "seraphbot/tw/chat/send.hpp"
#include "seraphbot/tw/config.hpp"
#include "seraphbot/tw/eventsub.hpp"
// Test
#include "seraphbot/discord/notifications.hpp"

namespace {
namespace sbc  = sbot::core;
namespace sbt  = sbot::tw;
namespace asio = boost::asio;
} // namespace

sbc::TwitchService::TwitchService(
    std::shared_ptr<sbc::ConnectionManager> connection, tw::ClientConfig cfg)
    : m_connection{std::move(connection)},
      m_auth{std::make_unique<sbt::Auth>(
          m_connection, "seraphbot-oauth-server.onrender.com")},
      m_config{std::move(cfg)} {
  LOG_CONTEXT("TwitchService");
  LOG_INFO("Initializing");
}

sbc::TwitchService::~TwitchService() { LOG_INFO("Shutting down"); }

auto sbc::TwitchService::startLogin() -> void {
  if (m_state != State::Disconnected) {
    LOG_WARN("Login requested but not in disconnected state");
    return;
  }

  setState(State::LoggingIn, "Starting login process...");

  asio::co_spawn(
      *m_connection->getIoContext(),
      [this]() -> asio::awaitable<void> {
        try {
          co_await m_auth->loginAsync();
          auto user_info{co_await m_auth->fetchUserInfoAsync()};
          m_current_user          = user_info.display_name;
          m_config.access_token   = m_auth->accessToken();
          m_config.broadcaster_id = user_info.id;
          m_config.client_id      = m_auth->clientId();

          setState(State::LoggedIn, "Logged in as " + m_current_user);
        } catch (const std::exception &err) {
          LOG_ERROR("Login failed: {}", err.what());
          setState(State::Error, "Login failed: " + std::string(err.what()));
        }
        co_return;
      },
      asio::detached);
}

auto sbc::TwitchService::connectToChat() -> asio::awaitable<void> {
  if (m_state != State::LoggedIn) {
    LOG_WARN("Chat connection requested but not logged in");
    co_return;
  }

  setState(State::ConnectingToChat, "Connecting to chat...");

  try {
    setupChatServices();

    auto timeout_timer = asio::steady_timer{*m_connection->getIoContext(),
                                            std::chrono::seconds(10)};
    co_await m_eventsub->connect();

    LOG_INFO("EventSub connected, starting read loop");

    asio::co_spawn(
        *m_connection->getIoContext(),
        [this]() -> asio::awaitable<void> {
          try {
            co_await m_eventsub->doRead();
          } catch (const std::exception &err) {
            LOG_ERROR("Read loop error: {}", err.what());
          }
        },
        asio::detached);

    asio::co_spawn(*m_connection->getIoContext(),
                   m_eventsub->start([this](const std::string &msg) {
                     try {
                       handleEventSubMessage(msg);
                     } catch (const std::exception &err) {
                       LOG_ERROR("Read loop error: {}", err.what());
                     }
                   }),
                   asio::detached);

    co_await asio::steady_timer{*m_connection->getIoContext(),
                                std::chrono::milliseconds(500)}
        .async_wait(asio::use_awaitable);

    co_await m_chat_read->requestSubscription();
    co_spawn(co_await boost::asio::this_coro::executor,
             m_eventsub->subscribe("channel.update", "2"),
             boost::asio::detached); // TODO: Find details
    // co_await m_eventsub->subscribe("channel.follow", "2"); // TODO: update
    // subscribe function
    co_spawn(co_await boost::asio::this_coro::executor,
             m_eventsub->subscribe("channel.ad_break.begin", "1"),
             boost::asio::detached); // TODO: Find details
    co_spawn(co_await boost::asio::this_coro::executor,
             m_eventsub->subscribe("channel.chat.clear", "1"),
             boost::asio::detached);
    co_spawn(co_await boost::asio::this_coro::executor,
             m_eventsub->subscribe("channel.chat.clear_user_messages", "1"),
             boost::asio::detached);
    co_spawn(co_await boost::asio::this_coro::executor,
             m_eventsub->subscribe("channel.chat.message_delete", "1"),
             boost::asio::detached);
    co_spawn(co_await boost::asio::this_coro::executor,
             m_eventsub->subscribe("channel.chat.notification", "1"),
             boost::asio::detached);
    co_spawn(co_await boost::asio::this_coro::executor,
             m_eventsub->subscribe("stream.online", "1"),
             boost::asio::detached);

    // LOG_DEBUG("Testing Discord");
    // discord::Notifications notif{
    //     m_connection,
    //     m_config,
    // };
    // co_await notif.sendMessage("We are live - test!");
    // LOG_DEBUG("End Discord test");

    setState(State::ChatConnected, "Connected to chat");
  } catch (const std::exception &err) {
    LOG_ERROR("Chat connection failed: {}", err.what());
    setState(State::Error,
             "Chat connection failed: " + std::string(err.what()));
  }
}

auto sbc::TwitchService::sendMessage(const std::string &message) -> void {
  if (m_state != State::ChatConnected) {
    LOG_WARN("Cannot send message: not connected to chat");
    return;
  }

  asio::co_spawn(
      *m_connection->getIoContext(),
      [this, message]() -> asio::awaitable<void> {
        try {
          co_await m_chat_send->message(message, m_config.broadcaster_id);
          LOG_INFO("Sent message: {}", message);
        } catch (const std::exception &err) {
          LOG_ERROR("Failed to send message: {}", err.what());
          if (m_status_callback) {
            m_status_callback("Failed to send message: " +
                              std::string(err.what()));
          }
        }
      },
      asio::detached);
}

auto sbc::TwitchService::disconnect() -> void {
  setState(State::Disconnected, "Disconnected");

  m_chat_read.reset();
  m_chat_send.reset();
  m_eventsub.reset();
  m_current_user.clear();
}

auto sbc::TwitchService::setState(State state, const std::string &status)
    -> void {
  m_state = state;
  LOG_INFO("State: {} - {}", static_cast<int>(state), status);

  if (m_status_callback && !status.empty()) {
    m_status_callback(status);
  }
}

auto sbc::TwitchService::setupChatServices() -> void {
  m_eventsub  = std::make_shared<tw::EventSub>(m_connection, m_config);
  m_chat_read = std::make_unique<tw::chat::Read>(m_eventsub);
  m_chat_send = std::make_unique<tw::chat::Send>(m_connection, m_config);
}

auto sbc::TwitchService::handleEventSubMessage(const std::string &msg) -> void {
  LOG_INFO("Received message, length: {}", msg.length());

  try {
    nlohmann::json raw_msg = nlohmann::json::parse(msg);

    if (!raw_msg.contains("metadata")) {
      return;
    }

    if (raw_msg["metadata"]["message_type"] != "notification") {
      return;
    }

    std::string type{raw_msg["metadata"]["subscription_type"]};

    if (type == "channel.chat.message" && m_message_callback) {
      if (!raw_msg["payload"]["event"].contains("chatter_user_name") ||
          !raw_msg["payload"]["event"].contains("message") ||
          !raw_msg["payload"]["event"]["message"].contains("text")) {
        LOG_WARN("Incomplete chat message received");
        return;
      }
      std::string chatter{raw_msg["payload"]["event"]["chatter_user_name"]};
      std::string text{raw_msg["payload"]["event"]["message"]["text"]};
      std::string color{raw_msg["payload"]["event"]["color"]};

      LOG_INFO("Chat from {}: {}", chatter, text);

      ChatMessage chat_msg{.user  = std::move(chatter),
                           .text  = std::move(text),
                           .color = std::move(color)};
      m_message_callback(chat_msg);
    }
    if (type == "channel.ad_break.begin" && m_message_callback) {
      std::string text{raw_msg["payload"]["event"]["duration_seconds"]};
      LOG_INFO("System message:");
      ChatMessage chat_msg{.user  = "System",
                           .text  = text + " second ad break beginning.",
                           .color = "#AAAAAA"};
      m_message_callback(chat_msg);
    }
    if (type == "channel.chat.clear" && m_message_callback) {
      LOG_INFO("System message:");
      ChatMessage chat_msg{
          .user = "System", .text = "Chat clear requested", .color = "#AAAAAA"};
      m_message_callback(chat_msg);
    }
  } catch (const std::exception &err) {
    LOG_ERROR("Failed to parse EventSub message: {}", err.what());
  }
}
