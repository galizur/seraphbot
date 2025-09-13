#include "seraphbot/tw/eventsub.hpp"

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/impl/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
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
#include <boost/beast/core/role.hpp>
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
#include <string_view>
#include <utility>
#include <vector>

#include "seraphbot/core/connection_manager.hpp"
#include "seraphbot/core/logging.hpp"
#include "seraphbot/tw/config.hpp"

namespace {
namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace ws    = beast::websocket;
namespace ssl   = asio::ssl;
using tcp       = asio::ip::tcp;
using json      = nlohmann::json;
} // namespace

sbot::tw::EventSub::EventSub(
    std::shared_ptr<core::ConnectionManager> conn_manager, ClientConfig cfg)
    : m_cfg{std::move(cfg)}, m_conn_manager{std::move(conn_manager)} {
  LOG_CONTEXT("Twitch EventSub");
  LOG_TRACE("Initializing");
}

sbot::tw::EventSub::~EventSub() { LOG_TRACE("Shutting down"); }

auto sbot::tw::EventSub::start(on_notify_fn callback) -> asio::awaitable<void> {
  m_callback = std::move(callback);
  // co_await connect();
  co_return;
}

auto sbot::tw::EventSub::connect() -> asio::awaitable<void> {
  try {
    // Create async stream
    m_stream =
        co_await m_conn_manager->makeSslStreamAsync(m_cfg.host, m_cfg.port);

    // // Async SSL handshake
    co_await m_stream->async_handshake(ssl::stream_base::client,
                                       asio::use_awaitable);

    // // // Now upgrade to websocket over TLS
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
    json jsn = json::parse(welcome);
    std::string session_id;
    try {
      session_id = jsn.at("payload").at("session").at("id").get<std::string>();
    } catch (...) {
      LOG_ERROR("Failed to extract session.id from welcome message");
      throw std::runtime_error(
          "Failed to extract session.id from welcome message");
    }
    m_session_id = session_id;

    LOG_INFO("session_id = {}", session_id);
  } catch (const std::exception &ex) {
    LOG_ERROR("Connection error: {}", ex.what());
    throw;
  }
}

auto sbot::tw::EventSub::subscribe(std::string type, std::string version)
    -> asio::awaitable<void> {
  try {
    std::string token = stripOauthPrefix(m_cfg.access_token);

    std::vector<std::pair<std::string, std::string>> headers = {
        {"Client-Id",     m_cfg.client_id   },
        {"Authorization", "Bearer " + token },
        {"Content-Type",  "application/json"}
    };

    json condition = {
        {"broadcaster_user_id", m_cfg.broadcaster_id},
        {"user_id",             m_cfg.broadcaster_id}
    };

    json transport = {
        {"method",     "websocket" },
        {"session_id", m_session_id}
    };

    // Subscript to channel.chat.notification
    json subscription_request = {
        {"type",      type     },
        {"version",   version  },
        {"condition", condition},
        {"transport", transport}
    };
    LOG_INFO("Subscription dump: {}", subscription_request.dump());
    try {
      std::string resp = co_await httpsPostAsync(
          m_conn_manager, m_cfg.helix_host, m_cfg.helix_port,
          "/helix/eventsub/subscriptions", subscription_request.dump(),
          headers);

      LOG_INFO("Subscription response: {}", resp);

      // Parse the response to check if subscription was successful
      json sub_response = json::parse(resp);
      if (sub_response.contains("error")) {
        LOG_ERROR("Subscription failed: {}", sub_response.dump());
        throw std::runtime_error("EventSub subscription failed");
      }
    } catch (const std::exception &e) {
      LOG_ERROR("Exception during subscription: {}", e.what());
      throw; // Re-throw to prevent silent failure
    }

    constexpr int c_wait{300};
    // Optionally wait a bit for subscription to activate
    co_await asio::steady_timer{*m_conn_manager->getIoContext(),
                                std::chrono::milliseconds(c_wait)}
        .async_wait(asio::use_awaitable);
  } catch (const std::exception &ex) {
    LOG_ERROR("Connection error: {}", ex.what());
    throw;
  }
}

auto sbot::tw::EventSub::doRead() -> asio::awaitable<void> {
  LOG_DEBUG("Entering doRead");
  if (!m_wss) {
    LOG_ERROR("WebSocket not connected");
    co_return;
  }
  try {
    while (m_wss && m_wss->is_open()) {
      co_await m_wss->async_read(m_buffer, asio::use_awaitable);

      std::string msg = beast::buffers_to_string(m_buffer.data());
      m_buffer.consume(m_buffer.size());

      LOG_DEBUG("{}", msg);

      if (m_callback) {
        m_callback(msg);
      }
    }
  } catch (const beast::system_error &ec) {
    if (ec.code() == ws::error::closed) {
      LOG_WARN("WebSocket closed by server");
    } else if (ec.code() == asio::error::operation_aborted) {
      LOG_INFO("Read operation cancelled (normal during shutdown)");
    } else {
      LOG_ERROR("Read error: {}", ec.what());
    }
  } catch (const std::exception &ex) {
    LOG_ERROR("Unexpected error in doRead: {}", ex.what());
  }
}

void sbot::tw::EventSub::reconnect() {
  if (m_wss) {
    beast::error_code err;
    m_wss->close(ws::close_code::normal, err);
  }
  m_wss.reset();
  asio::co_spawn(*m_conn_manager->getIoContext(), connect(), asio::detached);
}

auto sbot::tw::EventSub::shutdown() -> asio::awaitable<void> {
  LOG_TRACE("Shutting down Chat connection");
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
