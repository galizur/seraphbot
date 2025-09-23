#ifndef SBOT_OBS_OBSSERVICE_HPP
#define SBOT_OBS_OBSSERVICE_HPP

#include "seraphbot/core/logging.hpp"
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/websocket/stream_base.hpp>
#include <chrono>
#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <utility>

namespace sbot::obs {

class OBSService {
public:
  OBSService(std::string password = "") : m_obs_password(std::move(password)) {}

  auto connectToOBS() -> boost::asio::awaitable<bool> {
    try {
      auto executor = co_await boost::asio::this_coro::executor;
      boost::asio::ip::tcp::resolver resolver(executor);
      auto results = co_await resolver.async_resolve(
          m_obs_host, m_obs_password, boost::asio::use_awaitable);
      m_ws = std::make_shared<
          boost::beast::websocket::stream<boost::asio::ip::tcp::socket>>(
          executor);
      auto ep = co_await boost::asio::async_connect(m_ws->next_layer(), results,
                                                    boost::asio::use_awaitable);
      m_ws->set_option(boost::beast::websocket::stream_base::decorator(
          [](boost::beast::websocket::request_type &req) {
            req.set(boost::beast::http::field::user_agent, "SeraphBot/1.0");
            req.set(boost::beast::http::field::sec_websocket_protocol,
                    "obswebsocket.json");
          }));
      LOG_INFO("Attempting WebSocket handshake to {}:{}", m_obs_host,
               m_obs_port);
      co_await m_ws->async_handshake(m_obs_host, "",
                                     boost::asio::use_awaitable);
      LOG_INFO("WebSocket handshake successfull");
      if (!m_obs_password.empty()) {
        co_await authenticate();
      }
      m_connected = true;
      LOG_INFO("Connected to OBS WebSocket");
      co_return true;
    } catch (const std::exception &err) {
      LOG_ERROR("Failed to connect to OBS: {}", err.what());
      m_connected = false;
      co_return false;
    }
  }

  auto sendOBSCommand(const nlohmann::json &command)
      -> boost::asio::awaitable<nlohmann::json> {
    if (!m_connected) {
      co_await connectToOBS();
    }
    co_await sendMessage(command);
    co_return co_await receiveMessage();
  }

  auto getSourceId(const std::string &scene_name,
                   const std::string &source_name)
      -> boost::asio::awaitable<int> {
    nlohmann::json request = {
        {"op", 6                        },
        {"d",
         {{"requestType", "GetSceneItemList"},
          {"requestData", {{"sceneName", scene_name}}},
          {"requestId", m_request_id++}}}
    };

    auto response = co_await sendOBSCommand(request);

    if (response.contains("d") && response["d"].contains("responseData") &&
        response["d"]["responseData"].contains("sceneItems")) {
      for (const auto &item : response["d"]["responseData"]["sceneItems"]) {
        if (item.contains("sourceName") && item["sourceName"] == source_name) {
          co_return item.value("sceneItemId", -1);
        }
      }
    }
    LOG_WARN("Source '{}' not found in scene '{}'", source_name, scene_name);
    co_return -1;
  }

  auto getMediaDuration(const std::string &source_name)
      -> boost::asio::awaitable<std::chrono::milliseconds> {
    nlohmann::json request = {
        {"op", 6                        },
        {"d",
         {{"requestType", "GetMediaInputStatus"},
          {"requestData", {{"inputName", source_name}}},
          {"requestId", m_request_id++}}}
    };

    auto response = co_await sendOBSCommand(request);

    if (response.contains("d") && response["d"].contains("responseData")) {
      auto data = response["d"]["responseData"];
      if (data.contains("mediaDuration")) {
        std::int64_t duration_ms = data["mediaDuration"];
        co_return std::chrono::milliseconds(duration_ms);
      }
    }
    co_return std::chrono::milliseconds(5000);
  }

  auto waitForVideoEnd() -> boost::asio::awaitable<void> {
    // Better approach would be to monitor media state changes
    co_await boost::asio::steady_timer(
        co_await boost::asio::this_coro::executor, std::chrono::seconds(1))
        .async_wait(boost::asio::use_awaitable);
  }

  auto playVideo(const std::string &video_path, const std::string &scene_name,
                 const std::string &source_name)
      -> boost::asio::awaitable<void> {

    try {
      LOG_INFO("Playing video: {} in scene: {}", video_path, scene_name);
      if (!m_connected) {
        co_await connectToOBS();
      }

      nlohmann::json set_media = {
          {"op", 6                        },
          {"d",
           {{"requestType", "SetInputSettings"},
            {"requestData",
             {{"inputName", source_name},
              {"inputSettings",
               {{"local_file", video_path}, {"restart_on_activate", true}}}}},
            {"requestId", m_request_id++}}}
      };

      co_await sendOBSCommand(set_media);

      int source_id = co_await getSourceId(scene_name, source_name);
      if (source_id == -1) {
        LOG_ERROR("Could not find source {} in scene {}", source_name,
                  scene_name);
        co_return;
      }

      nlohmann::json show_source = {
          {"op", 6                        },
          {"d",
           {{"requestType", "SetSceneItemEnabled"},
            {"requestData",
             {{"sceneName", scene_name},
              {"sceneItemId", source_name},
              {"sceneItemEnabled", true}}},
            {"requestId", m_request_id++}}}
      };
      co_await sendOBSCommand(show_source);

      auto duration = co_await getMediaDuration(source_name);
      LOG_INFO("Video duration: {}ms", duration.count());

      co_await boost::asio::steady_timer(
          co_await boost::asio::this_coro::executor, duration)
          .async_wait(boost::asio::use_awaitable);

      nlohmann::json hide_source = {
          {"op", 6                        },
          {"d",
           {{"requestType", "SetSceneItemEneabled"},
            {"requestData",
             {{"sceneName", scene_name},
              {"sceneItemId", source_id},
              {"sceneItemEnabled", false}}},
            {"requestId", m_request_id++}}}
      };
      co_await sendOBSCommand(hide_source);

      LOG_INFO("Video playback completed");
    } catch (const std::exception &err) {
      LOG_ERROR("Error playing video: {}", err.what());
    }
  }

private:
  std::string m_obs_host{"192.168.1.2"};
  std::string m_obs_port{"4455"};
  std::string m_obs_password;
  std::shared_ptr<boost::beast::websocket::stream<boost::asio::ip::tcp::socket>>
      m_ws;
  bool m_connected{false};
  std::uint64_t m_request_id{1};

  auto authenticate() -> boost::asio::awaitable<void> {
    nlohmann::json auth_request = {
        {"op", 6                                                            },
        {"d",
         {{"requestType", "GetAuthRequired"}, {"requestId", m_request_id++}}}
    };
    co_await sendMessage(auth_request);
    auto response = co_await receiveMessage();
    if (response.contains("d") && response["d"].contains("authRequired") &&
        response["d"]["authRequired"] == true) {
      std::string challenge     = response["d"]["challenge"];
      std::string salt          = response["d"]["salt"];
      std::string auth_response = generateAuthResponse(challenge, salt);
      nlohmann::json identify   = {
          {"op", 1                                                     },
          {"d",  {{"rpcVersion", 1}, {"authentication", auth_response}}}
      };
      co_await sendMessage(identify);
    }
  }

  auto generateAuthResponse(const std::string &challenge,
                            const std::string &salt) -> std::string {
    return "";
  }

  auto sendMessage(const nlohmann::json &message)
      -> boost::asio::awaitable<void> {
    std::string msg_str = message.dump();
    co_await m_ws->async_write(boost::asio::buffer(msg_str),
                               boost::asio::use_awaitable);
  }

  auto receiveMessage() -> boost::asio::awaitable<nlohmann::json> {
    boost::beast::flat_buffer buffer;
    co_await m_ws->async_read(buffer, boost::asio::use_awaitable);
    std::string response = boost::beast::buffers_to_string(buffer.data());
    co_return nlohmann::json::parse(response);
  }
};
} // namespace sbot::obs

#endif
