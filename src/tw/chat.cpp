#include "seraphbot/tw/chat.hpp"

#include "seraphbot/core/connection_manager.hpp"
#include "seraphbot/core/logging.hpp"

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/string_type.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/websocket/stream_base.hpp>
#include <chrono>
#include <cstddef>
#include <exception>
#include <memory>
#include <nlohmann/json.hpp>
#include <openssl/tls1.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace ws    = beast::websocket;
namespace ssl   = asio::ssl;
using tcp       = asio::ip::tcp;
using json      = nlohmann::json;
} // namespace

namespace {
auto stripOauthPrefix(std::string token) -> std::string {
  if (token.starts_with("oauth:")) {
    return token.substr(6);
  }
  return token;
}
} // namespace

/* Helper: Perform a synchronous HTTPS POST returning the full body as string
  host example: "api.twitch.tv", target e.g. "/helix/eventsub/subscriptions"
 */
auto sbot::tw::Chat::httpsPostAsync(
    const std::string &host, const std::string &port, const std::string &target,
    const std::string &body,
    const std::vector<std::pair<std::string, std::string>> &headers)
    -> asio::awaitable<std::string> {
  auto stream = co_await m_conn_manager->makeSslStreamAsync(host, port);

  co_await stream->async_handshake(ssl::stream_base::client,
                                   asio::use_awaitable);

  // Set up HTTP request
  http::request<http::string_body> req{http::verb::post, target, 11};
  req.set(http::field::host, host);
  req.set(http::field::user_agent, "seraphbot-eventsub/0.1");
  req.set(http::field::content_type, "application/json");
  req.set(http::field::connection,
          "close"); // Important: signal connection close

  for (const auto &head : headers) {
    req.set(head.first, head.second);
  }
  req.body() = body;
  req.prepare_payload();

  co_await http::async_write(*stream, req, asio::use_awaitable);

  beast::flat_buffer buffer;
  http::response<http::dynamic_body> res;
  co_await http::async_read(*stream, buffer, res, asio::use_awaitable);

  // Extract response body before shutdown
  std::string response_body = beast::buffers_to_string(res.body().data());

  try {
    // Set a timeout for shutdown
    co_await stream->async_shutdown(boost::asio::use_awaitable);
  } catch (const beast::system_error &err_c) {
    if (err_c.code() != asio::error::eof &&
        err_c.code() != ssl::error::stream_truncated &&
        err_c.code() != beast::error::timeout) {
      LOG_WARN("SSL shutdown warning (usually harmless): {}", err_c.what());
    }
  }
  co_return response_body;
}

sbot::tw::Chat::Chat(std::shared_ptr<core::ConnectionManager> conn_manager,
                     ClientConfig cfg)
    : m_cfg{std::move(cfg)}, m_conn_manager{std::move(conn_manager)} {}

sbot::tw::Chat::~Chat() { LOG_DEBUG("Chat destructor called"); }

auto sbot::tw::Chat::start(on_notify_fn callback) -> asio::awaitable<void> {
  m_callback = std::move(callback);
  co_await connect();
}

/*auto sbot::tw::Chat::pollIo() -> void {
  while (m_conn_manager->getIoContext()->poll_one() != 0U) {
  }
}*/

auto sbot::tw::Chat::connect() -> asio::awaitable<void> {
  try {
    // Create async stream
    m_stream =
        co_await m_conn_manager->makeSslStreamAsync(m_cfg.host, m_cfg.port);

    // Async SSL handshake
    co_await m_stream->async_handshake(ssl::stream_base::client,
                                       asio::use_awaitable);

    // // Now upgrade to websocket over TLS
    m_wss = std::make_unique<ws::stream<beast::ssl_stream<tcp::socket>>>(
        std::move(*m_stream));

    // Set timeout options (optional)
    m_wss->set_option(
        ws::stream_base::timeout::suggested(beast::role_type::client));

    // Perform Async WebSocket handshake on /ws
    co_await m_wss->async_handshake(m_cfg.host, "/ws", asio::use_awaitable);

    m_wss->control_callback(
        [](ws::frame_type kind, beast::string_view /*payload*/) {
          if (kind == ws::frame_type::ping) {
            LOG_TRACE("Received ping, Beast will auto-pong");
          }
        });

    // Read welcome message
    beast::flat_buffer buffer;
    co_await m_wss->async_read(buffer, asio::use_awaitable);
    std::string welcome = beast::buffers_to_string(buffer.data());
    LOG_INFO("WELCOME: {}", welcome);

    // Parse JSON and extract session.id
    json j = json::parse(welcome);
    std::string session_id;
    try {
      session_id = j.at("payload").at("session").at("id").get<std::string>();
    } catch (...) {
      LOG_ERROR("Failed to extract session.id from welcome message");
      throw std::runtime_error(
          "Failed to extract session.id from welcome message");
    }
    m_session_id = session_id;

    LOG_INFO("[EventSub] session_id = {}", session_id);

    // Subscribe to channel.chat.message for your broadcaster_id
    json payload = {
        {"type",      "channel.chat.message"                               },
        {"version",   "1"                                                  },
        {"condition",
         {{"broadcaster_user_id", m_cfg.broadcaster_id},
          {"user_id", m_cfg.broadcaster_id}}                               },
        {"transport", {{"method", "websocket"}, {"session_id", session_id}}}
    };

    std::string token = stripOauthPrefix(m_cfg.access_token);

    std::vector<std::pair<std::string, std::string>> headers = {
        {"Client-Id",     m_cfg.client_id   },
        {"Authorization", "Bearer " + token },
        {"Content-Type",  "application/json"}
    };

    try {
      std::string resp = co_await httpsPostAsync(
          m_cfg.helix_host, m_cfg.helix_port, "/helix/eventsub/subscriptions",
          payload.dump(), headers);

      LOG_INFO("[EventSub] subscription response: {}", resp);

      // Parse the response to check if subscription was successful
      json sub_response = json::parse(resp);
      if (sub_response.contains("error")) {
        LOG_ERROR("[EventSub] Subscription failed: {}", sub_response.dump());
        throw std::runtime_error("EventSub subscription failed");
      }
    } catch (const std::exception &e) {
      LOG_ERROR("[EventSub] Exception during subscription: {}", e.what());
      throw; // Re-throw to prevent silent failure
    }

    // Optionally wait a bit for subscription to activate
    co_await asio::steady_timer{*m_conn_manager->getIoContext(),
                                std::chrono::milliseconds(300)}
        .async_wait(asio::use_awaitable);

    co_await doRead();
  } catch (const std::exception &ex) {
    LOG_ERROR("[EventSub] Connection error: {}", ex.what());
    throw;
  }
}

auto sbot::tw::Chat::doRead() -> asio::awaitable<void> {
  try {
    while (true) {
      auto bytes = co_await m_wss->async_read(m_buffer, asio::use_awaitable);

      std::string msg = beast::buffers_to_string(m_buffer.data());
      m_buffer.consume(bytes);

      LOG_INFO("[EventSub Raw] {}", msg);

      if (m_callback) {
        m_callback(msg);
      }
    }
  } catch (const beast::system_error &ec) {
    if (ec.code() == ws::error::closed) {
      LOG_WARN("[EventSub] WebSocket closed by server");
    } else if (ec.code() == asio::error::operation_aborted) {
      LOG_INFO("[EventSub] Read operation cancelled (normal during shutdown)");
    } else {
      LOG_ERROR("[EventSub] Read error: {}", ec.what());
    }
  } catch (const std::exception &ex) {
    LOG_ERROR("[EventSub] Unexpected error in doRead: {}", ex.what());
  }
}

/*void sbot::tw::Chat::sendChatMessage(const std::string &message,
                                     const std::string &sender_id) {
  // Helix chat/messages body
  json body = {
      {"broadcaster_id", m_cfg.broadcaster_id},
      {"message",        message             },
      {"sender_id",      sender_id           }
  };

  std::string token = stripOauthPrefix(m_cfg.access_token);
  std::vector<std::pair<std::string, std::string>> headers = {
      {"Client-Id",     m_cfg.client_id  },
      {"Authorization", "Bearer " + token}
  };

  std::string resp = co_await httpsPostAsync(
      m_cfg.helix_host, m_cfg.helix_port,
      "/helix/chat/messages", body.dump(), headers);

  LOG_INFO("[Helix chat/send] resp: {}", resp);
}*/

void sbot::tw::Chat::reconnect() {
  if (m_wss) {
    beast::error_code ec;
    m_wss->close(ws::close_code::normal, ec);
  }
  m_wss.reset();
  connect();
}

auto sbot::tw::Chat::shutdown() -> asio::awaitable<void> {
  LOG_INFO("Shutting down Chat connection");
  if (m_wss) {
    try {
      // Send close frame and wait for server to respond
      co_await m_wss->async_close(ws::close_code::normal, asio::use_awaitable);
      LOG_INFO("WebSocket closed gracefully");
    } catch (const beast::system_error &err) {
      if (err.code() != ws::error::closed) {
        LOG_WARN("WebSocket close warning: {}", err.what());
      }
    } catch (const std::exception &err) {
      LOG_ERROR("Error during WebSocket shutdown: {}", err.what());
    }
  }
  m_wss.reset();
}
