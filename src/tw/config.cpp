#include "seraphbot/tw/config.hpp"

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/dynamic_body_fwd.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/impl/read.hpp>
#include <boost/beast/http/impl/write.hpp>
#include <boost/beast/http/message_fwd.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <boost/beast/http/verb.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "seraphbot/core/connection_manager.hpp"
#include "seraphbot/core/logging.hpp"

namespace {
namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace ssl   = asio::ssl;
using tcp       = asio::ip::tcp;
} // namespace

auto sbot::tw::httpsPostAsync(
    const std::shared_ptr<sbot::core::ConnectionManager> conn_manager,
    std::string host, std::string port, std::string target, std::string body,
    std::vector<std::pair<std::string, std::string>> headers)
    -> asio::awaitable<std::string> {
  auto stream = co_await conn_manager->makeSslStreamAsync(host, port);

  co_await stream->async_handshake(ssl::stream_base::client,
                                   asio::use_awaitable);

  constexpr int c_http_version{11};
  // Set up HTTP request
  http::request<http::string_body> req{http::verb::post, target,
                                       c_http_version};
  req.set(http::field::host, host);
  req.set(http::field::user_agent, "seraphbot-eventsub/0.1");
  req.set(http::field::content_type, "application/json");
  req.set(http::field::connection, "close");

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

auto sbot::tw::stripOauthPrefix(std::string token) -> std::string {
  constexpr int c_oauth_str_len{6};
  if (token.starts_with("oauth:")) {
    return token.substr(c_oauth_str_len);
  }
  return token;
}

auto sbot::tw::httpsGetAsync(
    const std::shared_ptr<sbot::core::ConnectionManager> conn_manager,
    std::string host, std::string port, std::string target,
    std::vector<std::pair<std::string, std::string>> headers)
    -> asio::awaitable<std::string> {
  auto stream = co_await conn_manager->makeSslStreamAsync(host, port);

  co_await stream->async_handshake(ssl::stream_base::client,
                                   asio::use_awaitable);

  constexpr int c_http_version{11};
  // Set up HTTP request
  http::request<http::empty_body> req{http::verb::get, target, c_http_version};
  req.set(http::field::host, host);
  req.set(http::field::user_agent, "seraphbot-eventsub/0.1");
  req.set(http::field::connection, "close");

  for (const auto &head : headers) {
    req.set(head.first, head.second);
  }

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
