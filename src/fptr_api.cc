#include "fptr_api.h"

#include <vector>

#if defined(_WIN32)
#  include <windows.h>
#elif defined(__APPLE__)
#  include <dlfcn.h>
#  include <mach-o/dyld.h>
#else
#  include <dlfcn.h>
#  include <unistd.h>
#endif

FptrApi g_fptrApi = {};

namespace {

std::string g_loadedFrom;

#if defined(_WIN32)
typedef HMODULE LibHandle;
const char kSep = '\\';

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

bool endsWith(const std::string &s, const std::string &tail) {
  return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

// dir empty: let Windows search (PATH, system dirs). Otherwise load from the
// directory, with msvcp140.dll from the same place first, as ATOL's wrapper does.
LibHandle openLib(const std::string &dirOrFile) {
  std::string dir = dirOrFile;
  if (endsWith(dir, "fptr10.dll")) dir = dir.substr(0, dir.size() - std::string("fptr10.dll").size());
  if (!dir.empty() && dir.back() != '\\' && dir.back() != '/') dir += kSep;
  if (dir.empty()) return LoadLibraryW(L"fptr10.dll");
  LoadLibraryExW(widen(dir + "msvcp140.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  return LoadLibraryExW(widen(dir + "fptr10.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
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

std::string installDir() {
  // The installer writes the key into the registry view of its own bitness;
  // try the native view first, then the 32-bit one.
  const REGSAM views[] = {0, KEY_WOW64_32KEY};
  for (REGSAM view : views) {
    wchar_t buf[1024] = {0};
    DWORD size = sizeof(buf);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\ATOL\\Drivers\\10.0\\KKT", L"INSTALL_DIR",
                     RRF_RT_REG_SZ | view, nullptr, buf, &size) == ERROR_SUCCESS && buf[0]) {
      return narrow(buf) + "\\bin";
    }
  }
  return std::string();
}

std::string lastError() {
  return "Windows error " + std::to_string(GetLastError());
}

#else
typedef void *LibHandle;
const char kSep = '/';

#  if defined(__APPLE__)
const char *kLibFiles[] = {"fptr10.framework/fptr10", "libfptr10.dylib"};
#  else
const char *kLibFiles[] = {"libfptr10.so"};
#  endif

bool endsWith(const std::string &s, const std::string &tail) {
  return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

// dirOrFile empty: bare file name, i.e. the system search path.
LibHandle openLib(const std::string &dirOrFile) {
  for (const char *file : kLibFiles) {
    std::string p = dirOrFile;
    if (!endsWith(p, file)) {
      if (!p.empty() && p.back() != kSep) p += kSep;
      p += file;
    }
    LibHandle h = dlopen(p.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (h) return h;
  }
  return nullptr;
}

void *symbol(LibHandle h, const char *name) {
  return dlsym(h, name);
}

std::string libPath(LibHandle h) {
  // dladdr on any exported symbol tells which file was actually mapped.
  Dl_info info;
  void *sym = dlsym(h, "libfptr_get_version_string");
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

// ATOL's installers on Linux and macOS put the library into system locations;
// there is no separate installation directory to look up.
std::string installDir() {
  return std::string();
}

std::string lastError() {
  const char *e = dlerror();
  return e ? e : "unknown error";
}
#endif

template <typename T>
bool resolve(LibHandle h, const char *name, T &fn, std::string &error) {
  fn = reinterpret_cast<T>(symbol(h, name));
  if (!fn) {
    error = std::string("function ") + name + " not found in the driver library";
    return false;
  }
  return true;
}

bool resolveAll(LibHandle h, std::string &error) {
  FptrApi api = {};
  bool ok = resolve(h, "libfptr_get_version_string", api.get_version_string, error) &&
            resolve(h, "libfptr_create", api.create, error) &&
            resolve(h, "libfptr_destroy", api.destroy, error) &&
            resolve(h, "libfptr_get_settings", api.get_settings, error) &&
            resolve(h, "libfptr_set_settings", api.set_settings, error) &&
            resolve(h, "libfptr_open", api.open, error) &&
            resolve(h, "libfptr_close", api.close, error) &&
            resolve(h, "libfptr_is_opened", api.is_opened, error) &&
            resolve(h, "libfptr_process_json", api.process_json, error) &&
            resolve(h, "libfptr_set_param_int", api.set_param_int, error) &&
            resolve(h, "libfptr_set_param_str", api.set_param_str, error) &&
            resolve(h, "libfptr_get_param_int", api.get_param_int, error) &&
            resolve(h, "libfptr_get_param_str", api.get_param_str, error) &&
            resolve(h, "libfptr_get_param_datetime", api.get_param_datetime, error) &&
            resolve(h, "libfptr_error_code", api.error_code, error) &&
            resolve(h, "libfptr_error_description", api.error_description, error) &&
            resolve(h, "libfptr_report", api.report, error) &&
            resolve(h, "libfptr_fn_query_data", api.fn_query_data, error);
  if (ok) g_fptrApi = api;
  return ok;
}

}  // namespace

bool fptrApiLoaded() {
  return g_fptrApi.create != nullptr;
}

const std::string &fptrApiLoadedFrom() {
  return g_loadedFrom;
}

bool fptrApiLoad(const std::string &libraryPath, std::string &loadedFrom, std::string &error) {
  if (fptrApiLoaded()) {
    loadedFrom = g_loadedFrom;
    return true;
  }

  // An explicit path is final: silently falling back to some other driver
  // would hide a misconfiguration.
  std::vector<std::string> candidates;
  if (!libraryPath.empty()) {
    candidates.push_back(libraryPath);
  } else {
    std::string exe = exeDir();
    if (!exe.empty()) candidates.push_back(exe);
    std::string install = installDir();
    if (!install.empty()) candidates.push_back(install);
    candidates.push_back(std::string());  // system search path
  }

  std::string tried;
  for (const std::string &candidate : candidates) {
    LibHandle h = openLib(candidate);
    if (!h) {
      tried += "\n  " + (candidate.empty() ? std::string("<system library path>") : candidate) + ": " + lastError();
      continue;
    }
    if (!resolveAll(h, error)) return false;  // found a library, but not a DTO 10 one
    g_loadedFrom = libPath(h);
    if (g_loadedFrom.empty()) g_loadedFrom = candidate.empty() ? "<system library path>" : candidate;
    loadedFrom = g_loadedFrom;
    return true;
  }

  error = "ATOL DTO 10 driver library not found. Install the driver or pass its location. Tried:" + tried;
  return false;
}
