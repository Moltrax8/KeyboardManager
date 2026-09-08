#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_WASAPI
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "AudioEngine.hpp"

#include "Paths.hpp"

#include <algorithm>
#include <utility>

namespace km {
namespace {

std::string stableDeviceId(const ma_device_id& id) {
    constexpr char Hex[] = "0123456789abcdef";
    const auto* bytes = reinterpret_cast<const unsigned char*>(&id);
    std::string result(sizeof(id) * 2U, '0');
    for (std::size_t index = 0; index < sizeof(id); ++index) {
        result[index * 2U] = Hex[bytes[index] >> 4U];
        result[index * 2U + 1U] = Hex[bytes[index] & 0x0FU];
    }
    return result;
}

} // namespace

struct AudioEngine::Impl {
    struct Device {
        AudioDeviceInfo info;
        ma_device_id id{};
    };

    ma_context context{};
    ma_engine engine{};
    bool contextReady{};
    bool engineReady{};
    float volume{0.8F};
    int selected{-1};
    std::vector<Device> deviceStorage;
    std::vector<AudioDeviceInfo> deviceViews;

    bool initializeEngine(const int index) noexcept {
        if (!contextReady) {
            return false;
        }
        if (index < -1 || (index >= 0 && static_cast<std::size_t>(index) >= deviceStorage.size())) {
            return false;
        }
        if (engineReady) {
            ma_engine_uninit(&engine);
            engineReady = false;
        }

        ma_engine_config config = ma_engine_config_init();
        config.pContext = &context;
        config.pPlaybackDeviceID = index >= 0 ? &deviceStorage[static_cast<std::size_t>(index)].id : nullptr;
        const ma_result result = ma_engine_init(&config, &engine);
        if (result != MA_SUCCESS) {
            logMessage(L"Audio output initialization failed.");
            return false;
        }
        engineReady = true;
        selected = index;
        ma_engine_set_volume(&engine, volume);
        return true;
    }
};

AudioEngine::AudioEngine() : impl_(std::make_unique<Impl>()) {
    if (ma_context_init(nullptr, 0, nullptr, &impl_->context) != MA_SUCCESS) {
        logMessage(L"Audio context initialization failed.");
        return;
    }
    impl_->contextReady = true;
    refreshDevices();
    static_cast<void>(impl_->initializeEngine(-1));
}

AudioEngine::~AudioEngine() {
    if (impl_->engineReady) {
        ma_engine_uninit(&impl_->engine);
    }
    if (impl_->contextReady) {
        ma_context_uninit(&impl_->context);
    }
}

bool AudioEngine::available() const noexcept {
    return impl_->engineReady;
}

const std::vector<AudioDeviceInfo>& AudioEngine::devices() const noexcept {
    return impl_->deviceViews;
}

int AudioEngine::selectedDevice() const noexcept {
    return impl_->selected;
}

std::string AudioEngine::selectedDeviceName() const {
    if (impl_->selected < 0 || static_cast<std::size_t>(impl_->selected) >= impl_->deviceStorage.size()) {
        return {};
    }
    return impl_->deviceStorage[static_cast<std::size_t>(impl_->selected)].info.name;
}

std::string AudioEngine::selectedDeviceId() const {
    if (impl_->selected < 0 || static_cast<std::size_t>(impl_->selected) >= impl_->deviceStorage.size()) {
        return {};
    }
    return impl_->deviceStorage[static_cast<std::size_t>(impl_->selected)].info.stableId;
}

void AudioEngine::setVolume(const float volume) noexcept {
    impl_->volume = std::clamp(volume, 0.0F, 1.0F);
    if (impl_->engineReady) {
        ma_engine_set_volume(&impl_->engine, impl_->volume);
    }
}

bool AudioEngine::selectDevice(const int index) noexcept {
    if (index < -1 || (index >= 0 && static_cast<std::size_t>(index) >= impl_->deviceStorage.size())) {
        return false;
    }
    const int previous = impl_->selected >= 0 &&
                                 static_cast<std::size_t>(impl_->selected) < impl_->deviceStorage.size()
                             ? impl_->selected
                             : -1;
    if (impl_->initializeEngine(index)) {
        return true;
    }
    // A failed route change must leave sound usable on the prior route whenever possible.
    static_cast<void>(impl_->initializeEngine(previous));
    return false;
}

bool AudioEngine::selectDevice(const std::string& stableId, const std::string& fallbackName) noexcept {
    if (stableId.empty() && fallbackName.empty()) {
        return selectDevice(-1);
    }
    auto iterator = std::find_if(impl_->deviceStorage.begin(), impl_->deviceStorage.end(),
                                 [&stableId](const Impl::Device& device) {
                                     return !stableId.empty() && device.info.stableId == stableId;
                                 });
    // Display names are only a migration fallback for configs created before stable IDs existed.
    if (iterator == impl_->deviceStorage.end() && stableId.empty() && !fallbackName.empty()) {
        iterator = std::find_if(impl_->deviceStorage.begin(), impl_->deviceStorage.end(),
                                [&fallbackName](const Impl::Device& device) {
                                    return device.info.name == fallbackName;
                                });
    }
    if (iterator == impl_->deviceStorage.end()) {
        logMessage(L"Saved audio device is unavailable; using the system default.");
        return selectDevice(-1);
    }
    return selectDevice(static_cast<int>(std::distance(impl_->deviceStorage.begin(), iterator)));
}

void AudioEngine::refreshDevices() noexcept {
    if (!impl_->contextReady) {
        return;
    }
    ma_device_info* playback = nullptr;
    ma_uint32 playbackCount = 0;
    if (ma_context_get_devices(&impl_->context, &playback, &playbackCount, nullptr, nullptr) != MA_SUCCESS) {
        logMessage(L"Audio device enumeration failed.");
        return;
    }

    impl_->deviceStorage.clear();
    impl_->deviceViews.clear();
    impl_->deviceStorage.reserve(playbackCount);
    impl_->deviceViews.reserve(playbackCount);
    for (ma_uint32 index = 0; index < playbackCount; ++index) {
        Impl::Device device;
        device.info.name = playback[index].name;
        device.info.stableId = stableDeviceId(playback[index].id);
        device.info.isDefault = playback[index].isDefault != MA_FALSE;
        device.id = playback[index].id;
        impl_->deviceViews.push_back(device.info);
        impl_->deviceStorage.push_back(std::move(device));
    }
    impl_->selected = -1;
}

bool AudioEngine::reloadSoundFiles() noexcept {
    const int selected = impl_->selected >= 0 &&
                                 static_cast<std::size_t>(impl_->selected) < impl_->deviceStorage.size()
                             ? impl_->selected
                             : -1;

    // Fire-and-forget sounds retain resource-manager references after playback. Rebuilding the
    // engine releases those references so a replaced file is decoded again even when its path is unchanged.
    if (impl_->initializeEngine(selected)) {
        return true;
    }
    if (selected >= 0) {
        static_cast<void>(impl_->initializeEngine(-1));
    }
    return false;
}

bool AudioEngine::play(const std::filesystem::path& path) noexcept {
    if (!impl_->engineReady) {
        return false;
    }
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
        logMessage(L"Sound file is missing: " + path.wstring());
        return false;
    }
    const auto utf8Path = toUtf8(path.wstring());
    if (utf8Path.empty() || ma_engine_play_sound(&impl_->engine, utf8Path.c_str(), nullptr) != MA_SUCCESS) {
        logMessage(L"Sound playback failed: " + path.wstring());
        return false;
    }
    return true;
}

} // namespace km
