#pragma once
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <atomic>
#include <chrono>
#include <ctime>

namespace wiq {
namespace log {

enum class Level { TRACE=0, DEBUG=1, INFO=2, WARN=3, ERROR=4, NONE=5 };

inline const char* level_name(Level lv) {
  switch (lv) {
    case Level::TRACE: return "TRACE";
    case Level::DEBUG: return "DEBUG";
    case Level::INFO:  return "INFO";
    case Level::WARN:  return "WARN";
    case Level::ERROR: return "ERROR";
    default:           return "NONE";
  }
}

inline Level parse_level(const char* s) {
  if (!s || !*s) return Level::INFO;
  auto eq = [](char a, char b){ return (a==b) || (a>='A' && a<='Z' && (a+32)==b); };
  auto starts = [&](const char* p, const char* t){ while (*t && *p) { if (!eq(*p,*t)) return false; ++p; ++t; } return *t=='\0'; };
  if (starts(s, "trace") || std::strcmp(s, "0")==0) return Level::TRACE;
  if (starts(s, "debug") || std::strcmp(s, "1")==0) return Level::DEBUG;
  if (starts(s, "info")  || std::strcmp(s, "2")==0) return Level::INFO;
  if (starts(s, "warn")  || std::strcmp(s, "3")==0) return Level::WARN;
  if (starts(s, "error") || std::strcmp(s, "4")==0) return Level::ERROR;
  if (starts(s, "none")  || std::strcmp(s, "5")==0) return Level::NONE;
  return Level::INFO;
}

inline bool parse_bool(const char* s, bool defv=false) {
  if (!s) return defv;
  if (*s=='\0') return defv;
  if (s[0]=='1' && s[1]=='\0') return true;
  if (s[0]=='0' && s[1]=='\0') return false;
  auto eq = [](char a, char b) {
    if (a == b) return true;
    if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + 32);
    if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + 32);
    return a == b;
  };
  auto starts = [&](const char* value, const char* target) {
    std::size_t idx = 0;
    while (target[idx]) {
      if (value[idx] == '\0') return false;
      if (!eq(value[idx], target[idx])) return false;
      ++idx;
    }
    return true;
  };
  if (starts(s, "true")) return true;
  if (starts(s, "false")) return false;
  return defv;
}

inline Level current_level() {
  static std::atomic<int> cached{-1};
  int v = cached.load(std::memory_order_acquire);
  if (v >= 0) return static_cast<Level>(v);
  const char* env = std::getenv("WIQ_LOG_LEVEL");
  Level lv = parse_level(env);
  cached.store(static_cast<int>(lv), std::memory_order_release);
  return lv;
}

inline bool with_timestamp() {
  static std::atomic<int> cached{-1};
  int v = cached.load(std::memory_order_acquire);
  if (v >= 0) return v != 0;
  const char* env = std::getenv("WIQ_LOG_TS");
  bool on = parse_bool(env, false);
  cached.store(on ? 1 : 0, std::memory_order_release);
  return on;
}

inline void logv(Level lv, const char* file, int line, const char* fmt, va_list ap) {
  if (lv < current_level()) return;
  char msg[1024];
#if defined(_MSC_VER)
  vsnprintf(msg, sizeof(msg), fmt, ap);
#else
  vsnprintf(msg, sizeof(msg), fmt, ap);
#endif

  if (with_timestamp()) {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char ts[32];
    std::snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d",
                  tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    std::fprintf(stderr, "[%s] %-5s %s:%d: %s\n", ts, level_name(lv), file, line, msg);
  } else {
    std::fprintf(stderr, "%-5s %s:%d: %s\n", level_name(lv), file, line, msg);
  }
}

inline void logf(Level lv, const char* file, int line, const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  logv(lv, file, line, fmt, ap);
  va_end(ap);
}

template <typename... Args>
inline void log_trace(const char* file, int line, const char* fmt, Args... args) {
  logf(Level::TRACE, file, line, fmt, args...);
}

template <typename... Args>
inline void log_debug(const char* file, int line, const char* fmt, Args... args) {
  logf(Level::DEBUG, file, line, fmt, args...);
}

template <typename... Args>
inline void log_info(const char* file, int line, const char* fmt, Args... args) {
  logf(Level::INFO, file, line, fmt, args...);
}

template <typename... Args>
inline void log_warn(const char* file, int line, const char* fmt, Args... args) {
  logf(Level::WARN, file, line, fmt, args...);
}

template <typename... Args>
inline void log_error(const char* file, int line, const char* fmt, Args... args) {
  logf(Level::ERROR, file, line, fmt, args...);
}

} // namespace log
} // namespace wiq
