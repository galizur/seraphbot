#include "seraphbot/core/connection_manager.hpp"

#include "seraphbot/core/logging.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <cstddef>
#include <memory>

namespace {
namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace ssl   = asio::ssl;
using tcp       = asio::ip::tcp;
} // namespace

sbot::core::ConnectionManager::ConnectionManager(std::size_t thread_count)
    : m_io_context{std::make_shared<asio::io_context>()},
      m_ssl_context{std::make_shared<ssl::context>(ssl::context::tls_client)},
      m_resolver{std::make_shared<tcp::resolver>(*m_io_context)},
      m_thread_count{thread_count} {
  LOG_CONTEXT("ConnectionManager");
  m_ssl_context->set_options(ssl::context::default_workarounds |
                             ssl::context::no_sslv2 | ssl::context::no_sslv3 |
                             ssl::context::no_tlsv1 | ssl::context::no_tlsv1_1);
  m_ssl_context->set_verify_mode(ssl::verify_peer);
  m_ssl_context->set_default_verify_paths();

  // Create work guard to keep io_context alive
  m_work_guard = std::make_unique<
      asio::executor_work_guard<asio::io_context::executor_type>>(
      m_io_context->get_executor());

  // Start thread pool (background threads only)
  m_thread_pool.reserve(m_thread_count);
  for (std::size_t i = 0; i < m_thread_count; ++i) {
    m_thread_pool.emplace_back([this, i] {
      LOG_INFO("IO thread {} starting", i);
      try {
        m_io_context->run();
        LOG_INFO("Io thread {} finished", i);
      } catch (const std::exception &err) {
        LOG_ERROR("IO thread {} error: {}", i, err.what());
      }
    });
  }
  LOG_INFO("ConnectionManager started with {} threads", m_thread_count);
}

sbot::core::ConnectionManager::~ConnectionManager() {
  LOG_CONTEXT("ConnectionManager");
  LOG_INFO("Shutting down ConnectionManager");

  // Stop accepting new work
  m_work_guard.reset();

  // Wait for all operations to complete
  for (auto &thread : m_thread_pool) {
    if (thread.joinable()) {
      thread.join();
    }
  }
  LOG_INFO("ConnectionManager shutdown complete");
}

auto sbot::core::ConnectionManager::getIoContext() const
    -> std::shared_ptr<boost::asio::io_context> {
  return m_io_context;
}

auto sbot::core::ConnectionManager::getSslContext() const
    -> std::shared_ptr<boost::asio::ssl::context> {
  return m_ssl_context;
}

auto sbot::core::ConnectionManager::getResolver() const
    -> std::shared_ptr<boost::asio::ip::tcp::resolver> {
  return m_resolver;
}

auto sbot::core::ConnectionManager::makeSslStreamAsync(std::string const &host,
                                                       std::string const &port)
    -> asio::awaitable<std::unique_ptr<
        boost::beast::ssl_stream<boost::asio::ip::tcp::socket>>> {

  auto stream = std::make_unique<beast::ssl_stream<tcp::socket>>(
      *m_io_context, *m_ssl_context);
  // Async resolve and connect
  auto const results =
      co_await m_resolver->async_resolve(host, port, asio::use_awaitable);
  // Connect socket
  co_await asio::async_connect(stream->next_layer(), results,
                               asio::use_awaitable);

  // SNI
  if (!SSL_set_tlsext_host_name(stream->native_handle(), host.c_str())) {
    LOG_ERROR("Failed to set SNI");
    throw std::runtime_error("Failed to set SNI");
  }

  co_return stream;
}
