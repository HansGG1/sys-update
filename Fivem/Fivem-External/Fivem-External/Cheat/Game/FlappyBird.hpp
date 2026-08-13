#pragma once
#include <atomic>

namespace FlappyBird
{
    void Run(std::atomic<bool>& cheatActivated);
    void Cleanup();
}
