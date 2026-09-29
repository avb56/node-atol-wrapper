#ifndef INCLUDE_FPTR_API_H
#define INCLUDE_FPTR_API_H

// Runtime binding to the ATOL DTO 10 driver library.
//
// The wrapper does not link against fptr10 at build time and does not ship the
// driver: it loads the library installed on the machine when the first driver
// handle is created (or when loadLibrary() is called explicitly). This way the
// addon works with whatever DTO 10.x version is installed and does not need a
// rebuild for every driver release.
//
// libfptr10.h is still included for types and constants. The calls in the rest
// of the code stay as written (libfptr_xxx(...)): the macros at the bottom of
// this file route them to the function pointers resolved at load time.

#include <string>
#include "libfptr10.h"

struct FptrApi {
  decltype(&::libfptr_get_version_string) get_version_string;
  decltype(&::libfptr_create) create;
  decltype(&::libfptr_destroy) destroy;
  decltype(&::libfptr_get_settings) get_settings;
  decltype(&::libfptr_set_settings) set_settings;
  decltype(&::libfptr_open) open;
  decltype(&::libfptr_close) close;
  decltype(&::libfptr_is_opened) is_opened;
  decltype(&::libfptr_process_json) process_json;
  decltype(&::libfptr_set_param_int) set_param_int;
  decltype(&::libfptr_set_param_str) set_param_str;
  decltype(&::libfptr_get_param_int) get_param_int;
  decltype(&::libfptr_get_param_str) get_param_str;
  decltype(&::libfptr_get_param_datetime) get_param_datetime;
  decltype(&::libfptr_error_code) error_code;
  decltype(&::libfptr_error_description) error_description;
  decltype(&::libfptr_report) report;
  decltype(&::libfptr_fn_query_data) fn_query_data;
};

extern FptrApi g_fptrApi;

// Loads the driver library. Search order is the same as in ATOL's own
// wrappers (C++ fptr10.h shipped with the driver):
//   1. libraryPath, if not empty — a directory or the library file itself;
//   2. the directory of the running executable;
//   3. the driver installation directory (Windows: registry
//      HKLM\SOFTWARE\ATOL\Drivers\10.0\KKT, INSTALL_DIR + \bin);
//   4. the system library search path.
// On success fills loadedFrom with where the library came from. Loading twice
// is a no-op that reports the first location.
bool fptrApiLoad(const std::string &libraryPath, std::string &loadedFrom, std::string &error);
bool fptrApiLoaded();
const std::string &fptrApiLoadedFrom();

#define libfptr_get_version_string (g_fptrApi.get_version_string)
#define libfptr_create (g_fptrApi.create)
#define libfptr_destroy (g_fptrApi.destroy)
#define libfptr_get_settings (g_fptrApi.get_settings)
#define libfptr_set_settings (g_fptrApi.set_settings)
#define libfptr_open (g_fptrApi.open)
#define libfptr_close (g_fptrApi.close)
#define libfptr_is_opened (g_fptrApi.is_opened)
#define libfptr_process_json (g_fptrApi.process_json)
#define libfptr_set_param_int (g_fptrApi.set_param_int)
#define libfptr_set_param_str (g_fptrApi.set_param_str)
#define libfptr_get_param_int (g_fptrApi.get_param_int)
#define libfptr_get_param_str (g_fptrApi.get_param_str)
#define libfptr_get_param_datetime (g_fptrApi.get_param_datetime)
#define libfptr_error_code (g_fptrApi.error_code)
#define libfptr_error_description (g_fptrApi.error_description)
#define libfptr_report (g_fptrApi.report)
#define libfptr_fn_query_data (g_fptrApi.fn_query_data)

#endif
