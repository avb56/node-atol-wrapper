#include "shtrih.h"

#include <ctime>

Nan::Persistent<v8::Function> ShtrihClassic::constructor;

namespace {

void setValue(ShtrihCtx ctx, const ShtrihValue &v) {
  void *fn = shtrihSymbol("Set_" + v.name);
  if (!fn) return;
  switch (v.type) {
    case 'i': reinterpret_cast<void (*)(ShtrihCtx, int)>(fn)(ctx, static_cast<int>(v.i)); break;
    case 'b': reinterpret_cast<void (*)(ShtrihCtx, bool)>(fn)(ctx, v.i != 0); break;
    case 'l': reinterpret_cast<void (*)(ShtrihCtx, int64_t)>(fn)(ctx, v.i); break;
    case 'u': reinterpret_cast<void (*)(ShtrihCtx, uint32_t)>(fn)(ctx, static_cast<uint32_t>(v.i)); break;
    case 't': reinterpret_cast<void (*)(ShtrihCtx, time_t)>(fn)(ctx, static_cast<time_t>(v.i)); break;
    case 'q': reinterpret_cast<void (*)(ShtrihCtx, uint64_t)>(fn)(ctx, v.q); break;
    case 'd': reinterpret_cast<void (*)(ShtrihCtx, double)>(fn)(ctx, v.d); break;
    case 's': reinterpret_cast<void (*)(ShtrihCtx, const char *, size_t)>(fn)(ctx, v.s.data(), v.s.size()); break;
    case 'y':
      reinterpret_cast<void (*)(ShtrihCtx, const uint8_t *, size_t)>(fn)(
          ctx, reinterpret_cast<const uint8_t *>(v.s.data()), v.s.size());
      break;
  }
}

template <typename C>
std::string readBuffer(void *fn, ShtrihCtx ctx) {
  typedef size_t (*Getter)(ShtrihCtx, C *, size_t);
  Getter get = reinterpret_cast<Getter>(fn);
  std::vector<C> buf(1024);
  size_t n = get(ctx, &buf[0], buf.size());
  if (n > buf.size()) {
    buf.resize(n + 1);
    n = get(ctx, &buf[0], buf.size());
  }
  if (n > buf.size()) n = buf.size();
  return std::string(reinterpret_cast<const char *>(&buf[0]), n);
}

void getValue(ShtrihCtx ctx, ShtrihValue &v) {
  void *fn = shtrihSymbol("Get_" + v.name);
  if (!fn) { v.type = 0; return; }
  switch (v.type) {
    case 'i': v.i = reinterpret_cast<int (*)(ShtrihCtx)>(fn)(ctx); break;
    case 'b': v.i = reinterpret_cast<bool (*)(ShtrihCtx)>(fn)(ctx) ? 1 : 0; break;
    case 'l': v.i = reinterpret_cast<int64_t (*)(ShtrihCtx)>(fn)(ctx); break;
    case 'u': v.i = reinterpret_cast<uint32_t (*)(ShtrihCtx)>(fn)(ctx); break;
    case 't': v.i = static_cast<int64_t>(reinterpret_cast<time_t (*)(ShtrihCtx)>(fn)(ctx)); break;
    case 'q': v.q = reinterpret_cast<uint64_t (*)(ShtrihCtx)>(fn)(ctx); break;
    case 'd': v.d = reinterpret_cast<double (*)(ShtrihCtx)>(fn)(ctx); break;
    case 's':
      v.s = readBuffer<char>(fn, ctx);
      while (!v.s.empty() && v.s.back() == '\0') v.s.pop_back();  // with or without the terminator
      break;
    case 'y': v.s = readBuffer<uint8_t>(fn, ctx); break;
  }
}

v8::Local<v8::Value> toJs(const ShtrihValue &v) {
  switch (v.type) {
    case 'b': return Nan::New(v.i != 0);
    case 'i': case 'l': case 'u': case 't': return Nan::New(static_cast<double>(v.i));
    case 'q': return Nan::New(static_cast<double>(v.q));
    case 'd': return Nan::New(v.d);
    case 's': return Nan::New(v.s).ToLocalChecked();
    case 'y': return Nan::CopyBuffer(v.s.data(), static_cast<uint32_t>(v.s.size())).ToLocalChecked();
  }
  return Nan::Undefined();
}

// JS value → property value of the setter's type; false with error text.
bool fromJs(v8::Local<v8::Value> js, char type, ShtrihValue &v, std::string &error) {
  v.type = type;
  switch (type) {
    case 'b':
      v.i = Nan::To<bool>(js).FromMaybe(false) ? 1 : 0;
      return true;
    case 'i': case 'l': case 'u': case 't': case 'q': case 'd': {
      if (js->IsBoolean()) { v.i = Nan::To<bool>(js).FromJust() ? 1 : 0; v.q = v.i; v.d = v.i; return true; }
      if (!js->IsNumber()) { error = "expected a number"; return false; }
      double d = Nan::To<double>(js).FromJust();
      v.d = d;
      v.i = static_cast<int64_t>(d);
      v.q = static_cast<uint64_t>(d < 0 ? 0 : d);
      return true;
    }
    case 's':
      if (node::Buffer::HasInstance(js)) {
        v.s.assign(node::Buffer::Data(js), node::Buffer::Length(js));
        return true;
      }
      v.s = *Nan::Utf8String(js);
      return true;
    case 'y':
      if (node::Buffer::HasInstance(js)) {
        v.s.assign(node::Buffer::Data(js), node::Buffer::Length(js));
        return true;
      }
      if (js->IsString()) { v.s = *Nan::Utf8String(js); return true; }
      error = "expected a Buffer";
      return false;
  }
  error = "unsupported property type";
  return false;
}

class CallWorker : public Nan::AsyncWorker {
 public:
  CallWorker(ShtrihClassic *obj, std::string method, std::vector<ShtrihValue> in, std::vector<ShtrihValue> out,
             Nan::Callback *callback)
      : Nan::AsyncWorker(callback), obj_(obj), method_(method), in_(in), out_(out) {}

  void Execute() override {
    PlainLock lock(obj_->mutex);
    if (!obj_->ctx) { SetErrorMessage("ShtrihClassic: context destroyed"); return; }
    void *fn = shtrihSymbol(method_);
    if (!fn) { SetErrorMessage(("ShtrihClassic: the driver has no method " + method_).c_str()); return; }
    for (const ShtrihValue &v : in_) setValue(obj_->ctx, v);
    reinterpret_cast<int (*)(ShtrihCtx)>(fn)(obj_->ctx);
    for (ShtrihValue &v : out_) getValue(obj_->ctx, v);
  }

  void HandleOKCallback() override {
    Nan::HandleScope scope;
    v8::Local<v8::Object> result = Nan::New<v8::Object>();
    for (const ShtrihValue &v : out_) {
      if (v.type) Nan::Set(result, Nan::New(v.name).ToLocalChecked(), toJs(v));
    }
    v8::Local<v8::Value> argv[] = {Nan::Null(), result};
    Nan::Call(callback->GetFunction(), Nan::GetCurrentContext()->Global(), 2, argv);
  }

 private:
  ShtrihClassic *obj_;
  std::string method_;
  std::vector<ShtrihValue> in_;
  std::vector<ShtrihValue> out_;
};

}  // namespace

ShtrihClassic::ShtrihClassic() {}

ShtrihClassic::~ShtrihClassic() {
  PlainLock lock(mutex);
  shtrihDestroy(ctx);
  ctx = nullptr;
}

NAN_MODULE_INIT(ShtrihClassic::Init) {
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("ShtrihClassic").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  Nan::SetPrototypeMethod(tpl, "call", Call);
  Nan::SetPrototypeMethod(tpl, "destroy", Destroy);
  constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
  Nan::Set(target, Nan::New("ShtrihClassic").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(ShtrihClassic::New) {
  if (!info.IsConstructCall()) return Nan::ThrowError("ShtrihClassic: use new");
  if (!shtrihApiLoaded()) return Nan::ThrowError("ShtrihClassic: the driver library is not loaded (loadShtrihLibrary)");
  ShtrihClassic *obj = new ShtrihClassic();
  obj->ctx = shtrihCreate();
  if (!obj->ctx) {
    delete obj;
    return Nan::ThrowError("ShtrihClassic: c_classic_init failed");
  }
  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

// call(method, props, outNames, callback)
NAN_METHOD(ShtrihClassic::Call) {
  ShtrihClassic *obj = Nan::ObjectWrap::Unwrap<ShtrihClassic>(info.Holder());
  if (info.Length() < 4 || !info[0]->IsString() || !info[3]->IsFunction()) {
    return Nan::ThrowError("call(method, props, outNames, callback)");
  }
  std::string method = *Nan::Utf8String(info[0]);
  if (!shtrihIsMethod(method)) return Nan::ThrowError(("ShtrihClassic: unknown method " + method).c_str());

  std::vector<ShtrihValue> in;
  if (info[1]->IsObject()) {
    v8::Local<v8::Object> props = info[1].As<v8::Object>();
    v8::Local<v8::Array> keys = Nan::GetOwnPropertyNames(props).ToLocalChecked();
    for (uint32_t k = 0; k < keys->Length(); k++) {
      v8::Local<v8::Value> key = Nan::Get(keys, k).ToLocalChecked();
      std::string name = *Nan::Utf8String(key);
      const ShtrihProp *p = shtrihFindProp(name);
      if (!p || !p->setType) return Nan::ThrowError(("ShtrihClassic: property " + name + " cannot be set").c_str());
      ShtrihValue v;
      v.name = name;
      std::string error;
      if (!fromJs(Nan::Get(props, key).ToLocalChecked(), p->setType, v, error)) {
        return Nan::ThrowError(("ShtrihClassic: " + name + " — " + error).c_str());
      }
      in.push_back(v);
    }
  }

  std::vector<ShtrihValue> out;
  std::vector<std::string> names = {"ResultCode", "ResultCodeDescription"};
  if (info[2]->IsArray()) {
    v8::Local<v8::Array> arr = info[2].As<v8::Array>();
    for (uint32_t k = 0; k < arr->Length(); k++) names.push_back(*Nan::Utf8String(Nan::Get(arr, k).ToLocalChecked()));
  }
  for (const std::string &name : names) {
    const ShtrihProp *p = shtrihFindProp(name);
    if (!p || !p->getType) return Nan::ThrowError(("ShtrihClassic: property " + name + " cannot be read").c_str());
    ShtrihValue v;
    v.name = name;
    v.type = p->getType;
    out.push_back(v);
  }

  Nan::Callback *callback = new Nan::Callback(info[3].As<v8::Function>());
  CallWorker *worker = new CallWorker(obj, method, in, out, callback);
  worker->SaveToPersistent("self", info.Holder());  // keep the object alive during the call
  Nan::AsyncQueueWorker(worker);
}

NAN_METHOD(ShtrihClassic::Destroy) {
  ShtrihClassic *obj = Nan::ObjectWrap::Unwrap<ShtrihClassic>(info.Holder());
  PlainLock lock(obj->mutex);
  shtrihDestroy(obj->ctx);
  obj->ctx = nullptr;
}
