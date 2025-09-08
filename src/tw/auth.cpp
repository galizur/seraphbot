#include "seraphbot/tw/auth.hpp"

#include "seraphbot/core/connection_manager.hpp"
#include "seraphbot/core/logging.hpp"

#include <algorithm>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/process.hpp>
#include <boost/process/v1/child.hpp>
#include <boost/process/v1/detail/child_decl.hpp>
#include <boost/process/v2/bind_launcher.hpp>
#include <boost/process/v2/environment.hpp>
#include <boost/process/v2/execute.hpp>
#include <boost/process/v2/pid.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <boost/url.hpp>
#include <boost/url/encode.hpp>
#include <boost/url/encoding_opts.hpp>
#include <boost/url/grammar/string_token.hpp>
#include <boost/url/pct_string_view.hpp>
#include <boost/url/url.hpp>
#include <boost/url/urls.hpp>
#include <chrono>
#include <ctime>
#include <exception>
#include <iomanip>
#include <memory>
#include <nlohmann/json.hpp>
#include <openssl/tls1.h>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace ssl   = asio::ssl;
using tcp       = asio::ip::tcp;
using json      = nlohmann::json;
} // namespace

#ifdef _LIBCPP_VERSION
#define SBOT_LIBCXX 1
// NOLINTNEXTLINE
#warning "libc++ detected - using chrono workarounds."
#else
#define SBOT_LIBCXX 0
#endif

auto parseTime(const std::string &s) -> std::chrono::system_clock::time_point {
  std::istringstream ss{s};
#if !SBOT_LIBCXX
  std::chrono::system_clock::time_point tp;
  ss >> std::chrono::parse("%Y-%m-%dT%H:%M:%S", tp);

  if (ss.fail()) {
    LOG_ERROR("Invalid time format: {}", s);
    throw std::runtime_error("Invalid time format: " + s);
  }
  return tp;
#else
  LOG_INFO("libc++ detected - using chrono workarounds.");
  std::tm tm{};
  ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  if (ss.fail()) {
    LOG_ERROR("Invalid time format: {}", s);
    throw std::runtime_error("Invalid time format: " + s);
  }
#ifdef _WIN32
  auto time_c = _mkgmtime(&tm);
#else
  auto time_c = timegm(&tm);
#endif

  return std::chrono::system_clock::from_time_t(time_c);
#endif
}

sbot::tw::Auth::Auth(std::shared_ptr<core::ConnectionManager> conn_manager,
                     std::string url_base)
    : m_conn_manager{std::move(conn_manager)}, m_url_base{std::move(url_base)} {
  LOG_CONTEXT("Twitch Auth");
  LOG_TRACE("Auth starting");
}

auto sbot::tw::Auth::isLoggedIn() const -> bool {
  return !m_access_token.empty();
}

auto sbot::tw::Auth::accessToken() const -> std::string {
  return m_access_token;
}

auto sbot::tw::Auth::clientId() const -> std::string { return m_client_id; }

auto sbot::tw::Auth::generateState() -> std::string {
  static constexpr std::string_view c_alphanum{"0123456789"
                                               "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                               "abcdefghijklmnopqrstuvwxyz"};
  static thread_local std::mt19937 gen{std::random_device{}()};
  std::uniform_int_distribution<std::size_t> dis(0, c_alphanum.size() - 1);

  std::string state(c_state_length, '\0');

  std::ranges::generate(state, [&]() { return c_alphanum[dis(gen)]; });

  return state;
}

auto sbot::tw::Auth::getHttpAsync(const std::string &url)
    -> boost::asio::awaitable<std::string> {
  LOG_CONTEXT("Twitch Auth");
  try {
    auto stream = co_await m_conn_manager->makeSslStreamAsync(
        m_url_base, std::string(c_twitch_port));

    co_await stream->async_handshake(ssl::stream_base::client,
                                     asio::use_awaitable);

    http::request<http::string_body> req{http::verb::get, url, c_http_version};
    req.set(http::field::host, m_url_base);
    req.set(http::field::user_agent, c_user_agent);
    req.set(http::field::connection, "close");

    co_await http::async_write(*stream, req, asio::use_awaitable);

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    co_await http::async_read(*stream, buffer, res, asio::use_awaitable);

    // Extract response body before shutdown
    std::string response_body = res.body();

    // Graceful SSL shutdown
    try {
      co_await stream->async_shutdown(asio::use_awaitable);
    } catch (const beast::system_error &err) {
      if (err.code() != asio::error::eof &&
          err.code() != ssl::error::stream_truncated) {
        LOG_DEBUG("SSL shutdown info: {}", err.what());
      }
    }

    // Check HTTP status
    if (res.result() != http::status::ok &&
        res.result() != http::status::found) {
      LOG_ERROR("HTTP request failed with status: {}", res.result_int());
      throw std::runtime_error("HTTP request failed with status: " +
                               std::to_string(res.result_int()));
    }

    // For redirects, return the Location header
    if (res.result() == http::status::found) {
      auto loc_hdr = res.base()["Location"];
      if (!loc_hdr.empty()) {
        co_return std::string{loc_hdr.data(), loc_hdr.size()};
      }
      LOG_ERROR("Redirect response missing Location header");
      throw std::runtime_error("Redirect response missing Location header");
    }

    co_return response_body;
  } catch (const beast::system_error &err) {
    LOG_ERROR("HTTP request error: {}", err.what());
    throw std::runtime_error("HTTP request failed: " + std::string(err.what()));
  } catch (const std::exception &err) {
    LOG_ERROR("HTTP request unexpected error: {}", err.what());
    throw;
  }
}

auto sbot::tw::Auth::fetchClientIdAsync() -> asio::awaitable<std::string> {
  std::string url           = "/client_id";
  std::string response_body = co_await getHttpAsync(url);

  auto json_res = json::parse(response_body);
  m_client_id   = json_res.at("client_id").get<std::string>();

  LOG_INFO("Client ID retrieved: {}", m_client_id);
  co_return m_client_id;
}

auto sbot::tw::Auth::getAuthUrlAsync() -> asio::awaitable<std::string> {
  m_state = generateState();

  // Build GET /auth_url?state=...
  std::string url          = "/auth_url?state=" + m_state;
  std::string redirect_url = co_await getHttpAsync(url);

  LOG_INFO("Redirect URL = {}", redirect_url);
  co_return redirect_url;
}

auto sbot::tw::Auth::requestTokenAsync() -> asio::awaitable<void> {
  std::string url           = "/token?state=" + m_state;
  std::string response_body = co_await getHttpAsync(url);

  auto json_res = json::parse(response_body);
  LOG_INFO("Token response: {}", json_res.dump());

  std::string token = json_res.at("access_token").get<std::string>();
  storeToken(token);

  std::string refresh_token = json_res.at("refresh_token").get<std::string>();
  storeRefreshToken(refresh_token);
}

auto sbot::tw::Auth::fetchUserInfoAsync() -> asio::awaitable<TwitchUserInfo> {
  std::string url           = "/user_info?state=" + m_state;
  std::string response_body = co_await getHttpAsync(url);

  auto json_res = json::parse(response_body);
  co_return parseTwitchUser(json_res);
}

auto sbot::tw::Auth::storeToken(const std::string &token) -> void {
  m_access_token = token;
}

auto sbot::tw::Auth::storeRefreshToken(const std::string &refresh_token)
    -> void {
  m_refresh_token = refresh_token;
}

auto sbot::tw::Auth::loginAsync() -> asio::awaitable<void> {
  LOG_CONTEXT("Twitch Auth");
  LOG_INFO("Starting login flow.");
  try {
    co_await fetchClientIdAsync();
  } catch (const std::exception &e) {
    LOG_ERROR("Failed to fetch client ID: {}", e.what());
    throw std::runtime_error("Failed to fetch client ID: {}" +
                             std::string(e.what()));
  }

  std::string url = co_await getAuthUrlAsync();

#if defined(_WIN32)
  std::string cmd = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
  std::string cmd = "open \"" + url + "\"";
#else
  std::string cmd = "xdg-open \"" + url + "\"";
#endif
  boost::process::v1::child child(cmd.c_str());
  child.detach();

  LOG_INFO("Waiting for authorization...");

  // Poll for token
  int attempts           = 1;
  const int max_attempts = 10;
  while (attempts <= max_attempts) {
    auto delay = std::chrono::seconds(attempts);
    LOG_INFO("Waiting {} seconds before attempt {}", delay.count(),
             attempts);

    co_await asio::steady_timer{*m_conn_manager->getIoContext(), delay}
        .async_wait(asio::use_awaitable);
    try {
      co_await requestTokenAsync();
      LOG_INFO("Authorization successful!");
      co_return;
    } catch (const std::exception &e) {
      LOG_WARN("Poll attempt {} failed: {}", attempts, e.what());
    }
    attempts++;
  }
  LOG_CRITICAL("Authorization timeout after {} attempts", max_attempts);
  throw std::runtime_error("Authorization timeout");
}

auto sbot::tw::Auth::parseTwitchUser(const json &user_json) -> TwitchUserInfo {
  LOG_CONTEXT("Twitch Auth");
  // Twitch API returns a "data" array. Usually with one user only
  if (!user_json.contains("data") || !user_json["data"].is_array() ||
      user_json["data"].empty()) {
    LOG_ERROR("No user data found in Twitch response");
    throw std::runtime_error("No user data found in Twitch response");
  }

  const auto &user = user_json["data"][0];

  auto created_str = user.value("created_at", "");
  auto created_at  = std::chrono::system_clock::time_point{};
  if (!created_str.empty()) {
    created_at = parseTime(created_str);
  }

  return TwitchUserInfo{
      .id                = user.value("id", ""),
      .login             = user.value("login", ""),
      .display_name      = user.value("display_name", ""),
      .broadcaster_type  = user.value("broadcaster_type", ""),
      .description       = user.value("description", ""),
      .created_at        = created_at,
      .profile_image_url = user.value("profile_image_url", ""),
      .view_count        = user.value("view_count", 0U),
      .offline_image_url = user.value("offline_image_url", ""),
      .email             = user.value("email", "")};
}
