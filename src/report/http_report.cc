#include "report/http_report.h"

#include <cstring>
#include <string>

#include "boost/asio/connect.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/write.hpp"
#include "boost/beast/version.hpp"
#include "spdlog/spdlog.h"

#include "third_party/mbedtls/include/mbedtls/ctr_drbg.h"
#include "third_party/mbedtls/include/mbedtls/entropy.h"
#include "third_party/mbedtls/include/mbedtls/error.h"
#include "third_party/mbedtls/include/mbedtls/net_sockets.h"
#include "third_party/mbedtls/include/mbedtls/ssl.h"

namespace beast = boost::beast;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace buried {

namespace {

const char kUserAgent[] = BOOST_BEAST_VERSION_STRING;

static boost::asio::io_context ioc;

int TlsBioSend(void* ctx, const unsigned char* buf, size_t len) {
  auto* socket = static_cast<tcp::socket*>(ctx);
  boost::system::error_code ec;
  size_t sent = boost::asio::write(*socket, boost::asio::buffer(buf, len), ec);
  if (ec) {
    return MBEDTLS_ERR_NET_SEND_FAILED;
  }
  return static_cast<int>(sent);
}

int TlsBioRecv(void* ctx, unsigned char* buf, size_t len) {
  auto* socket = static_cast<tcp::socket*>(ctx);
  boost::system::error_code ec;
  size_t recv = socket->read_some(boost::asio::buffer(buf, len), ec);
  if (ec) {
    if (ec == boost::asio::error::eof) {
      return 0;
    }
    return MBEDTLS_ERR_NET_RECV_FAILED;
  }
  return static_cast<int>(recv);
}

std::string TlsErrorString(int ret) {
  char buf[128] = {0};
  mbedtls_strerror(ret, buf, sizeof(buf));
  return buf;
}

struct TlsContextGuard {
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;

  TlsContextGuard() {
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
  }

  ~TlsContextGuard() {
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
  }
};

}  // namespace

HttpReporter::HttpReporter(std::shared_ptr<spdlog::logger> logger, bool secure)
    : logger_(logger), secure_(secure) {}

std::string HttpReporter::BuildRequest_() {
  std::string request = "POST " + topic_ + " HTTP/1.1\r\n";
  request += "Host: " + host_ + "\r\n";
  request += "User-Agent: " + std::string(kUserAgent) + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + std::to_string(body_.size()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += body_;
  return request;
}

bool HttpReporter::Report() {
  if (secure_) {
    return ReportTls_();
  }
  return ReportPlain_();
}

bool HttpReporter::ReportPlain_() {
  try {
    tcp::resolver resolver(ioc);
    tcp::resolver::query query(host_, port_);
    auto const results = resolver.resolve(query);

    tcp::socket socket(ioc);
    boost::asio::connect(socket, results);

    std::string request = BuildRequest_();
    boost::asio::write(socket,
                       boost::asio::buffer(request.data(), request.size()));

    std::string response;
    char buf[1024];
    boost::system::error_code ec;
    while (response.find("\r\n\r\n") == std::string::npos &&
           response.size() < 65536) {
      size_t n = socket.read_some(boost::asio::buffer(buf, sizeof(buf)), ec);
      if (ec) {
        break;
      }
      response.append(buf, n);
    }
    socket.close();
    return response.find(" 200 ") != std::string::npos;
  } catch (std::exception const& e) {
    SPDLOG_LOGGER_ERROR(logger_, "report error " + std::string(e.what()));
    return false;
  }
}

bool HttpReporter::ReportTls_() {
  try {
    tcp::resolver resolver(ioc);
    tcp::resolver::query query(host_, port_);
    auto const results = resolver.resolve(query);

    tcp::socket socket(ioc);
    boost::asio::connect(socket, results);

    TlsContextGuard tls;
    int ret = mbedtls_ctr_drbg_seed(
        &tls.ctr_drbg, mbedtls_entropy_func, &tls.entropy,
        reinterpret_cast<const unsigned char*>("buried"), 6);
    if (ret != 0) {
      SPDLOG_LOGGER_ERROR(logger_, "report tls seed error: {}",
                          TlsErrorString(ret));
      return false;
    }
    ret = mbedtls_ssl_config_defaults(&tls.conf, MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
      SPDLOG_LOGGER_ERROR(logger_, "report tls config error: {}",
                          TlsErrorString(ret));
      return false;
    }
    mbedtls_ssl_conf_authmode(&tls.conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&tls.conf, mbedtls_ctr_drbg_random, &tls.ctr_drbg);
    ret = mbedtls_ssl_setup(&tls.ssl, &tls.conf);
    if (ret != 0) {
      SPDLOG_LOGGER_ERROR(logger_, "report tls setup error: {}",
                          TlsErrorString(ret));
      return false;
    }
    mbedtls_ssl_set_hostname(&tls.ssl, host_.c_str());
    mbedtls_ssl_set_bio(&tls.ssl, &socket, TlsBioSend, TlsBioRecv, nullptr);

    ret = mbedtls_ssl_handshake(&tls.ssl);
    if (ret != 0) {
      SPDLOG_LOGGER_ERROR(logger_, "report tls handshake error: {}",
                          TlsErrorString(ret));
      return false;
    }

    std::string request = BuildRequest_();
    ret = mbedtls_ssl_write(
        &tls.ssl, reinterpret_cast<const unsigned char*>(request.data()),
        request.size());
    if (ret <= 0) {
      SPDLOG_LOGGER_ERROR(logger_, "report tls write error: {}",
                          TlsErrorString(ret));
      return false;
    }

    std::string response;
    char buf[1024];
    while (response.find("\r\n\r\n") == std::string::npos &&
           response.size() < 65536) {
      ret = mbedtls_ssl_read(&tls.ssl,
                             reinterpret_cast<unsigned char*>(buf),
                             sizeof(buf));
      if (ret <= 0) {
        break;
      }
      response.append(buf, static_cast<size_t>(ret));
    }

    mbedtls_ssl_close_notify(&tls.ssl);
    socket.close();
    return response.find(" 200 ") != std::string::npos;
  } catch (std::exception const& e) {
    SPDLOG_LOGGER_ERROR(logger_, "report error " + std::string(e.what()));
    return false;
  }
}

}  // namespace buried
