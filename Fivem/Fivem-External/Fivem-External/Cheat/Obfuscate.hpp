#pragma once
#include <string>

// ── Compile-time XOR string obfuscation ─────────────────────────────────────
//
//   OBF("text")   → std::string   (narrow)
//   WOBF(L"text") → std::wstring  (wide)
//
// How it works:
//   The macro creates a `constexpr static` struct inside a lambda.
//   The compiler evaluates the XOR at compile time and stores only the
//   scrambled bytes in the binary — the plaintext never appears.
//   .dec() runs at call-time, decrypts to a local std::string on the heap,
//   and the unscrambled bytes exist only for the duration of that expression.
// ────────────────────────────────────────────────────────────────────────────

namespace Obf {

static constexpr unsigned char KEY = 0x6E;

template<size_t N>
struct Str {
    char buf[N]{};
    constexpr Str(const char (&s)[N]) noexcept {
        for (size_t i = 0; i < N; ++i)
            buf[i] = static_cast<char>(s[i] ^ KEY);
    }
    [[nodiscard]] std::string dec() const noexcept {
        std::string r(N - 1, '\0');
        for (size_t i = 0; i < N - 1; ++i)
            r[i] = static_cast<char>(buf[i] ^ KEY);
        return r;
    }
};

template<size_t N>
struct WStr {
    wchar_t buf[N]{};
    constexpr WStr(const wchar_t (&s)[N]) noexcept {
        for (size_t i = 0; i < N; ++i)
            buf[i] = static_cast<wchar_t>(s[i] ^ KEY);
    }
    [[nodiscard]] std::wstring dec() const noexcept {
        std::wstring r(N - 1, L'\0');
        for (size_t i = 0; i < N - 1; ++i)
            r[i] = static_cast<wchar_t>(buf[i] ^ KEY);
        return r;
    }
};

} // namespace Obf

#define OBF(s)  ([]() noexcept { constexpr static Obf::Str <sizeof(s)>   _e(s); return _e.dec(); }())
#define WOBF(s) ([]() noexcept { constexpr static Obf::WStr<sizeof(s)/2> _e(s); return _e.dec(); }())
