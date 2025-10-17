#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>

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

int main() {
  // join/split roundtrip no swap
  {
    std::uint16_t hi = 0x1122, lo = 0x3344;
    std::uint32_t u = join_u32(hi, lo, false);
    std::uint16_t hi2=0, lo2=0; split_u32(u, false, hi2, lo2);
    assert(hi == hi2 && lo == lo2);
  }
  // join/split roundtrip with swap
  {
    std::uint16_t hi = 0xABCD, lo = 0x0123;
    std::uint32_t u = join_u32(hi, lo, true);
    std::uint16_t hi2=0, lo2=0; split_u32(u, true, hi2, lo2);
    assert(hi == hi2 && lo == lo2);
  }
  // float pack/unpack via join/split
  {
    float f = 3.14159f; std::uint32_t u; std::memcpy(&u, &f, 4);
    std::uint16_t hi, lo; split_u32(u, false, hi, lo);
    std::uint32_t u2 = join_u32(hi, lo, false); float f2; std::memcpy(&f2, &u2, 4);
    assert(std::abs(f - f2) < 1e-6f);
  }
  // float pack/unpack with swap_words=true roundtrip
  {
    float f = -7.25f; std::uint32_t u; std::memcpy(&u, &f, 4);
    std::uint16_t hi, lo; split_u32(u, true, hi, lo);
    std::uint32_t u2 = join_u32(hi, lo, true); float f2; std::memcpy(&f2, &u2, 4);
    assert(std::abs(f - f2) < 1e-6f);
  }
  // scale/unscale invert
  {
    double raw = -123.0, scale = 0.1, offset = 2.5;
    double sc = apply_scale(raw, scale, offset);
    double back = unscale(sc, scale, offset);
    assert(std::abs(raw - back) < 1e-9);
  }

  std::puts("unit_codec: ok");
  return 0;
}
