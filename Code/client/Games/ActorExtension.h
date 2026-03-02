#pragma once

#include <Structs/ActionEvent.h>

struct ActorExtension
{
    enum
    {
        kRemote = 1 << 0,
        kPlayer = 1 << 1,
    };

    bool IsRemote() const noexcept;
    bool IsLocal() const noexcept;
    bool IsPlayer() const noexcept;
    bool IsRemotePlayer() const noexcept;
    bool IsLocalPlayer() const noexcept;
    void SetRemote(bool aSet) noexcept;
    void SetPlayer(bool aSet) noexcept;
    void SetNakedDeadline() noexcept { nakedDeadline = std::chrono::steady_clock::now();  }

    ActionEvent LatestAnimation{};
    size_t GraphDescriptorHash = 0;
    std::chrono::steady_clock::time_point nakedDeadline{};

  private:
    uint32_t onlineFlags{0};
};
