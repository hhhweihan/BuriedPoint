#include "buried_config.h"
#include "common/common_service.h"
#include "fs_util.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <unistd.h>
#include <sys/utsname.h>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

namespace buried {

namespace {

std::string GetDeviceConfigPath() {
  const char* home = std::getenv("HOME");
  if (!home || !*home) {
    home = ".";
  }
  std::string config_dir = std::string(home) + "/.buried";
  if (!buried::PathExists(config_dir)) {
    buried::CreateDirectories(config_dir);
  }
  return config_dir + "/device_id";
}

std::string GetDeviceId() {
  static std::string device_id;
  if (device_id.empty()) {
    std::ifstream in(GetDeviceConfigPath());
    if (in) {
      std::getline(in, device_id);
    }
    if (device_id.empty()) {
      device_id = CommonService::GetRandomId();
      std::ofstream out(GetDeviceConfigPath(), std::ios::trunc);
      if (out) {
        out << device_id;
      }
    }
  }
  return device_id;
}

std::string GetLifeCycleId() {
  static std::string life_cycle_id = CommonService::GetRandomId();
  return life_cycle_id;
}

std::string GetSystemVersion() {
#if defined(__APPLE__)
  char buf[128] = {0};
  size_t size = sizeof(buf);
  if (sysctlbyname("kern.osproductversion", buf, &size, nullptr, 0) == 0 &&
      buf[0] != '\0') {
    return buf;
  }
#endif
  struct utsname uts;
  if (uname(&uts) == 0) {
    return uts.release;
  }
  return "unknown";
}

std::string GetDeviceName() {
  char buf[256] = {0};
  if (gethostname(buf, sizeof(buf)) != 0) {
    return "";
  }
  return buf;
}

time_t GetProcessStartTime(long* msec_out) {
  time_t start_sec = 0;
  long start_msec = 0;
#if defined(__APPLE__)
  struct kinfo_proc info;
  std::memset(&info, 0, sizeof(info));
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
  size_t size = sizeof(info);
  if (sysctl(mib, 4, &info, &size, nullptr, 0) == 0) {
    start_sec = info.kp_proc.p_starttime.tv_sec;
    start_msec = info.kp_proc.p_starttime.tv_usec / 1000;
  }
#elif defined(__linux__)
  {
    unsigned long long btime = 0;
    std::ifstream stat_file("/proc/stat");
    std::string line;
    while (std::getline(stat_file, line)) {
      if (line.compare(0, 5, "btime") == 0) {
        btime = std::strtoull(line.c_str() + 5, nullptr, 10);
        break;
      }
    }
    std::ifstream self_stat("/proc/self/stat");
    std::string content((std::istreambuf_iterator<char>(self_stat)),
                        std::istreambuf_iterator<char>());
    size_t rparen = content.rfind(')');
    if (rparen != std::string::npos) {
      std::istringstream iss(content.substr(rparen + 1));
      unsigned long long f3, f4, f5, f6, f7, f8, f9, f10, f11, f12;
      unsigned long long f13, f14, f15, f16, f17, f18, f19, f20, f21, f22;
      iss >> f3 >> f4 >> f5 >> f6 >> f7 >> f8 >> f9 >> f10 >> f11 >> f12 >>
          f13 >> f14 >> f15 >> f16 >> f17 >> f18 >> f19 >> f20 >> f21 >> f22;
      long ticks_per_sec = sysconf(_SC_CLK_TCK);
      if (ticks_per_sec > 0) {
        start_sec = static_cast<time_t>(btime + f22 / ticks_per_sec);
        start_msec = static_cast<long>((f22 % ticks_per_sec) * 1000 /
                                       ticks_per_sec);
      }
    }
  }
#endif
  if (msec_out) {
    *msec_out = start_msec;
  }
  return start_sec;
}

}  // namespace

CommonService::CommonService() { Init(); }

std::string CommonService::GetProcessTime() {
  long msec = 0;
  time_t sec = GetProcessStartTime(&msec);
  if (sec == 0) {
    return "";
  }
  struct tm local_tm;
  localtime_r(&sec, &local_tm);
  char buf[128] = {0};
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
                local_tm.tm_year + 1900, local_tm.tm_mon + 1,
                local_tm.tm_mday, local_tm.tm_hour, local_tm.tm_min,
                local_tm.tm_sec, msec);
  return buf;
}

std::string CommonService::GetNowDate() {
  time_t t = time(nullptr);
  struct tm local_tm;
  localtime_r(&t, &local_tm);
  char buf[32] = {0};
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local_tm);
  return buf;
}

std::string CommonService::GetRandomId() {
  static constexpr size_t len = 32;
  static constexpr auto chars =
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  static std::mt19937_64 rng{std::random_device{}()};
  static std::uniform_int_distribution<size_t> dist{0, 61};
  std::string result;
  result.reserve(len);
  std::generate_n(std::back_inserter(result), len,
                  [&]() { return chars[dist(rng)]; });
  return result;
}

void CommonService::Init() {
  system_version = GetSystemVersion();
  device_name = GetDeviceName();
  device_id = GetDeviceId();
  buried_version = PROJECT_VER;
  lifecycle_id = GetLifeCycleId();
}

}  // namespace buried
