#include "export.hpp"
#include "IModbusClient.hpp"
#include <nlohmann/json.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <fstream>
#include <cmath>
#include <set>
#include "log.hpp"
#include <thread>
#include <chrono>

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
      wiq::log::log_warn(__FILE__, __LINE__, "modbus_connect failed: %s", modbus_strerror(errno));
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
  std::string word_order{"ABCD"}; // for 64-bit (double): ABCD|BADC|CDAB|DCBA
};

struct IoContext {
  std::unordered_map<std::string, ItemCfg> items;
  std::unique_ptr<IModbusClient> client;
  // tcp config
  std::string host; int port{1502}; int timeout_ms{1000};
  // transport
  std::string transport; // tcp|rtu
  // reconnect policy
  int reconnect_retries{1};            // number of reconnect attempts upon NOT_CONNECTED
  int reconnect_interval_ms{0};        // base wait before each reconnect attempt
  double reconnect_backoff{1.0};       // multiplier applied after each attempt (>=1.0)
  int reconnect_max_interval_ms{0};    // optional cap; 0 means uncapped
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

static std::uint64_t join_u64_be(const std::uint16_t r[4]) {
  return (std::uint64_t(r[0]) << 48) | (std::uint64_t(r[1]) << 32) |
         (std::uint64_t(r[2]) << 16) | std::uint64_t(r[3]);
}

static void split_u64_be(std::uint64_t u, std::uint16_t out[4]) {
  out[0] = (u >> 48) & 0xFFFF;
  out[1] = (u >> 32) & 0xFFFF;
  out[2] = (u >> 16) & 0xFFFF;
  out[3] = u & 0xFFFF;
}

static void reorder_words4(const std::uint16_t in[4], const std::string& order, std::uint16_t out[4]) {
  auto pick = [&](int idx)->std::uint16_t { return in[idx]; };
  if (order == "ABCD") { out[0]=pick(0); out[1]=pick(1); out[2]=pick(2); out[3]=pick(3); return; }
  if (order == "BADC") { out[0]=pick(1); out[1]=pick(0); out[2]=pick(3); out[3]=pick(2); return; }
  if (order == "CDAB") { out[0]=pick(2); out[1]=pick(3); out[2]=pick(0); out[3]=pick(1); return; }
  if (order == "DCBA") { out[0]=pick(3); out[1]=pick(2); out[2]=pick(1); out[3]=pick(0); return; }
  // default
  out[0]=pick(0); out[1]=pick(1); out[2]=pick(2); out[3]=pick(3);
}

static double apply_scale(double raw, double scale, double offset) { return raw * scale + offset; }
static double unscale(double scaled, double scale, double offset) { return (scale == 0.0) ? 0.0 : (scaled - offset) / scale; }

// Helper: perform a Modbus client operation; if NOT_CONNECTED, try reconnect once and retry
static int call_with_reconnect(wiq::IoContext* ctx, const std::function<int()>& op) {
  int rc = op();
  if (!ctx || !ctx->client) return rc;
  const int NOT_CONNECTED_RC = static_cast<int>(wiq::ModbusErr::NOT_CONNECTED);
  if (rc != NOT_CONNECTED_RC) return rc;

  int attempts = ctx->reconnect_retries;
  if (attempts <= 0) return rc;

  using namespace std::chrono;
  double backoff = (ctx->reconnect_backoff < 1.0) ? 1.0 : ctx->reconnect_backoff;
  int wait_ms = (ctx->reconnect_interval_ms < 0) ? 0 : ctx->reconnect_interval_ms;
  int max_ms = (ctx->reconnect_max_interval_ms < 0) ? 0 : ctx->reconnect_max_interval_ms;

  wiq::log::log_debug(__FILE__, __LINE__,
                  "NOT_CONNECTED -> reconnect policy: retries=%d interval_ms=%d backoff=%.2f cap=%d",
                  attempts, wait_ms, backoff, max_ms);
  for (int attempt = 1; attempt <= attempts; ++attempt) {
    if (wait_ms > 0) {
      wiq::log::log_trace(__FILE__, __LINE__, "reconnect attempt %d/%d: sleep %d ms", attempt, attempts, wait_ms);
      std::this_thread::sleep_for(milliseconds(wait_ms));
    } else {
      wiq::log::log_trace(__FILE__, __LINE__, "reconnect attempt %d/%d: no wait", attempt, attempts);
    }
    (void)ctx->client->connect();
    rc = op();
    if (rc != NOT_CONNECTED_RC) {
      wiq::log::log_debug(__FILE__, __LINE__, "reconnect attempt %d/%d: operation returned %d", attempt, attempts, rc);
      return rc; // success or different error
    }

    // prepare next wait with backoff
    if (wait_ms > 0) {
      double next = wait_ms * backoff;
      if (next > static_cast<double>(INT32_MAX)) next = static_cast<double>(INT32_MAX);
      int next_ms = static_cast<int>(next);
      if (max_ms > 0 && next_ms > max_ms) next_ms = max_ms;
      wait_ms = next_ms;
    }
  }
  wiq::log::log_warn(__FILE__, __LINE__, "all reconnect attempts failed with NOT_CONNECTED");
  return rc;
}

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
  if (!jsonConfigPath) {
    wiq::log::log_error(__FILE__, __LINE__, "CreateIoInstance: jsonConfigPath=nullptr");
    return nullptr;
  }
  std::string text;
  if (!wiq::load_file(jsonConfigPath, text)) {
    wiq::log::log_error(__FILE__, __LINE__, "CreateIoInstance: cannot read config '%s'", jsonConfigPath);
    return nullptr;
  }
  nlohmann::json cfg;
  try { cfg = nlohmann::json::parse(text); }
  catch (...) {
    wiq::log::log_error(__FILE__, __LINE__, "CreateIoInstance: invalid JSON in '%s'", jsonConfigPath);
    return nullptr;
  }

  std::unique_ptr<wiq::IoContext> ctx(new wiq::IoContext());
  ctx->transport = cfg.value("transport", std::string("tcp"));
  if (!(ctx->transport == "tcp" || ctx->transport == "rtu")) {
    wiq::log::log_error(__FILE__, __LINE__, "invalid transport '%s' (expected tcp|rtu)", ctx->transport.c_str());
    return nullptr;
  }
  if (cfg.contains("tcp")) {
    auto t = cfg["tcp"]; ctx->host = t.value("host", std::string("127.0.0.1")); ctx->port = t.value("port", 1502); ctx->timeout_ms = t.value("timeout_ms", 1000);
    if (ctx->port <= 0 || ctx->port > 65535) return nullptr;
  }
  // reconnect policy (optional)
  if (cfg.contains("reconnect") && cfg["reconnect"].is_object()) {
    auto r = cfg["reconnect"];
    ctx->reconnect_retries = r.value("retries", ctx->reconnect_retries);
    ctx->reconnect_interval_ms = r.value("interval_ms", ctx->reconnect_interval_ms);
    ctx->reconnect_backoff = r.value("backoff_multiplier", ctx->reconnect_backoff);
    ctx->reconnect_max_interval_ms = r.value("max_interval_ms", ctx->reconnect_max_interval_ms);
  }

  ctx->client = wiq::make_client_for(ctx->transport, ctx->host, ctx->port);
  wiq::log::log_info(__FILE__, __LINE__,
                 "CreateIoInstance: transport=%s host=%s port=%d timeout_ms=%d retries=%d interval_ms=%d backoff=%.2f cap=%d",
                 ctx->transport.c_str(), ctx->host.c_str(), ctx->port, ctx->timeout_ms,
                 ctx->reconnect_retries, ctx->reconnect_interval_ms, ctx->reconnect_backoff, ctx->reconnect_max_interval_ms);
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
    ic.word_order = it.value("word_order", std::string("ABCD"));

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
    auto valid_word = [&](const std::string& w){ return w=="ABCD"||w=="BADC"||w=="CDAB"||w=="DCBA"; };
    if (!valid_word(ic.word_order)) return nullptr;
    // Enforce float/double counts if provided; set default if omitted
    if ((ic.function == 3 || ic.function == 4 || ic.function == 16) && ic.type == std::string("float")) {
      if (it.contains("count")) { if (ic.count != 2) return nullptr; } else { ic.count = 2; }
    }
    if ((ic.function == 3 || ic.function == 4 || ic.function == 16) && ic.type == std::string("double")) {
      if (it.contains("count")) { if (ic.count != 4) return nullptr; } else { ic.count = 4; }
    }
    if ((ic.type == std::string("double")) && ic.function == 6) return nullptr; // single reg not allowed for double

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
      std::uint8_t v = 0; int rc = call_with_reconnect(ctx, [&]{ return ctx->client->read_coils(ic.unit_id, ic.address, 1, &v); });
      if (rc != 0) return rc;
      return write_str(outJson, outSize, v ? "true" : "false") ? 0 : 0;
    } else {
      std::vector<std::uint8_t> buf(count);
      int rc = call_with_reconnect(ctx, [&]{ return ctx->client->read_coils(ic.unit_id, ic.address, count, buf.data()); });
      if (rc != 0) return rc;
      json arr = json::array();
      for (int i = 0; i < count; ++i) arr.push_back(buf[i] != 0);
      return write_str(outJson, outSize, arr.dump()) ? 0 : 0;
    }
  }
  if (ic.function == 2) {
    int count = ic.count > 0 ? ic.count : 1;
    if (count == 1) {
      std::uint8_t v = 0; int rc = call_with_reconnect(ctx, [&]{ return ctx->client->read_discrete_inputs(ic.unit_id, ic.address, 1, &v); });
      if (rc != 0) return rc;
      return write_str(outJson, outSize, v ? "true" : "false") ? 0 : 0;
    } else {
      std::vector<std::uint8_t> buf(count);
      int rc = call_with_reconnect(ctx, [&]{ return ctx->client->read_discrete_inputs(ic.unit_id, ic.address, count, buf.data()); });
      if (rc != 0) return rc;
      json arr = json::array();
      for (int i = 0; i < count; ++i) arr.push_back(buf[i] != 0);
      return write_str(outJson, outSize, arr.dump()) ? 0 : 0;
    }
  }
  if (ic.function == 3) {
    if (ic.type == "float") {
      std::uint16_t rr[2] = { static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(0) };
      int rc = call_with_reconnect(ctx, [&]{ return ctx->client->read_holding_regs(ic.unit_id, ic.address, 2, rr); });
      if (rc != 0) return rc;
      std::uint32_t u = wiq::join_u32(rr[0], rr[1], ic.swap_words);
      float f; std::memcpy(&f, &u, 4);
      std::string s = std::to_string(static_cast<double>(f));
      (void)write_str(outJson, outSize, s);
      return 0;
    } else if (ic.type == "double") {
      std::uint16_t rr_dev[4] = { static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(0), static_cast<std::uint16_t>(0) };
      int rc = call_with_reconnect(ctx, [&]{ return ctx->client->read_holding_regs(ic.unit_id, ic.address, 4, rr_dev); });
      if (rc != 0) return rc;
      std::uint16_t rr_be[4]; wiq::reorder_words4(rr_dev, ic.word_order, rr_be);
      std::uint64_t u = wiq::join_u64_be(rr_be);
      double d; std::memcpy(&d, &u, 8);
      std::string s = std::to_string(d);
      (void)write_str(outJson, outSize, s);
      return 0;
    }
    int need = ic.count > 0 ? ic.count : 1;
    if (need == 1) {
      std::uint16_t r = 0; int rc = call_with_reconnect(ctx, [&]{ return ctx->client->read_holding_regs(ic.unit_id, ic.address, 1, &r); });
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
      int rc = call_with_reconnect(ctx, [&]{ return ctx->client->read_holding_regs(ic.unit_id, ic.address, need, rr.data()); });
      if (rc != 0) return rc;
      json arr = json::array();
      for (int i = 0; i < need; ++i) arr.push_back(rr[i]);
      return write_str(outJson, outSize, arr.dump()) ? 0 : 0;
    }
  }
  if (ic.function == 4) {
    int need = ic.count > 0 ? ic.count : 1; if (need < 1) need = 1;
    std::vector<std::uint16_t> rr(need);
    int rc = call_with_reconnect(ctx, [&]{ return ctx->client->read_input_regs(ic.unit_id, ic.address, need, rr.data()); });
    if (rc != 0) return rc;
    if (ic.type == "double" && need >= 4) {
      std::uint16_t rr_be[4] = { rr[0], rr.size()>1?rr[1]:static_cast<std::uint16_t>(0), rr.size()>2?rr[2]:static_cast<std::uint16_t>(0), rr.size()>3?rr[3]:static_cast<std::uint16_t>(0) };
      // rr is device order already; convert to BE first
      std::uint16_t rr_dev[4] = { rr_be[0], rr_be[1], rr_be[2], rr_be[3] };
      std::uint16_t rr_be2[4]; wiq::reorder_words4(rr_dev, ic.word_order, rr_be2);
      std::uint64_t u = wiq::join_u64_be(rr_be2);
      double d; std::memcpy(&d, &u, 8);
      std::string s = std::to_string(d);
      (void)write_str(outJson, outSize, s);
      return 0;
    }
    if (ic.type == "float") {
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

  // Allow writes even when item is mapped with read FCs for convenience
  if (ic.type == std::string("bool") && (ic.function == 1 || ic.function == 2)) { // FC1/2 -> write as FC5
    bool on = false;
    if (v.is_boolean()) on = v.get<bool>();
    else if (v.is_number_integer()) on = (v.get<int>() != 0);
    else return static_cast<int>(wiq::ModbusErr::PARSE_ERROR);
    return call_with_reconnect(ctx, [&]{ return ctx->client->write_single_coil(ic.unit_id, ic.address, on); });
  }
  if (ic.function == 3) { // FC3 -> write as FC6/16 depending on type/count
    if (ic.type == std::string("float")) {
      if (!v.is_number()) return static_cast<int>(wiq::ModbusErr::PARSE_ERROR);
      float f = static_cast<float>(v.get<double>());
      std::uint32_t u; std::memcpy(&u, &f, 4);
      std::uint16_t hi, lo; wiq::split_u32(u, ic.swap_words, hi, lo);
      std::uint16_t rr[2] = {hi, lo};
      return call_with_reconnect(ctx, [&]{ return ctx->client->write_multiple_regs(ic.unit_id, ic.address, 2, rr); });
    } else { // int16 path
      double d = 0.0; if (v.is_number()) d = v.get<double>(); else return static_cast<int>(wiq::ModbusErr::PARSE_ERROR);
      if (ic.scale == 0.0) return static_cast<int>(wiq::ModbusErr::PARSE_ERROR);
      double rawd = wiq::unscale(d, ic.scale, ic.offset);
      int32_t rawi = static_cast<int32_t>(llround(rawd));
      std::uint16_t reg = static_cast<std::uint16_t>(static_cast<int16_t>(rawi));
      return call_with_reconnect(ctx, [&]{ return ctx->client->write_single_reg(ic.unit_id, ic.address, reg); });
    }
  }

  if (ic.function == 5) { // single coil
    bool on = false;
    if (v.is_boolean()) on = v.get<bool>();
    else if (v.is_number_integer()) on = (v.get<int>() != 0);
    else return static_cast<int>(wiq::ModbusErr::PARSE_ERROR);
    return call_with_reconnect(ctx, [&]{ return ctx->client->write_single_coil(ic.unit_id, ic.address, on); });
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
    return call_with_reconnect(ctx, [&]{ return ctx->client->write_multiple_coils(ic.unit_id, ic.address, count, buf.data()); });
  }
  if (ic.function == 6) { // single reg
    double d = 0.0; if (v.is_number()) d = v.get<double>(); else return static_cast<int>(wiq::ModbusErr::PARSE_ERROR);
    if (ic.scale == 0.0) return static_cast<int>(wiq::ModbusErr::PARSE_ERROR);
    double rawd = wiq::unscale(d, ic.scale, ic.offset);
    int32_t rawi = static_cast<int32_t>(llround(rawd));
    std::uint16_t reg = static_cast<std::uint16_t>(static_cast<int16_t>(rawi));
    return call_with_reconnect(ctx, [&]{ return ctx->client->write_single_reg(ic.unit_id, ic.address, reg); });
  }
  if (ic.function == 16 || (ic.function == 6 && ic.type == std::string("float"))) {
    // write multiple regs; for float accept single number -> 2 regs
    if (v.is_number()) {
      double dv = v.get<double>();
      if (!std::isfinite(dv)) return static_cast<int>(wiq::ModbusErr::PARSE_ERROR);
      if (ic.type == std::string("double")) {
        std::uint64_t u; std::memcpy(&u, &dv, 8);
        std::uint16_t rr_be[4]; wiq::split_u64_be(u, rr_be);
        std::uint16_t rr_dev[4]; wiq::reorder_words4(rr_be, ic.word_order, rr_dev);
        return call_with_reconnect(ctx, [&]{ return ctx->client->write_multiple_regs(ic.unit_id, ic.address, 4, rr_dev); });
      } else {
        float f = static_cast<float>(dv);
        std::uint32_t u; std::memcpy(&u, &f, 4);
        std::uint16_t hi, lo; wiq::split_u32(u, ic.swap_words, hi, lo);
        std::uint16_t rr[2] = {hi, lo};
        return call_with_reconnect(ctx, [&]{ return ctx->client->write_multiple_regs(ic.unit_id, ic.address, 2, rr); });
      }
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
      return call_with_reconnect(ctx, [&]{ return ctx->client->write_multiple_regs(ic.unit_id, ic.address, count, regs.data()); });
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
