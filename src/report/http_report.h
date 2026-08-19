#pragma once

#include <stdint.h>

#include <memory>
#include <string>

namespace spdlog {
class logger;
}

namespace buried {

class HttpReporter {
 public:
  explicit HttpReporter(std::shared_ptr<spdlog::logger> logger,
                        bool secure = false);

  HttpReporter& Host(const std::string& host) {
    host_ = host;
    return *this;
  }

  HttpReporter& Topic(const std::string& topic) {
    topic_ = topic;
    return *this;
  }

  HttpReporter& Port(const std::string& port) {
    port_ = port;
    return *this;
  }

  HttpReporter& Body(const std::string& body) {
    body_ = body;
    return *this;
  }

  bool Report();

 private:
  std::string BuildRequest_();
  bool ReportPlain_();
  bool ReportTls_();

  std::string host_;
  std::string topic_;
  std::string port_;
  std::string body_;

  bool secure_ = false;
  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace buried
