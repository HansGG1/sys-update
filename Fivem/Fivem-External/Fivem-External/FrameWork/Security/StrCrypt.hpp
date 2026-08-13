#pragma once
#include <cstdint>
#include <cstddef>
#include <type_traits>

// Custom compile-time string encryption.
// Uses Murmur3 finalizer mixing + per-site counter seed.
// Produces completely different binary patterns from jmXorStr (no SIMD, no FNV, no AVX2).

namespace SC {

    // Murmur3 64-bit finalizer — strong avalanche, different from jmXorStr's FNV
    [[nodiscard]] constexpr uint64_t mx(uint64_t x) noexcept {
        x ^= x >> 33;  x *= 0xff51afd7ed558ccdull;
        x ^= x >> 33;  x *= 0xc4ceb9fe1a85ec53ull;
        return x ^ (x >> 33);
    }

    // Build-time seed derived from __TIME__ + __DATE__
    [[nodiscard]] constexpr uint64_t bseed() noexcept {
        uint64_t h = 0x9e3779b97f4a7c15ull;
        for (unsigned char c : __TIME__ __DATE__)
            h = mx(h ^ (uint64_t)c);
        return h;
    }

    // Per-character key byte: unique per (global seed, call-site counter, char position)
    template<uint64_t S, uint64_t C>
    [[nodiscard]] constexpr uint8_t kb(size_t i) noexcept {
        return (uint8_t)(mx(mx(S ^ C) ^ ((uint64_t)i * 0x9e3779b97f4a7c15ull)) & 0xFF);
    }

    template<typename Ch, size_t N, uint64_t S, uint64_t C>
    struct Cipher {
        using UCh = std::make_unsigned_t<Ch>;
        Ch e[N]{};

        constexpr Cipher(const Ch* s) noexcept {
            for (size_t i = 0; i < N; ++i)
                e[i] = (Ch)((UCh)s[i] ^ (UCh)kb<S, C>(i));
        }

        // Decrypt once on first call, then return the cached buffer on every subsequent call.
        // O(N) cost only on first call — hot-path (render loop) cost is a single bool load + branch.
        __forceinline const Ch* get() const noexcept {
            static Ch  buf[N];
            static bool ready = false;
            if (!ready) {
                for (size_t i = 0; i < N; ++i)
                    buf[i] = (Ch)((UCh)e[i] ^ (UCh)kb<S, C>(i));
                ready = true;
            }
            return buf;
        }
    };

} // namespace SC

// Each expansion gets a unique C from __COUNTER__ so call sites never share a buffer.
#define _SC_SID(n) SC::mx(SC::bseed() ^ ((uint64_t)(n) * 0xbf58476d1ce4e5b9ull) ^ ((uint64_t)__LINE__ * 0x94d049bb133111ebull))
#define CStr(s)    (SC::Cipher<std::decay_t<decltype((s)[0])>, sizeof(s)/sizeof((s)[0]), SC::bseed(), _SC_SID(__COUNTER__)>(s).get())
