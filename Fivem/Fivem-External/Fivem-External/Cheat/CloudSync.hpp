#pragma once
#include "Obfuscate.hpp"

namespace CloudSync {

    // Firebase host — decrypted once at first call, never stored plaintext in the binary.
    inline const wchar_t* GetHost() noexcept {
        static const std::wstring h = WOBF(L"hans-9e9e6-default-rtdb.firebaseio.com");
        return h.c_str();
    }

    // Reads cfg.dat next to the exe and sets per-user Firebase paths.
    // Call before Start().
    void Init();

    // Resets activate=false and shutdown=false in Firebase so panel shows correct state.
    // Call after Init(), before Start().
    void ResetSession();

    // Blocks until Firebase general/activate is true. Call before Initialize().
    void WaitForActivate();

    // Starts background thread that polls Firebase every 2 seconds
    // and updates g_Options with the latest settings.
    void Start();

    // Signals the polling thread to stop and waits for it to exit.
    void Stop();

    // Returns the install directory read from cfg.dat (with trailing backslash).
    const std::wstring& GetInstallDir() noexcept;

    // Returns the launcher task name read from cfg.dat.
    const std::wstring& GetTaskName() noexcept;

}
