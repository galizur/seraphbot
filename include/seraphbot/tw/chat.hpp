#ifndef SBOT_TW_CHAT_HPP
#define SBOT_TW_CHAT_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "seraphbot/core/connection_manager.hpp"

namespace sbot::tw {
struct ClientConfig {
  std::string host       = "eventsub.wss.twitch.tv";
  std::string port       = "443";
  std::string helix_host = "api.twitch.tv";
  std::string helix_port = "443";

  std::string client_id;
  std::string access_token;   // raw token (no "oauth:" prefix)
  std::string broadcaster_id; // user id of the channel you want events for
};

class Chat {
public:
  using on_notify_fn = std::function<void(const std::string &json_text)>;

  Chat(std::shared_ptr<core::ConnectionManager> conn_manager, ClientConfig cfg);
  ~Chat();

  auto start(on_notify_fn callback) -> boost::asio::awaitable<void>;
  // auto pollIo() -> void;

  // Helper: send a chat message using Helix chat/messages
  // message - the text to send
  // sender_id - the user id of the sender (bot account id)
  /*auto sendChatMessage(const std::string &message, const std::string &sender_id)
      -> void;*/

  auto shutdown() -> boost::asio::awaitable<void>;
private:
  auto connect() -> boost::asio::awaitable<void>;
  auto reconnect() -> void;
  auto doRead() -> boost::asio::awaitable<void>;
  auto httpsPostAsync(
      const std::string &host, const std::string &port,
      const std::string &target, const std::string &body,
      const std::vector<std::pair<std::string, std::string>> &headers)
      -> boost::asio::awaitable<std::string>;

  ClientConfig m_cfg;
  std::shared_ptr<core::ConnectionManager> m_conn_manager;
  std::unique_ptr<boost::beast::ssl_stream<boost::asio::ip::tcp::socket>>
      m_stream;
  std::unique_ptr<boost::beast::websocket::stream<
      boost::beast::ssl_stream<boost::asio::ip::tcp::socket>>>
      m_wss;
  boost::beast::flat_buffer m_buffer;
  on_notify_fn m_callback;
  std::chrono::system_clock::time_point m_expires_at;
  std::string m_session_id;
};

} // namespace sbot::tw

#endif
