#include "shtrih_api.h"

#include <cstring>
#include <map>
#include <vector>

#include "plain_mutex.h"

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <mutex>
#  include <dlfcn.h>
#  include <unistd.h>
#  if defined(__APPLE__)
#    include <mach-o/dyld.h>
#  endif
#endif

#include "shtrih_api.inc"

#if defined(_WIN32)
PlainMutex::PlainMutex() : impl_(new CRITICAL_SECTION) { InitializeCriticalSection(static_cast<CRITICAL_SECTION *>(impl_)); }
PlainMutex::~PlainMutex() { DeleteCriticalSection(static_cast<CRITICAL_SECTION *>(impl_)); delete static_cast<CRITICAL_SECTION *>(impl_); }
void PlainMutex::lock() { EnterCriticalSection(static_cast<CRITICAL_SECTION *>(impl_)); }
void PlainMutex::unlock() { LeaveCriticalSection(static_cast<CRITICAL_SECTION *>(impl_)); }
#else
PlainMutex::PlainMutex() : impl_(new std::mutex) {}
PlainMutex::~PlainMutex() { delete static_cast<std::mutex *>(impl_); }
void PlainMutex::lock() { static_cast<std::mutex *>(impl_)->lock(); }
void PlainMutex::unlock() { static_cast<std::mutex *>(impl_)->unlock(); }
#endif

namespace {

std::string g_loadedFrom;
typedef ShtrihCtx (*InitFn)(const char *);
typedef void (*DeinitFn)(ShtrihCtx);
InitFn g_init = nullptr;
DeinitFn g_deinit = nullptr;
std::map<std::string, void *> g_symbols;
PlainMutex g_symbolsMutex;

bool endsWith(const std::string &s, const std::string &tail) {
  return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

#if defined(_WIN32)
typedef HMODULE LibHandle;
const char kSep = '\\';
const char *kLibFile = "classic_fr_drv_ng.dll";
LibHandle g_lib = nullptr;

std::wstring widen(const std::string &s) {
  if (s.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  std::wstring w(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
  w.resize(n - 1);
  return w;
}

std::string narrow(const std::wstring &w) {
  if (w.empty()) return std::string();
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
  std::string s(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
  s.resize(n - 1);
  return s;
}

// Empty: let Windows search (PATH, system dirs). Otherwise the DLL from the
// directory, its dependencies from the same place (LOAD_WITH_ALTERED_SEARCH_PATH).
LibHandle openLib(const std::string &dirOrFile) {
  if (dirOrFile.empty()) return LoadLibraryW(widen(kLibFile).c_str());
  std::string p = dirOrFile;
  if (!endsWith(p, kLibFile)) {
    if (p.back() != '\\' && p.back() != '/') p += kSep;
    p += kLibFile;
  }
  return LoadLibraryExW(widen(p).c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
}

void *symbol(LibHandle h, const char *name) {
  return reinterpret_cast<void *>(GetProcAddress(h, name));
}

std::string libPath(LibHandle h) {
  std::vector<wchar_t> buf(MAX_PATH * 4);
  DWORD n = GetModuleFileNameW(h, &buf[0], static_cast<DWORD>(buf.size()));
  return narrow(std::wstring(&buf[0], n));
}

std::string exeDir() {
  std::vector<wchar_t> buf(MAX_PATH * 4);
  DWORD n = GetModuleFileNameW(nullptr, &buf[0], static_cast<DWORD>(buf.size()));
  std::string p = narrow(std::wstring(&buf[0], n));
  return p.substr(0, p.find_last_of("\\/"));
}

std::string lastError() {
  return "Windows error " + std::to_string(GetLastError());
}

#else
typedef void *LibHandle;
const char kSep = '/';
#  if defined(__APPLE__)
const char *kLibFile = "libclassic_fr_drv_ng.dylib";
#  else
const char *kLibFile = "libclassic_fr_drv_ng.so";
#  endif
LibHandle g_lib = nullptr;

LibHandle openLib(const std::string &dirOrFile) {
  std::string p = dirOrFile;
  if (!endsWith(p, kLibFile)) {
    if (!p.empty() && p.back() != kSep) p += kSep;
    p += kLibFile;
  }
  return dlopen(p.c_str(), RTLD_NOW | RTLD_LOCAL);
}

void *symbol(LibHandle h, const char *name) {
  return dlsym(h, name);
}

std::string libPath(LibHandle h) {
  Dl_info info;
  void *sym = dlsym(h, "c_classic_init");
  if (sym && dladdr(sym, &info) && info.dli_fname) return info.dli_fname;
  return std::string();
}

std::string exeDir() {
#  if defined(__APPLE__)
  std::vector<char> buf(4096);
  uint32_t size = static_cast<uint32_t>(buf.size());
  if (_NSGetExecutablePath(&buf[0], &size) != 0) return std::string();
  std::string p(&buf[0]);
#  else
  std::vector<char> buf(4096);
  ssize_t n = readlink("/proc/self/exe", &buf[0], buf.size() - 1);
  if (n <= 0) return std::string();
  std::string p(&buf[0], static_cast<size_t>(n));
#  endif
  return p.substr(0, p.rfind('/'));
}

std::string lastError() {
  const char *e = dlerror();
  return e ? e : "unknown error";
}
#endif

}  // namespace

bool shtrihApiLoaded() {
  return g_init != nullptr;
}

const std::string &shtrihApiLoadedFrom() {
  return g_loadedFrom;
}

bool shtrihApiLoad(const std::string &libraryPath, std::string &loadedFrom, std::string &error) {
  if (shtrihApiLoaded()) {
    loadedFrom = g_loadedFrom;
    return true;
  }
  std::vector<std::string> candidates;
  if (!libraryPath.empty()) {
    candidates.push_back(libraryPath);
  } else {
    std::string exe = exeDir();
    if (!exe.empty()) candidates.push_back(exe);
    candidates.push_back(std::string());
  }
  std::string tried;
  for (const std::string &candidate : candidates) {
    LibHandle h = openLib(candidate);
    if (!h) {
      tried += "\n  " + (candidate.empty() ? std::string("<system library path>") : candidate) + ": " + lastError();
      continue;
    }
    InitFn init = reinterpret_cast<InitFn>(symbol(h, "c_classic_init"));
    DeinitFn deinit = reinterpret_cast<DeinitFn>(symbol(h, "c_classic_deinit"));
    if (!init || !deinit) {
      error = "c_classic_init / c_classic_deinit not found: " + candidate + " is not the POScenter fr_drv_ng library";
      return false;
    }
    g_lib = h;
    g_init = init;
    g_deinit = deinit;
    g_loadedFrom = libPath(h);
    if (g_loadedFrom.empty()) g_loadedFrom = candidate.empty() ? "<system library path>" : candidate;
    loadedFrom = g_loadedFrom;
    return true;
  }
  error = std::string("Shtrih-M (POScenter fr_drv_ng) driver library ") + kLibFile +
          " not found. Install the driver or pass its location. Tried:" + tried;
  return false;
}

ShtrihCtx shtrihCreate() {
  return g_init ? g_init(nullptr) : nullptr;
}

void shtrihDestroy(ShtrihCtx ctx) {
  if (g_deinit && ctx) g_deinit(ctx);
}

const ShtrihProp *shtrihFindProp(const std::string &name) {
  size_t lo = 0, hi = sizeof(kShtrihProps) / sizeof(kShtrihProps[0]);
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    int c = std::strcmp(kShtrihProps[mid].name, name.c_str());
    if (c == 0) return &kShtrihProps[mid];
    if (c < 0) lo = mid + 1; else hi = mid;
  }
  return nullptr;
}

bool shtrihIsMethod(const std::string &name) {
  size_t lo = 0, hi = sizeof(kShtrihMethods) / sizeof(kShtrihMethods[0]);
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    int c = std::strcmp(kShtrihMethods[mid], name.c_str());
    if (c == 0) return true;
    if (c < 0) lo = mid + 1; else hi = mid;
  }
  return false;
}

void *shtrihSymbol(const std::string &name) {
  if (!g_lib) return nullptr;
  PlainLock lock(g_symbolsMutex);
  auto it = g_symbols.find(name);
  if (it != g_symbols.end()) return it->second;
  void *p = symbol(g_lib, name.c_str());
  g_symbols[name] = p;
  return p;
}
