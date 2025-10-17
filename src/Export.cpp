#include "export.hpp"
#include "IModbusClient.hpp"
#include <nlohmann/json.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <fstream>
#include <cmath>
#include <set>

#if defined(WITH_LIBMODBUS)
# include <modbus.h>
#endif

using nlohmann::json;

namespace wiq {

#if defined(WITH_LIBMODBUS)
class TcpModbusClient : public IModbusClient {
public:
  TcpModbusClient(std::string host, int port)
  : host_(std::move(host)), port_(port), ctx_(nullptr), timeout_ms_(1000) {}
  ~TcpModbusClient() override { close(); }

  int connect() override {
    close();
    ctx_ = modbus_new_tcp(host_.c_str(), port_);
    if (!ctx_) return static_cast<int>(ModbusErr::IO_ERROR);
    // set timeouts
    set_timeout_ms(timeout_ms_);
    if (modbus_connect(ctx_) == -1) {
      modbus_free(ctx_); ctx_ = nullptr; return static_cast<int>(ModbusErr::IO_ERROR);
    }
    return 0;
  }
  void close() override {
    if (ctx_) { modbus_close(ctx_); modbus_free(ctx_); ctx_ = nullptr; }
  }
  void set_timeout_ms(int ms) override {
    timeout_ms_ = ms;
    if (ctx_) {
      struct timeval tv; tv.tv_sec = ms/1000; tv.tv_usec = (ms%1000)*1000;
      modbus_set_response_timeout(ctx_, tv.tv_sec, tv.tv_usec);
    }
  }

  int read_coils(int unit, int addr, int count, std::uint8_t* out) override {
    if (!ctx_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    modbus_set_slave(ctx_, unit);
    int rc = modbus_read_bits(ctx_, addr, count, out);
    return (rc == count) ? 0 : static_cast<int>(ModbusErr::IO_ERROR);
  }
  int read_discrete_inputs(int unit, int addr, int count, std::uint8_t* out) override {
    if (!ctx_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    modbus_set_slave(ctx_, unit);
    int rc = modbus_read_input_bits(ctx_, addr, count, out);
    return (rc == count) ? 0 : static_cast<int>(ModbusErr::IO_ERROR);
  }
  int read_holding_regs(int unit, int addr, int count, std::uint16_t* out) override {
    if (!ctx_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    modbus_set_slave(ctx_, unit);
    int rc = modbus_read_registers(ctx_, addr, count, out);
    return (rc == count) ? 0 : static_cast<int>(ModbusErr::IO_ERROR);
  }
  int read_input_regs(int unit, int addr, int count, std::uint16_t* out) override {
    if (!ctx_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    modbus_set_slave(ctx_, unit);
    int rc = modbus_read_input_registers(ctx_, addr, count, out);
    return (rc == count) ? 0 : static_cast<int>(ModbusErr::IO_ERROR);
  }
  int write_single_coil(int unit, int addr, bool on) override {
    if (!ctx_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    modbus_set_slave(ctx_, unit);
    int rc = modbus_write_bit(ctx_, addr, on ? 1 : 0);
    return (rc == 1) ? 0 : static_cast<int>(ModbusErr::IO_ERROR);
  }
  int write_single_reg(int unit, int addr, std::uint16_t value) override {
    if (!ctx_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    modbus_set_slave(ctx_, unit);
    int rc = modbus_write_register(ctx_, addr, value);
    return (rc == 1) ? 0 : static_cast<int>(ModbusErr::IO_ERROR);
  }
  int write_multiple_coils(int unit, int addr, int count, const std::uint8_t* v) override {
    if (!ctx_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    modbus_set_slave(ctx_, unit);
    int rc = modbus_write_bits(ctx_, addr, count, v);
    return (rc == count) ? 0 : static_cast<int>(ModbusErr::IO_ERROR);
  }
  int write_multiple_regs(int unit, int addr, int count, const std::uint16_t* v) override {
    if (!ctx_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    modbus_set_slave(ctx_, unit);
    int rc = modbus_write_registers(ctx_, addr, count, v);
    return (rc == count) ? 0 : static_cast<int>(ModbusErr::IO_ERROR);
  }

private:
  std::string host_; int port_;
  modbus_t* ctx_;
  int timeout_ms_;
};
#endif

struct ItemCfg {
  std::string name;
  int unit_id{};
  int function{}; // 1/2/3/4/5/6/15/16
  int address{};
  int count{1};
  std::string type; // bool|int16|uint16|int32|uint32|float|double
  double scale{1.0};
  double offset{0.0};
  bool swap_words{false};
  int poll_ms{0};
};

struct IoContext {
  std::unordered_map<std::string, ItemCfg> items;
  std::unique_ptr<IModbusClient> client;
  // tcp config
  std::string host; int port{1502}; int timeout_ms{1000};
  // transport
  std::string transport; // tcp|rtu
};

static bool load_file(const char* path, std::string& out) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) return false;
  out.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
  return true;
}

static std::uint32_t join_u32(std::uint16_t hi, std::uint16_t lo, bool swap_words) {
  return swap_words ? (std::uint32_t(lo) << 16) | hi
                    : (std::uint32_t(hi) << 16) | lo;
}

static void split_u32(std::uint32_t v, bool swap_words, std::uint16_t& hi, std::uint16_t& lo) {
  std::uint16_t h = std::uint16_t((v >> 16) & 0xFFFF);
  std::uint16_t l = std::uint16_t(v & 0xFFFF);
  if (swap_words) { hi = l; lo = h; } else { hi = h; lo = l; }
}

static double apply_scale(double raw, double scale, double offset) { return raw * scale + offset; }
static double unscale(double scaled, double scale, double offset) { return (scale == 0.0) ? 0.0 : (scaled - offset) / scale; }

static std::unique_ptr<IModbusClient> make_client_for(const std::string& transport, const std::string& host, int port) {
#if defined(WITH_LIBMODBUS)
  if (transport == "tcp") {
    return std::unique_ptr<IModbusClient>(new TcpModbusClient(host, port));
  }
#endif
  // fallback to stub
  return make_stub_client();
}

} // namespace wiq

extern "C" {

using IoHandle = void*;

WIQ_IOH_API IoHandle CreateIoInstance(void* /*user_param*/, const char* jsonConfigPath) {
  if (!jsonConfigPath) return nullptr;
  std::string text;
  if (!wiq::load_file(jsonConfigPath, text)) return nullptr;
  nlohmann::json cfg;
  try { cfg = nlohmann::json::parse(text); } catch (...) { return nullptr; }

  std::unique_ptr<wiq::IoContext> ctx(new wiq::IoContext());
  ctx->transport = cfg.value("transport", std::string("tcp"));
  if (!(ctx->transport == "tcp" || ctx->transport == "rtu")) {
    return nullptr;
  }
  if (cfg.contains("tcp")) {
    auto t = cfg["tcp"]; ctx->host = t.value("host", std::string("127.0.0.1")); ctx->port = t.value("port", 1502); ctx->timeout_ms = t.value("timeout_ms", 1000);
    if (ctx->port <= 0 || ctx->port > 65535) return nullptr;
  }
  ctx->client = wiq::make_client_for(ctx->transport, ctx->host, ctx->port);
  if (ctx->client) ctx->client->set_timeout_ms(ctx->timeout_ms);
  // parse items
  if (!(cfg.contains("items") && cfg["items"].is_array() && !cfg["items"].empty())) {
    return nullptr;
  }

  auto is_valid_type = [](const std::string& t)->bool{
    static const std::set<std::string> ok = {"bool","int16","uint16","int32","uint32","float","double"};
    return ok.count(t) != 0;
  };
  auto is_valid_fc = [](int fc)->bool{
    return fc==1||fc==2||fc==3||fc==4||fc==5||fc==6||fc==15||fc==16;
  };

  for (auto& it : cfg["items"]) {
    wiq::ItemCfg ic;
    ic.name = it.value("name", std::string());
    ic.unit_id = it.value("unit_id", 1);
    ic.function = it.value("function", 3);
    ic.address = it.value("address", 0);
    ic.count = it.value("count", 1);
    ic.type = it.value("type", std::string("int16"));
    ic.scale = it.value("scale", 1.0);
    ic.offset = it.value("offset", 0.0);
    ic.swap_words = it.value("swap_words", false);
    ic.poll_ms = it.value("poll_ms", 0);

    if (ic.name.empty()) return nullptr;
    if (!is_valid_fc(ic.function)) return nullptr;
    if (!is_valid_type(ic.type)) return nullptr;
    if (ic.unit_id < 1 || ic.unit_id > 247) return nullptr;
    if (ic.address < 0) return nullptr;
    if (ic.count < 1) ic.count = 1;
    if (ic.function == 5 || ic.function == 6) ic.count = 1; // single
    if ((ic.function == 15 || ic.function == 16) && ic.count < 1) return nullptr;
    // type compatibility
    if ((ic.function == 1 || ic.function == 2 || ic.function == 5 || ic.function == 15) && ic.type != "bool") return nullptr;
    if ((ic.function == 3 || ic.function == 4 || ic.function == 6 || ic.function == 16) && ic.type == "bool") return nullptr;
    if ((ic.type == std::string("float")) && ic.count < 2 && (ic.function == 3 || ic.function == 4 || ic.function == 16)) ic.count = 2;

    ctx->items.emplace(ic.name, ic);
  }

  // Connect to backend
  if (ctx->client) {
    (void)ctx->client->connect();
  }

  return reinterpret_cast<IoHandle>(ctx.release());
}

WIQ_IOH_API void DestroyIoInstance(IoHandle h) {
  auto* ctx = reinterpret_cast<wiq::IoContext*>(h);
  if (!ctx) return;
  if (ctx->client) ctx->client->close();
  delete ctx;
}

WIQ_IOH_API int SubscribeItems(IoHandle h, const char** /*names*/, int /*count*/) {
  auto* ctx = reinterpret_cast<wiq::IoContext*>(h);
  if (!ctx) return static_cast<int>(wiq::ModbusErr::INVALID_ARG);
  return 0; // no-op for now
}

WIQ_IOH_API int UnsubscribeItems(IoHandle h, const char** /*names*/, int /*count*/) {
  auto* ctx = reinterpret_cast<wiq::IoContext*>(h);
  if (!ctx) return static_cast<int>(wiq::ModbusErr::INVALID_ARG);
  return 0;
}

static bool write_str(char* out, int outSize, const std::string& s) {
  if (!out || outSize <= 0) return false;
  size_t len = s.size();
  size_t n = (len >= static_cast<size_t>(outSize)) ? static_cast<size_t>(outSize - 1) : len;
  std::memcpy(out, s.data(), n);
  out[n] = '\0';
  return n == len;
}

WIQ_IOH_API int ReadItem(IoHandle h, const char* name, /*out*/char* outJson, int outSize) {
  auto* ctx = reinterpret_cast<wiq::IoContext*>(h);
  if (!ctx || !name) return static_cast<int>(wiq::ModbusErr::INVALID_ARG);
  auto it = ctx->items.find(name);
  if (it == ctx->items.end()) return static_cast<int>(wiq::ModbusErr::NOT_FOUND);
  const wiq::ItemCfg& ic = it->second;

  if (!ctx->client) return static_cast<int>(wiq::ModbusErr::NOT_CONNECTED);

  if (ic.function == 1) {
    int count = ic.count > 0 ? ic.count : 1;
    if (count == 1) {
      std::uint8_t v = 0; int rc = ctx->client->read_coils(ic.unit_id, ic.address, 1, &v);
      if (rc != 0) return rc;
      return write_str(outJson, outSize, v ? "true" : "false") ? 0 : 0;
    } else {
      std::vector<std::uint8_t> buf(count);
      int rc = ctx->client->read_coils(ic.unit_id, ic.address, count, buf.data());
      if (rc != 0) return rc;
      json arr = json::array();
      for (int i = 0; i < count; ++i) arr.push_back(buf[i] != 0);
      return write_str(outJson, outSize, arr.dump()) ? 0 : 0;
    }
  }
  if (ic.function == 2) {
    int count = ic.count > 0 ? ic.count : 1;
    if (count == 1) {
      std::uint8_t v = 0; int rc = ctx->client->read_discrete_inputs(ic.unit_id, ic.address, 1, &v);
      if (rc != 0) return rc;
      return write_str(outJson, outSize, v ? "true" : "false") ? 0 : 0;
    } else {
      std::vector<std::uint8_t> buf(count);
      int rc = ctx->client->read_discrete_inputs(ic.unit_id, ic.address, count, buf.data());
      if (rc != 0) return rc;
      json arr = json::array();
      for (int i = 0; i < count; ++i) arr.push_back(buf[i] != 0);
      return write_str(outJson, outSize, arr.dump()) ? 0 : 0;
    }
  }
  if (ic.function == 3) {
    if (ic.type == "float") {
      std::uint16_t rr[2] = {0,0};
      int rc = ctx->client->read_holding_regs(ic.unit_id, ic.address, 2, rr);
      if (rc != 0) return rc;
      std::uint32_t u = wiq::join_u32(rr[0], rr[1], ic.swap_words);
      float f; std::memcpy(&f, &u, 4);
      std::string s = std::to_string(static_cast<double>(f));
      (void)write_str(outJson, outSize, s);
      return 0;
    }
    int need = ic.count > 0 ? ic.count : 1;
    if (need == 1) {
      std::uint16_t r = 0; int rc = ctx->client->read_holding_regs(ic.unit_id, ic.address, 1, &r);
      if (rc != 0) return rc;
      if (ic.type == "int16") {
        double val = wiq::apply_scale(static_cast<int16_t>(r), ic.scale, ic.offset);
        std::string s = std::to_string(val);
        (void)write_str(outJson, outSize, s);
      } else {
        std::string s = std::to_string(static_cast<unsigned>(r));
        (void)write_str(outJson, outSize, s);
      }
      return 0;
    } else {
      std::vector<std::uint16_t> rr(need);
      int rc = ctx->client->read_holding_regs(ic.unit_id, ic.address, need, rr.data());
      if (rc != 0) return rc;
      json arr = json::array();
      for (int i = 0; i < need; ++i) arr.push_back(rr[i]);
      return write_str(outJson, outSize, arr.dump()) ? 0 : 0;
    }
  }
  if (ic.function == 4) {
    int need = ic.count > 0 ? ic.count : 1; if (need < 1) need = 1;
    std::vector<std::uint16_t> rr(need);
    int rc = ctx->client->read_input_regs(ic.unit_id, ic.address, need, rr.data());
    if (rc != 0) return rc;
    if (ic.type == "float" || need == 2) {
      // interpret first 2 regs as float
      std::uint32_t u = wiq::join_u32(rr[0], rr.size() > 1 ? rr[1] : 0, ic.swap_words);
      float f;
      std::memcpy(&f, &u, 4);
      std::string s = std::to_string(static_cast<double>(f));
      (void)write_str(outJson, outSize, s);
      return 0;
    } else if (need == 1) {
      std::string s = std::to_string(static_cast<unsigned>(rr[0]));
      (void)write_str(outJson, outSize, s);
      return 0;
    } else {
      json arr = json::array();
      for (int i = 0; i < need; ++i) arr.push_back(rr[i]);
      return write_str(outJson, outSize, arr.dump()) ? 0 : 0;
    }
  }
  return static_cast<int>(wiq::ModbusErr::UNSUPPORTED);
}

WIQ_IOH_API int WriteItem(IoHandle h, const char* name, const char* valueJson) {
  auto* ctx = reinterpret_cast<wiq::IoContext*>(h);
  if (!ctx || !name || !valueJson) return static_cast<int>(wiq::ModbusErr::INVALID_ARG);
  auto it = ctx->items.find(name);
  if (it == ctx->items.end()) return static_cast<int>(wiq::ModbusErr::NOT_FOUND);
  const wiq::ItemCfg& ic = it->second;
  if (!ctx->client) return static_cast<int>(wiq::ModbusErr::NOT_CONNECTED);

  // parse simple JSON literal
  nlohmann::json v;
  try { v = nlohmann::json::parse(valueJson); } catch (...) { return static_cast<int>(wiq::ModbusErr::PARSE_ERROR); }

  if (ic.function == 5) { // single coil
    bool on = false;
    if (v.is_boolean()) on = v.get<bool>();
    else if (v.is_number_integer()) on = (v.get<int>() != 0);
    else return static_cast<int>(wiq::ModbusErr::PARSE_ERROR);
    return ctx->client->write_single_coil(ic.unit_id, ic.address, on);
  }
  if (ic.function == 15) { // multiple coils
    if (!v.is_array()) return static_cast<int>(wiq::ModbusErr::PARSE_ERROR);
    int count = static_cast<int>(v.size());
    if (count <= 0) return static_cast<int>(wiq::ModbusErr::INVALID_ARG);
    if (ic.count > 0 && count != ic.count) return static_cast<int>(wiq::ModbusErr::INVALID_ARG);
    std::vector<std::uint8_t> buf(count);
    for (int i = 0; i < count; ++i) {
      const auto& e = v[i];
      if (e.is_boolean()) buf[i] = e.get<bool>() ? 1 : 0;
      else if (e.is_number_integer()) buf[i] = (e.get<int>() != 0) ? 1 : 0;
      else return static_cast<int>(wiq::ModbusErr::PARSE_ERROR);
    }
    return ctx->client->write_multiple_coils(ic.unit_id, ic.address, count, buf.data());
  }
  if (ic.function == 6) { // single reg
    double d = 0.0; if (v.is_number()) d = v.get<double>(); else return static_cast<int>(wiq::ModbusErr::PARSE_ERROR);
    if (ic.scale == 0.0) return static_cast<int>(wiq::ModbusErr::PARSE_ERROR);
    double rawd = wiq::unscale(d, ic.scale, ic.offset);
    int32_t rawi = static_cast<int32_t>(llround(rawd));
    std::uint16_t reg = static_cast<std::uint16_t>(static_cast<int16_t>(rawi));
    return ctx->client->write_single_reg(ic.unit_id, ic.address, reg);
  }
  if (ic.function == 16 || (ic.function == 6 && ic.type == std::string("float"))) {
    // write multiple regs; for float accept single number -> 2 regs
    if (v.is_number()) {
      double dv = v.get<double>();
      if (!std::isfinite(dv)) return static_cast<int>(wiq::ModbusErr::PARSE_ERROR);
      float f = static_cast<float>(dv);
      std::uint32_t u; std::memcpy(&u, &f, 4);
      std::uint16_t hi, lo; wiq::split_u32(u, ic.swap_words, hi, lo);
      std::uint16_t rr[2] = {hi, lo};
      return ctx->client->write_multiple_regs(ic.unit_id, ic.address, 2, rr);
    } else if (v.is_array()) {
      int count = static_cast<int>(v.size()); if (count <= 0) return static_cast<int>(wiq::ModbusErr::INVALID_ARG);
      if (ic.count > 0 && count != ic.count) return static_cast<int>(wiq::ModbusErr::INVALID_ARG);
      std::vector<std::uint16_t> regs(count);
      for (int i = 0; i < count; ++i) {
        if (!v[i].is_number_integer()) return static_cast<int>(wiq::ModbusErr::PARSE_ERROR);
        long long x = v[i].get<long long>();
        if (x < 0 || x > 65535) return static_cast<int>(wiq::ModbusErr::PARSE_ERROR);
        regs[i] = static_cast<std::uint16_t>(x);
      }
      return ctx->client->write_multiple_regs(ic.unit_id, ic.address, count, regs.data());
    } else {
      return static_cast<int>(wiq::ModbusErr::PARSE_ERROR);
    }
  }
  return static_cast<int>(wiq::ModbusErr::UNSUPPORTED);
}

WIQ_IOH_API int CallMethod(IoHandle h, const char* method, const char* paramsJson, /*out*/char* outJson, int outSize) {
  (void)paramsJson;
  auto* ctx = reinterpret_cast<wiq::IoContext*>(h);
  if (!ctx || !method) return static_cast<int>(wiq::ModbusErr::INVALID_ARG);
  std::string m(method);
  if (m == "connection.reconnect") {
    if (ctx->client) {
      ctx->client->close();
      int rc = ctx->client->connect();
      if (rc != 0) return rc;
    }
    if (outJson) (void)std::snprintf(outJson, outSize, "{\"ok\":true}");
    return 0;
  }
  if (m == "logger.set") {
    if (outJson) (void)std::snprintf(outJson, outSize, "{\"ok\":true}");
    return 0;
  }
  return static_cast<int>(wiq::ModbusErr::UNSUPPORTED);
}

} // extern "C"
