#ifndef INCLUDE_SHTRIH_H
#define INCLUDE_SHTRIH_H

#include "wcast_disable.h"

#include <string>
#include <vector>
DISABLE_WCAST_FUNCTION_TYPE
#include <nan.h>
DISABLE_WCAST_FUNCTION_TYPE_END

#include "shtrih_api.h"
#include "plain_mutex.h"

// A value of a driver property: type letter from shtrih_api.inc and the data.
struct ShtrihValue {
  std::string name;
  char type = 0;
  int64_t i = 0;
  uint64_t q = 0;
  double d = 0;
  std::string s;  // string (UTF-8) or bytes
};

// One driver context (one device connection). JS:
//   const o = new ShtrihClassic();
//   o.call(method, { Property: value, ... }, ['OutProperty', ...], (err, result) => {})
//     — sets the properties, runs the method in a worker thread, reads
//       ResultCode, ResultCodeDescription and the listed properties.
//       err — only for misuse (unknown method or property, destroyed context);
//       what the device or driver says is in result.ResultCode.
//   o.destroy()
// Calls on one context are serialized; different contexts run in parallel.
class ShtrihClassic : public Nan::ObjectWrap {
 public:
  static NAN_MODULE_INIT(Init);

  ShtrihCtx ctx = nullptr;
  PlainMutex mutex;

 private:
  ShtrihClassic();
  ~ShtrihClassic();
  static NAN_METHOD(New);
  static NAN_METHOD(Call);
  static NAN_METHOD(Destroy);
  static Nan::Persistent<v8::Function> constructor;
};

#endif
