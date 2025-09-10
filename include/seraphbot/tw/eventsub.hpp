#ifndef SBOT_TW_EVENTSUB_HPP
#define SBOT_TW_EVENTSUB_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>

#include "seraphbot/core/connection_manager.hpp"
#include "seraphbot/tw/config.hpp"

namespace sbot::tw {

class EventSub {
public:
  using on_notify_fn = std::function<void(const std::string &json_text)>;

  EventSub(std::shared_ptr<core::ConnectionManager> conn_manager,
           ClientConfig cfg);
  ~EventSub();

  auto start(on_notify_fn callback) -> boost::asio::awaitable<void>;
  auto subscribe(nlohmann::json subscription_request)
      -> boost::asio::awaitable<void>;
  auto shutdown() -> boost::asio::awaitable<void>;

auto getSessionId() -> std::string {  return m_session_id; }
auto getTwitchConfig() -> ClientConfig { return m_cfg; }

private:
  auto connect() -> boost::asio::awaitable<void>;
  auto reconnect() -> void;
  auto doRead() -> boost::asio::awaitable<void>;

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
