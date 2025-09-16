#ifndef SBOT_CORE_CONNECTION_MANAGER_HPP
#define SBOT_CORE_CONNECTION_MANAGER_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace sbot::core {

class ConnectionManager {
public:
  explicit ConnectionManager(std::size_t thread_count = 3);
  ~ConnectionManager();
  ConnectionManager(const ConnectionManager &)                     = delete;
  auto operator=(const ConnectionManager &) -> ConnectionManager & = delete;
  ConnectionManager(ConnectionManager &&)                          = delete;
  auto operator=(ConnectionManager &&) -> ConnectionManager &      = delete;

  [[nodiscard]] auto getIoContext() const
      -> std::shared_ptr<boost::asio::io_context>;
  [[nodiscard]] auto getSslContext() const
      -> std::shared_ptr<boost::asio::ssl::context>;
  [[nodiscard]] auto getResolver() const
      -> std::shared_ptr<boost::asio::ip::tcp::resolver>;

  [[nodiscard]] auto makeSslStreamAsync(std::string host, std::string port)
      -> boost::asio::awaitable<std::unique_ptr<
          boost::beast::ssl_stream<boost::asio::ip::tcp::socket>>>;

private:
  std::shared_ptr<boost::asio::io_context> m_io_context;
  std::shared_ptr<boost::asio::ssl::context> m_ssl_context;
  std::shared_ptr<boost::asio::ip::tcp::resolver> m_resolver;
  // Thread pool management
  std::unique_ptr<
      boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>
      m_work_guard;
  std::vector<std::jthread> m_thread_pool;
  std::size_t m_thread_count;
};

} // namespace sbot::core

#endif
