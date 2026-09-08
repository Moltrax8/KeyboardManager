#pragma once

#include "AudioEngine.hpp"
#include "BindEngine.hpp"
#include "ConfigStore.hpp"
#include "RawInputWindow.hpp"
#include "Types.hpp"

#include <Windows.h>
#include <d3d9.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <filesystem>
#include <optional>
#include <vector>

namespace km {

inline constexpr UINT WindowMessageTray = WM_APP + 1;
inline constexpr UINT WindowMessageRestore = WM_APP + 2;
inline constexpr UINT WindowMessageShutdown = WM_APP + 3;
inline constexpr wchar_t MainWindowClass[] = L"KeyboardManager.MainWindow";
inline constexpr wchar_t InstanceMutexName[] = L"KeyboardManager.Singleton";

class App final {
public:
    explicit App(HINSTANCE instance);
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    [[nodiscard]] bool initialize();
    int run();

private:
    static LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    [[nodiscard]] bool createMainWindow();
    [[nodiscard]] bool createGraphics();
    void destroyGraphics();
    void resizeGraphics(UINT width, UINT height);
    void render();
    void renderControls();
    void renderKeyboard();
    void renderActionEditor();
    void refreshSounds();
    void minimizeToTray();
    void restoreFromTray();
    void addTrayIcon();
    void removeTrayIcon();
    void showTrayMenu();
    void requestExit();
    void save();

    [[nodiscard]] Profile& activeProfile();
    [[nodiscard]] Binding* selectedBinding();
    [[nodiscard]] Binding& ensureSelectedBinding();
    [[nodiscard]] bool selectedKeyHasBinding(const KeyCode& key) const;
    void addAction(Action action);

    HINSTANCE instance_{};
    HWND window_{};
    UINT taskbarCreatedMessage_{};
    NOTIFYICONDATAW trayIcon_{};
    bool trayIconAdded_{};
    bool exitRequested_{};

    Microsoft::WRL::ComPtr<IDirect3D9> direct3D_;
    Microsoft::WRL::ComPtr<IDirect3DDevice9> device_;
    D3DPRESENT_PARAMETERS presentParameters_{};

    ConfigStore configStore_;
    Settings settings_;
    AudioEngine audio_;
    BindEngine bindEngine_;
    RawInputWindow rawInput_;
    std::optional<KeyCode> selectedKey_;
    std::uint8_t selectedModifiers_{ModifierNone};
    std::vector<std::filesystem::path> sounds_;
    int selectedSound_{};
    int selectedMediaKey_{};
    char newProfileName_[65]{};
};

} // namespace km
