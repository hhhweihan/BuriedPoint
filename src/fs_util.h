#pragma once

#include <string>
#include <sys/stat.h>
#include <sys/types.h>

namespace buried {

inline bool PathExists(const std::string& path) {
  if (path.empty()) {
    return false;
  }
  struct stat st;
  return ::stat(path.c_str(), &st) == 0;
}

inline bool CreateDirectories(const std::string& path) {
  if (path.empty()) {
    return false;
  }
  std::string cur;
  size_t pos = 0;
  if (path[0] == '/') {
    cur = "/";
    pos = 1;
  }
  while (pos <= path.size()) {
    size_t next = path.find('/', pos);
    std::string seg =
        path.substr(pos, next == std::string::npos ? std::string::npos
                                                   : next - pos);
    if (!seg.empty()) {
      if (cur == "/") {
        cur += seg;
      } else if (cur.empty()) {
        cur = seg;
      } else {
        cur += "/" + seg;
      }
      ::mkdir(cur.c_str(), 0755);
    }
    if (next == std::string::npos) {
      break;
    }
    pos = next + 1;
  }
  return true;
}

}  // namespace buried
