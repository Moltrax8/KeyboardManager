#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace km {

struct AudioDeviceInfo {
    std::string name;
    std::string stableId;
    bool isDefault{};
};

class AudioEngine final {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] const std::vector<AudioDeviceInfo>& devices() const noexcept;
    [[nodiscard]] int selectedDevice() const noexcept;
    [[nodiscard]] std::string selectedDeviceName() const;
    [[nodiscard]] std::string selectedDeviceId() const;

    void setVolume(float volume) noexcept;
    [[nodiscard]] bool selectDevice(int index) noexcept;
    [[nodiscard]] bool selectDevice(const std::string& stableId,
                                    const std::string& fallbackName) noexcept;
    void refreshDevices() noexcept;
    [[nodiscard]] bool reloadSoundFiles() noexcept;
    [[nodiscard]] bool play(const std::filesystem::path& path) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace km
