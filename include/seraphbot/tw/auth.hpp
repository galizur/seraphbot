#ifndef SBOT_TW_AUTH_HPP
#define SBOT_TW_AUTH_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/beast/http/message_fwd.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace sbot::core {
class ConnectionManager;
} // namespace sbot::core

namespace sbot::tw {

// TODO: To be moved to a different file
struct TwitchUserInfo {
  std::string id;
  std::string login;
  std::string display_name;
  std::string broadcaster_type;
  std::string description;
  std::chrono::system_clock::time_point created_at;
  std::string profile_image_url;
  std::uint64_t view_count;
  std::optional<std::string> offline_image_url;
  std::optional<std::string> email;
};

class Auth {
public:
  using on_token_fn = std::function<void(const std::string &access_token)>;

  Auth(std::shared_ptr<core::ConnectionManager> conn_manager,
       std::string url_base);

  auto loginAsync() -> boost::asio::awaitable<void>;
  [[nodiscard]] auto isLoggedIn() const -> bool;
  [[nodiscard]] auto accessToken() const -> std::string;
  [[nodiscard]] auto clientId() const -> std::string;
  auto fetchClientIdAsync() -> boost::asio::awaitable<std::string>;
  auto fetchUserInfoAsync() -> boost::asio::awaitable<TwitchUserInfo>;
  // [[nodiscard]] auto fullToken() const -> const nlohmann::json &;

private:
  static constexpr std::string_view c_twitch_port{"443"};
  static constexpr std::string_view c_user_agent{"seraphbot-auth/0.1"};
  static constexpr unsigned int c_http_version{11};
  static constexpr std::size_t c_state_length{16};

  std::shared_ptr<core::ConnectionManager> m_conn_manager;
  std::string m_url_base;
  std::string m_access_token;
  std::string m_client_id;
  std::string m_state;
  std::string m_refresh_token;
  TwitchUserInfo m_user_info; // TODO: To be moved to a different file
  // nlohmann::json m_token;

  auto getAuthUrlAsync() -> boost::asio::awaitable<std::string>;
  // auto pollToken() -> nlohmann::json;
  [[nodiscard]] static auto generateState() -> std::string;
  auto requestTokenAsync() -> boost::asio::awaitable<void>;
  auto storeToken(const std::string &token) -> void;
  auto storeRefreshToken(const std::string &refresh_token) -> void;
  auto getHttpAsync(const std::string &url)
      -> boost::asio::awaitable<std::string>;
  auto parseTwitchUser(const nlohmann::json &user_json) -> TwitchUserInfo;
};

} // namespace sbot::tw

#endif
