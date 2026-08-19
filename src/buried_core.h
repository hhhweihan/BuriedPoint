#pragma once

#include <stdint.h>

#include <memory>
#include <string>

#include "buried_common.h"
#include "include/buried.h"

namespace spdlog {
class logger;
}

namespace buried {
class BuriedReport;
}

struct Buried {
 public:
  struct Config {
    std::string host;
    std::string port;
    std::string topic;
    std::string user_id;
    std::string app_version;
    std::string app_name;
    std::string custom_data;
    uint32_t report_batch_size = 0;
    bool use_https = false;
  };

 public:
  Buried(const std::string& work_dir);
  ~Buried();

  BuriedResult Start(const Config& config);
  BuriedResult Report(std::string title, std::string data, uint32_t priority);

 private:
  void InitWorkPath_(const std::string& work_dir);
  void InitLogger_();
  std::shared_ptr<spdlog::logger> Logger();

 private:
  std::shared_ptr<spdlog::logger> logger_;
  std::unique_ptr<buried::BuriedReport> buried_report_;
  std::string work_path_;
};
