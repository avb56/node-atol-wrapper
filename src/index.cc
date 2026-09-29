#include "wcast_disable.h"
#include "fptr10.h"
#include "fptr_api.h"

// { loaded, path, version } of the driver library currently in use.
static v8::Local<v8::Object> driverInfoObject() {
  v8::Local<v8::Object> result = Nan::New<v8::Object>();
  bool loaded = fptrApiLoaded();
  Nan::Set(result, Nan::New("loaded").ToLocalChecked(), Nan::New(loaded));
  Nan::Set(result, Nan::New("path").ToLocalChecked(),
           loaded ? Nan::New(fptrApiLoadedFrom()).ToLocalChecked().As<v8::Value>() : Nan::Null().As<v8::Value>());
  Nan::Set(result, Nan::New("version").ToLocalChecked(),
           loaded ? Nan::New(libfptr_get_version_string()).ToLocalChecked().As<v8::Value>() : Nan::Null().As<v8::Value>());
  return result;
}

// loadLibrary([path]) — load the driver now, from path (a directory or the
// library file) or, without it, with the default search order. Returns
// driverInfo(). Throws if the driver is not found; the message lists the
// places tried.
NAN_METHOD(JsLoadLibrary) {
  std::string path;
  if (info.Length() > 0 && !info[0]->IsUndefined() && !info[0]->IsNull()) {
    if (!info[0]->IsString()) {
      return Nan::ThrowError(Nan::New("loadLibrary - expected path to be a string").ToLocalChecked());
    }
    path = *Nan::Utf8String(info[0]);
  }
  std::string loadedFrom, error;
  if (!fptrApiLoad(path, loadedFrom, error)) {
    return Nan::ThrowError(Nan::New(error).ToLocalChecked());
  }
  info.GetReturnValue().Set(driverInfoObject());
}

// driverInfo() — what is loaded, without loading anything.
NAN_METHOD(JsDriverInfo) {
  info.GetReturnValue().Set(driverInfoObject());
}

NAN_MODULE_INIT(InitModule) {
  Fptr10::Init(target);
  Nan::SetMethod(target, "loadLibrary", JsLoadLibrary);
  Nan::SetMethod(target, "driverInfo", JsDriverInfo);
}

DISABLE_WCAST_FUNCTION_TYPE
#if NODE_MAJOR_VERSION >= 10
NAN_MODULE_WORKER_ENABLED(node_atol_wrapper, InitModule)
#else
NODE_MODULE(node_atol_wrapper, InitModule)
#endif
DISABLE_WCAST_FUNCTION_TYPE_END
