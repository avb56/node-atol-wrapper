#ifndef INCLUDE_SHTRIH_API_H
#define INCLUDE_SHTRIH_API_H

// Runtime binding to the Shtrih-M / POScenter driver (fr_drv_ng, "classic"
// C interface: libclassic_fr_drv_ng.so, classic_fr_drv_ng.dll).
//
// Same idea as fptr_api.h: the driver is not linked and not shipped; the
// library installed on the machine is loaded on demand. The driver's module
// for TS PIoT (libmp_ts_piot.so next to the library) is loaded by the driver
// itself and serves gRPC on :4042 while a device is connected.
//
// The C interface is flat: a context, Set_<Property>/Get_<Property> and
// methods int <Method>(ctx). Names and value types come from
// shtrih_api.inc (generated from the driver's c_classic_interface.h).

#include <string>

struct ShtrihProp {
  const char *name;
  char setType;  // 0 — no setter
  char getType;  // 0 — no getter
};

typedef void *ShtrihCtx;

// Search order: libraryPath (a directory or the library file), the directory
// of the running executable, the system library search path.
bool shtrihApiLoad(const std::string &libraryPath, std::string &loadedFrom, std::string &error);
bool shtrihApiLoaded();
const std::string &shtrihApiLoadedFrom();

ShtrihCtx shtrihCreate();
void shtrihDestroy(ShtrihCtx ctx);

const ShtrihProp *shtrihFindProp(const std::string &name);
bool shtrihIsMethod(const std::string &name);
// Resolved symbol by name, nullptr if the library does not export it.
void *shtrihSymbol(const std::string &name);

#endif
