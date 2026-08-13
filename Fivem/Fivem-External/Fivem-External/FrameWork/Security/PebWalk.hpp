#pragma once
#include <intrin.h>
#include <cstdint>

// Fully self-contained PEB-based export resolver.
// Uses a multiply-xor-shift hash (Knuth multiplicative) — not FNV-1a, not the LazyImporter hash.
// Defines all required PE/PEB structures internally so this header has no Windows type dependencies.

namespace PW {

// ── Minimal PE/PEB layout structs (x64) ─────────────────────────────────────

struct PW_LE    { void* Flink; void* Blink; };
struct PW_US    { uint16_t Len; uint16_t MaxLen; uint32_t pad; wchar_t* Buf; }; // UNICODE_STRING x64
struct PW_DH    { uint16_t Magic; uint8_t pad[58]; int32_t lfanew; };           // IMAGE_DOS_HEADER (minimal)
struct PW_DD    { uint32_t VirtualAddress; uint32_t Size; };                    // IMAGE_DATA_DIRECTORY

struct PW_OH64 {                                   // IMAGE_OPTIONAL_HEADER64 (minimal)
    uint16_t Magic; uint8_t LinkerMajor; uint8_t LinkerMinor;
    uint32_t SizeOfCode; uint32_t SizeOfInitData; uint32_t SizeOfUninitData;
    uint32_t AddressOfEntryPoint; uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment; uint32_t FileAlignment;
    uint16_t OSMajor; uint16_t OSMinor; uint16_t ImageMajor; uint16_t ImageMinor;
    uint16_t SubMajor; uint16_t SubMinor;
    uint32_t Win32VersionValue; uint32_t SizeOfImage; uint32_t SizeOfHeaders;
    uint32_t CheckSum; uint16_t Subsystem; uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve; uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve; uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags; uint32_t NumberOfRvaAndSizes;
    PW_DD    DataDirectory[16];
};

struct PW_FH    { uint16_t Machine; uint16_t NumberOfSections; uint32_t TimeDateStamp;
                  uint32_t PointerToSymbolTable; uint32_t NumberOfSymbols;
                  uint16_t SizeOfOptionalHeader; uint16_t Characteristics; };

struct PW_NT64  { uint32_t Signature; PW_FH FileHeader; PW_OH64 OptionalHeader; };

struct PW_EXD   { uint32_t Characteristics; uint32_t TimeDateStamp; uint16_t MajorVersion;
                  uint16_t MinorVersion; uint32_t Name; uint32_t Base;
                  uint32_t NumberOfFunctions; uint32_t NumberOfNames;
                  uint32_t AddressOfFunctions; uint32_t AddressOfNames;
                  uint32_t AddressOfNameOrdinals; };

// ── Hash functions ───────────────────────────────────────────────────────────

// Case-sensitive hash for export names (exactly as they appear in the export directory)
[[nodiscard]] constexpr uint32_t fnhash(const char* s) noexcept {
    uint32_t h = 0xB1EAB1EAu;
    while (*s) {
        h ^= (uint8_t)*s++;
        h *= 0x45D9F3Bu;
        h ^= h >> 16;
    }
    return h;
}

// Case-insensitive hash for module names (BaseDllName in PEB LDR)
[[nodiscard]] constexpr uint32_t modhash(const wchar_t* s) noexcept {
    uint32_t h = 0xB1EAB1EAu;
    while (*s) {
        const wchar_t c = (*s >= L'A' && *s <= L'Z') ? (*s | 32u) : *s;
        ++s;
        h ^= (uint8_t)c;
        h *= 0x45D9F3Bu;
        h ^= h >> 16;
        h ^= (uint8_t)(c >> 8u);
        h *= 0x45D9F3Bu;
        h ^= h >> 16;
    }
    return h;
}

// ── PEB traversal ────────────────────────────────────────────────────────────

[[nodiscard]] inline void* get_module(uint32_t h) noexcept {
    // GS:[0x60] → PEB → Ldr (+0x18) → InLoadOrderModuleList (+0x10)
    const uint8_t* peb  = (const uint8_t*)__readgsqword(0x60);
    const uint8_t* ldr  = *(const uint8_t* const*)(peb + 0x18);
    const PW_LE*   head = (const PW_LE*)(ldr + 0x10);

    for (const PW_LE* e = (const PW_LE*)head->Flink; e != head; e = (const PW_LE*)e->Flink) {
        void*          base  = *(void**)((const uint8_t*)e + 0x30); // DllBase
        const PW_US*   uname = (const PW_US*)((const uint8_t*)e + 0x58); // BaseDllName
        if (!base || !uname->Buf || !uname->Len) continue;
        if (modhash(uname->Buf) == h) return base;
    }
    return nullptr;
}

[[nodiscard]] inline void* get_proc(void* mod, uint32_t h) noexcept {
    if (!mod) return nullptr;
    const uint8_t*  base = (const uint8_t*)mod;
    const PW_DH*    dos  = (const PW_DH*)base;
    const PW_NT64*  nt   = (const PW_NT64*)(base + dos->lfanew);
    const PW_DD&    dir  = nt->OptionalHeader.DataDirectory[0]; // IMAGE_DIRECTORY_ENTRY_EXPORT
    if (!dir.VirtualAddress) return nullptr;

    const PW_EXD*  exp   = (const PW_EXD*)(base + dir.VirtualAddress);
    const uint32_t* names = (const uint32_t*)(base + exp->AddressOfNames);
    const uint16_t* ords  = (const uint16_t*)(base + exp->AddressOfNameOrdinals);
    const uint32_t* funcs = (const uint32_t*)(base + exp->AddressOfFunctions);

    for (uint32_t i = 0; i < exp->NumberOfNames; ++i) {
        if (fnhash((const char*)(base + names[i])) != h) continue;
        uint32_t rva = funcs[ords[i]];
        if (rva >= dir.VirtualAddress && rva < dir.VirtualAddress + dir.Size) continue; // forwarded
        return (void*)(base + rva);
    }
    return nullptr;
}

// Walk all loaded modules and return the first module that exports a function matching h.
[[nodiscard]] inline void* find_any(uint32_t h) noexcept {
    const uint8_t* peb  = (const uint8_t*)__readgsqword(0x60);
    const uint8_t* ldr  = *(const uint8_t* const*)(peb + 0x18);
    const PW_LE*   head = (const PW_LE*)(ldr + 0x10);

    for (const PW_LE* e = (const PW_LE*)head->Flink; e != head; e = (const PW_LE*)e->Flink) {
        void* base = *(void**)((const uint8_t*)e + 0x30);
        void* fn   = get_proc(base, h);
        if (fn) return fn;
    }
    return nullptr;
}

} // namespace PW

// Resolves the export hash at compile time (optimizer constant-folds fnhash on string literals).
// decltype(&fn) gives the correct function-pointer type without importing fn into the IAT.
#define PW_CALL(fn) ((decltype(&fn))PW::find_any(PW::fnhash(#fn)))
