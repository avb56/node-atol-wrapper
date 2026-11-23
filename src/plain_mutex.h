#ifndef INCLUDE_PLAIN_MUTEX_H
#define INCLUDE_PLAIN_MUTEX_H

// A plain lock instead of std::mutex. With the static MSVC runtime std::mutex
// pulls in GetSystemTimePreciseAsFileTime (Windows 8+): on Windows 7 the addon
// then fails to load — "The specified procedure could not be found". On
// Windows this is a CRITICAL_SECTION (works on XP and later), elsewhere
// std::mutex. Implementation — shtrih_api.cc (keeps windows.h out of headers).
class PlainMutex {
 public:
  PlainMutex();
  ~PlainMutex();
  void lock();
  void unlock();
  PlainMutex(const PlainMutex &) = delete;
  PlainMutex &operator=(const PlainMutex &) = delete;

 private:
  void *impl_;
};

class PlainLock {
 public:
  explicit PlainLock(PlainMutex &m) : m_(m) { m_.lock(); }
  ~PlainLock() { m_.unlock(); }
  PlainLock(const PlainLock &) = delete;
  PlainLock &operator=(const PlainLock &) = delete;

 private:
  PlainMutex &m_;
};

#endif
