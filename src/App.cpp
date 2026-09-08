#include "App.hpp"

#include "KeyboardLayouts.hpp"
#include "Paths.hpp"

#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>

#include <CommDlg.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <format>
#include <string>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message,
                                                             WPARAM wParam, LPARAM lParam);

namespace km {
namespace {

constexpr UINT TrayIconId = 1;
constexpr UINT CommandShow = 1001;
constexpr UINT CommandExit = 1002;

struct MediaKeyOption {
    const char* name;
    std::uint16_t virtualKey;
};

constexpr std::array<MediaKeyOption, 7> MediaKeys{{
    {"Play / Pause", VK_MEDIA_PLAY_PAUSE},
    {"Next Track", VK_MEDIA_NEXT_TRACK},
    {"Previous Track", VK_MEDIA_PREV_TRACK},
    {"Stop", VK_MEDIA_STOP},
    {"Volume Up", VK_VOLUME_UP},
    {"Volume Down", VK_VOLUME_DOWN},
    {"Mute", VK_VOLUME_MUTE},
}};

void modifierCheckbox(const char* label, const std::uint8_t bit, std::uint8_t& value) {
    bool enabled = (value & bit) != 0;
    if (ImGui::Checkbox(label, &enabled)) {
        value = enabled ? static_cast<std::uint8_t>(value | bit)
                        : static_cast<std::uint8_t>(value & ~bit);
    }
}

std::string actionDescription(const Action& action) {
    if (action.type == ActionType::PlaySound) {
        return "Sound: " + toUtf8(std::filesystem::path(action.path).filename().wstring());
    }
    if (action.type == ActionType::LaunchFile) {
        return "Open: " + toUtf8(std::filesystem::path(action.path).filename().wstring());
    }
    const auto item = std::find_if(MediaKeys.begin(), MediaKeys.end(), [&action](const MediaKeyOption& option) {
        return option.virtualKey == action.virtualKey;
    });
    return item == MediaKeys.end() ? std::format("Key: 0x{:02X}", action.virtualKey)
                                   : std::string("Media: ") + item->name;
}

std::string modifierDescription(const std::uint8_t modifiers) {
    if (modifiers == ModifierNone) {
        return "No modifiers";
    }
    std::string result;
    const auto append = [&result](const char* name) {
        if (!result.empty()) {
            result += " + ";
        }
        result += name;
    };
    if ((modifiers & ModifierCtrl) != 0) {
        append("Ctrl");
    }
    if ((modifiers & ModifierShift) != 0) {
        append("Shift");
    }
    if ((modifiers & ModifierAlt) != 0) {
        append("Alt");
    }
    if ((modifiers & ModifierWin) != 0) {
        append("Win");
    }
    return result;
}

} // namespace

App::App(const HINSTANCE instance)
    : instance_(instance), settings_(configStore_.load()), actionExecutor_(audio_),
      bindEngine_(settings_, actionExecutor_) {}

App::~App() {
    save();
    removeTrayIcon();
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    destroyGraphics();
    if (window_ != nullptr) {
        DestroyWindow(window_);
    }
    UnregisterClassW(MainWindowClass, instance_);
}

bool App::initialize() {
    if (!createMainWindow() || !createGraphics()) {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(22.0F, 18.0F);
    style.FramePadding = ImVec2(10.0F, 7.0F);
    style.CellPadding = ImVec2(10.0F, 7.0F);
    style.ItemSpacing = ImVec2(10.0F, 9.0F);
    style.ItemInnerSpacing = ImVec2(7.0F, 5.0F);
    style.WindowRounding = 0.0F;
    style.ChildRounding = 10.0F;
    style.FrameRounding = 7.0F;
    style.PopupRounding = 8.0F;
    style.ScrollbarRounding = 9.0F;
    style.GrabRounding = 7.0F;
    style.FrameBorderSize = 1.0F;
    style.ChildBorderSize = 1.0F;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.035F, 0.043F, 0.059F, 1.0F);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.055F, 0.067F, 0.087F, 1.0F);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.050F, 0.061F, 0.080F, 1.0F);
    style.Colors[ImGuiCol_Border] = ImVec4(0.15F, 0.19F, 0.24F, 1.0F);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.075F, 0.092F, 0.119F, 1.0F);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.10F, 0.14F, 0.18F, 1.0F);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.12F, 0.18F, 0.22F, 1.0F);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.035F, 0.043F, 0.059F, 1.0F);
    style.Colors[ImGuiCol_Header] = ImVec4(0.08F, 0.35F, 0.34F, 1.0F);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.10F, 0.46F, 0.43F, 1.0F);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.11F, 0.54F, 0.49F, 1.0F);
    style.Colors[ImGuiCol_Button] = ImVec4(0.075F, 0.11F, 0.14F, 1.0F);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.09F, 0.39F, 0.37F, 1.0F);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.10F, 0.49F, 0.45F, 1.0F);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.25F, 0.88F, 0.72F, 1.0F);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.18F, 0.66F, 0.58F, 1.0F);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.27F, 0.86F, 0.70F, 1.0F);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.13F, 0.18F, 0.22F, 1.0F);

    if (!ImGui_ImplWin32_Init(window_) || !ImGui_ImplDX9_Init(device_.Get())) {
        logMessage(L"Dear ImGui backend initialization failed.");
        return false;
    }
    if (!rawInput_.create(instance_, bindEngine_)) {
        return false;
    }

    audio_.setVolume(settings_.masterVolume);
    static_cast<void>(audio_.selectDevice(settings_.outputDeviceId, settings_.outputDeviceName));
    settings_.outputDeviceId = audio_.selectedDeviceId();
    settings_.outputDeviceName = audio_.selectedDeviceName();
    refreshSounds();
    taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");
    ShowWindow(window_, SW_SHOWDEFAULT);
    UpdateWindow(window_);
    return true;
}

int App::run() {
    MSG message{};
    while (!exitRequested_) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                exitRequested_ = true;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (exitRequested_) {
            break;
        }
        if (!IsWindowVisible(window_) || IsIconic(window_)) {
            WaitMessage();
            continue;
        }
        render();
    }
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK App::windowProcedure(const HWND window, const UINT message, const WPARAM wParam,
                                      const LPARAM lParam) {
    auto* self = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<App*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->window_ = window;
    }
    if (self != nullptr) {
        return self->handleMessage(window, message, wParam, lParam);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT App::handleMessage(const HWND window, const UINT message, const WPARAM wParam,
                           const LPARAM lParam) {
    if (message == taskbarCreatedMessage_ && taskbarCreatedMessage_ != 0) {
        trayIconAdded_ = false;
        if (!IsWindowVisible(window_)) {
            addTrayIcon();
            if (!trayIconAdded_) {
                logMessage(L"The tray icon could not be restored after Explorer restarted.");
                restoreFromTray();
            }
        }
        return 0;
    }
    if (ImGui::GetCurrentContext() != nullptr &&
        ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam)) {
        return 1;
    }

    switch (message) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) {
            minimizeToTray();
        } else if (device_ != nullptr) {
            resizeGraphics(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0U) == SC_MINIMIZE) {
            minimizeToTray();
            return 0;
        }
        break;
    case WM_CLOSE:
        minimizeToTray();
        return 0;
    case WM_QUERYENDSESSION:
        save();
        return TRUE;
    case WM_ENDSESSION:
        if (wParam != FALSE) {
            requestExit();
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == CommandShow) {
            restoreFromTray();
        } else if (LOWORD(wParam) == CommandExit) {
            requestExit();
        }
        return 0;
    case WindowMessageTray:
        if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            restoreFromTray();
        } else if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) {
            showTrayMenu();
        }
        return 0;
    case WindowMessageRestore:
        restoreFromTray();
        return 0;
    case WindowMessageShutdown:
        requestExit();
        return 0;
    case WM_DESTROY:
        window_ = nullptr;
        removeTrayIcon();
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool App::createMainWindow() {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_CLASSDC;
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.lpszClassName = MainWindowClass;
    if (RegisterClassExW(&windowClass) == 0) {
        logMessage(L"Could not register the main window class.");
        return false;
    }
    window_ = CreateWindowExW(0, MainWindowClass, L"KeyboardManager", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 1280, 820, nullptr, nullptr,
                              instance_, this);
    if (window_ == nullptr) {
        logMessage(L"Could not create the main window.");
        return false;
    }
    return true;
}

bool App::createGraphics() {
    direct3D_.Attach(Direct3DCreate9(D3D_SDK_VERSION));
    if (direct3D_ == nullptr) {
        logMessage(L"Direct3D 9 initialization failed.");
        return false;
    }

    presentParameters_ = {};
    presentParameters_.Windowed = TRUE;
    presentParameters_.SwapEffect = D3DSWAPEFFECT_DISCARD;
    presentParameters_.BackBufferFormat = D3DFMT_UNKNOWN;
    presentParameters_.EnableAutoDepthStencil = FALSE;
    presentParameters_.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    const HRESULT result = direct3D_->CreateDevice(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window_,
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
        &presentParameters_, &device_);
    if (FAILED(result)) {
        logMessage(L"Direct3D 9 device creation failed.");
        return false;
    }
    return true;
}

void App::destroyGraphics() {
    device_.Reset();
    direct3D_.Reset();
}

void App::resizeGraphics(const UINT width, const UINT height) {
    if (device_ == nullptr || width == 0 || height == 0 || ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    ImGui_ImplDX9_InvalidateDeviceObjects();
    presentParameters_.BackBufferWidth = width;
    presentParameters_.BackBufferHeight = height;
    if (FAILED(device_->Reset(&presentParameters_))) {
        logMessage(L"Direct3D device reset failed.");
        return;
    }
    ImGui_ImplDX9_CreateDeviceObjects();
}

void App::render() {
    if (device_ == nullptr) {
        return;
    }
    const HRESULT cooperativeLevel = device_->TestCooperativeLevel();
    if (cooperativeLevel == D3DERR_DEVICELOST) {
        Sleep(50);
        return;
    }
    if (cooperativeLevel == D3DERR_DEVICENOTRESET) {
        RECT client{};
        GetClientRect(window_, &client);
        resizeGraphics(static_cast<UINT>(client.right), static_cast<UINT>(client.bottom));
        return;
    }
    if (FAILED(cooperativeLevel)) {
        logMessage(L"The Direct3D device encountered an unrecoverable error.");
        requestExit();
        return;
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    renderControls();

    ImGui::Render();
    device_->SetRenderState(D3DRS_ZENABLE, FALSE);
    device_->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device_->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device_->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_RGBA(6, 8, 12, 255), 1.0F, 0);
    if (SUCCEEDED(device_->BeginScene())) {
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        device_->EndScene();
    }
    const HRESULT presentResult = device_->Present(nullptr, nullptr, nullptr, nullptr);
    if (FAILED(presentResult) && presentResult != D3DERR_DEVICELOST) {
        logMessage(L"Direct3D presentation failed; KeyboardManager is shutting down safely.");
        requestExit();
    }
}

void App::renderControls() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("KeyboardManager", nullptr,
                  ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                      ImGuiWindowFlags_NoSavedSettings);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.045F, 0.057F, 0.075F, 1.0F));
    ImGui::BeginChild("AppHeader", ImVec2(0.0F, 76.0F), ImGuiChildFlags_Borders);
    if (ImGui::BeginTable("HeaderLayout", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 150.0F);
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(0.30F, 0.92F, 0.76F, 1.0F), "KEYBOARD MANAGER");
        ImGui::TextDisabled("Physical-key automation and soundboard");
        ImGui::TableNextColumn();
        ImGui::TextDisabled("ENGINE STATUS");
        ImGui::TextColored(bindEngine_.paused() ? ImVec4(1.0F, 0.52F, 0.38F, 1.0F)
                                                : ImVec4(0.30F, 0.92F, 0.68F, 1.0F),
                           bindEngine_.paused() ? "PAUSED" : "LISTENING");
        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();
    const int selectedDeviceIndex = audio_.selectedDevice();
    const bool selectedDeviceValid = selectedDeviceIndex >= 0 &&
        static_cast<std::size_t>(selectedDeviceIndex) < audio_.devices().size();
    const std::string selectedDevice = selectedDeviceValid
                                           ? audio_.devices()[static_cast<std::size_t>(selectedDeviceIndex)].name
                                           : "System Default";

    ImGui::BeginChild("ControlCenter", ImVec2(0.0F, 190.0F), ImGuiChildFlags_Borders);
    if (ImGui::BeginTable("ControlGrid", 3,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableNextColumn();
        ImGui::TextDisabled("WORKSPACE");
        ImGui::TextUnformatted("Keyboard layout");
        const char* sizes[] = {"Full-size (104-key)", "TKL (87-key)", "60%"};
        ImGui::SetNextItemWidth(-1.0F);
        if (ImGui::Combo("##KeyboardSize", &settings_.keyboardSize, sizes, std::size(sizes))) {
            save();
        }
        ImGui::TextUnformatted("Active profile");
        ImGui::SetNextItemWidth(-1.0F);
        if (ImGui::BeginCombo("##Profile", settings_.activeProfile.c_str())) {
            for (const auto& profile : settings_.profiles) {
                const bool selected = profile.name == settings_.activeProfile;
                if (ImGui::Selectable(profile.name.c_str(), selected)) {
                    settings_.activeProfile = profile.name;
                    selectedKey_.reset();
                    save();
                }
            }
            ImGui::EndCombo();
        }
        const float profileButtonWidth =
            (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5F;
        if (ImGui::Button("New profile", ImVec2(profileButtonWidth, 0.0F))) {
            std::memset(newProfileName_, 0, sizeof(newProfileName_));
            ImGui::OpenPopup("Create profile");
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(settings_.profiles.size() <= 1);
        if (ImGui::Button("Delete", ImVec2(-1.0F, 0.0F))) {
            const auto profile = std::find_if(settings_.profiles.begin(), settings_.profiles.end(),
                                              [this](const Profile& item) {
                                                  return item.name == settings_.activeProfile;
                                              });
            if (profile != settings_.profiles.end()) {
                settings_.profiles.erase(profile);
                settings_.activeProfile = settings_.profiles.front().name;
                selectedKey_.reset();
                save();
            }
        }
        ImGui::EndDisabled();

        ImGui::TableNextColumn();
        ImGui::TextDisabled("AUDIO");
        ImGui::TextUnformatted("Master volume");
        ImGui::SetNextItemWidth(-1.0F);
        float volumePercent = settings_.masterVolume * 100.0F;
        if (ImGui::SliderFloat("##MasterVolume", &volumePercent, 0.0F, 100.0F, "%.0f%%")) {
            settings_.masterVolume = volumePercent / 100.0F;
            audio_.setVolume(settings_.masterVolume);
            save();
        }
        ImGui::TextUnformatted("Output device");
        ImGui::SetNextItemWidth(-1.0F);
        if (ImGui::BeginCombo("##OutputDevice", selectedDevice.c_str())) {
            if (ImGui::Selectable("System Default", audio_.selectedDevice() < 0) && audio_.selectDevice(-1)) {
                settings_.outputDeviceId.clear();
                settings_.outputDeviceName.clear();
                save();
            }
            for (std::size_t index = 0; index < audio_.devices().size(); ++index) {
                const auto& device = audio_.devices()[index];
                std::string label = device.name + (device.isDefault ? " (default)" : "");
                if (ImGui::Selectable(label.c_str(), audio_.selectedDevice() == static_cast<int>(index)) &&
                    audio_.selectDevice(static_cast<int>(index))) {
                    settings_.outputDeviceId = audio_.selectedDeviceId();
                    settings_.outputDeviceName = audio_.selectedDeviceName();
                    save();
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Refresh output devices", ImVec2(-1.0F, 0.0F))) {
            const std::string currentId = audio_.selectedDeviceId();
            const std::string currentName = audio_.selectedDeviceName();
            audio_.refreshDevices();
            static_cast<void>(audio_.selectDevice(currentId, currentName));
            settings_.outputDeviceId = audio_.selectedDeviceId();
            settings_.outputDeviceName = audio_.selectedDeviceName();
            save();
        }

        ImGui::TableNextColumn();
        ImGui::TextDisabled("SESSION");
        ImGui::Text("%zu sound%s ready", sounds_.size(), sounds_.size() == 1 ? "" : "s");
        ImGui::TextDisabled("%zu profile%s available", settings_.profiles.size(),
                            settings_.profiles.size() == 1 ? "" : "s");
        if (ImGui::Button(bindEngine_.paused() ? "Resume all binds" : "Pause all binds",
                          ImVec2(-1.0F, 0.0F))) {
            bindEngine_.setPaused(!bindEngine_.paused());
        }
        if (ImGui::Button("Minimize to tray", ImVec2(-1.0F, 0.0F))) {
            minimizeToTray();
        }
        ImGui::TextWrapped("Ctrl + Alt + P toggles all binds. Physical input always passes through.");
        ImGui::EndTable();
    }
    ImGui::EndChild();

    if (ImGui::BeginPopupModal("Create profile", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", newProfileName_, sizeof(newProfileName_));
        const bool duplicate = std::any_of(settings_.profiles.begin(), settings_.profiles.end(),
                                            [this](const Profile& profile) {
                                                return profile.name == newProfileName_;
                                            });
        ImGui::BeginDisabled(newProfileName_[0] == '\0' || duplicate);
        if (ImGui::Button("Create")) {
            settings_.profiles.push_back({newProfileName_, {}});
            settings_.activeProfile = newProfileName_;
            save();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    renderKeyboard();
    renderActionEditor();
    ImGui::End();
}

void App::renderKeyboard() {
    ImGui::SeparatorText("Keyboard canvas");
    ImGui::TextDisabled("Bound keys are teal. The selected key is amber.");
    const auto& layout = keyboardLayout(settings_.keyboardSize);
    constexpr float canvasPadding = 10.0F;
    constexpr float keySpacing = 4.0F;
    constexpr float rowSpacing = 5.0F;
    constexpr float maximumUnit = 40.0F;
    const float borderWidth = ImGui::GetStyle().ChildBorderSize * 2.0F;

    const float availableWidth = std::max(
        1.0F, ImGui::GetContentRegionAvail().x - canvasPadding * 2.0F - borderWidth);
    float unit = maximumUnit;
    for (const auto& row : layout) {
        float rowUnits = 0.0F;
        for (const auto& item : row) {
            rowUnits += item.width;
        }
        const float spacingWidth = keySpacing * static_cast<float>(row.empty() ? 0 : row.size() - 1);
        unit = std::min(unit, std::max(1.0F, availableWidth - spacingWidth) / rowUnits);
    }
    const float keyHeight = std::clamp(unit * 0.86F, 22.0F, 35.0F);
    const float canvasHeight = canvasPadding * 2.0F + borderWidth +
        keyHeight * static_cast<float>(layout.size()) +
        rowSpacing * static_cast<float>(layout.empty() ? 0 : layout.size() - 1);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(canvasPadding, canvasPadding));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(keySpacing, rowSpacing));
    ImGui::BeginChild("KeyboardLayout", ImVec2(0.0F, canvasHeight),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    for (const auto& row : layout) {
        bool first = true;
        for (const auto& item : row) {
            if (!first) {
                ImGui::SameLine(0.0F, keySpacing);
            }
            first = false;
            if (item.gap) {
                ImGui::Dummy(ImVec2(unit * item.width, keyHeight));
                continue;
            }

            const bool active = selectedKeyHasBinding(item.key);
            const bool selected = selectedKey_.has_value() && *selectedKey_ == item.key;
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10F, 0.46F, 0.38F, 1.0F));
            }
            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72F, 0.39F, 0.12F, 1.0F));
            }
            const std::string identifier = localizedKeyName(item.key) +
                std::format("##{}_{}", item.key.scanCode, static_cast<int>(item.key.extended));
            const bool clicked = ImGui::Button(identifier.c_str(), ImVec2(unit * item.width, keyHeight));
            const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal);
            if (clicked) {
                selectedKey_ = item.key;
                selectedModifiers_ = ModifierNone;
            }
            if (selected) {
                ImGui::PopStyleColor();
            }
            if (active) {
                ImGui::PopStyleColor();
            }
            if (active && hovered && ImGui::BeginTooltip()) {
                ImGui::Text("Assignments for %s", localizedKeyName(item.key).c_str());
                for (const auto& binding : activeProfile().bindings) {
                    if (binding.key != item.key || binding.actions.empty()) {
                        continue;
                    }
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.30F, 0.92F, 0.76F, 1.0F), "%s",
                                       modifierDescription(binding.modifiers).c_str());
                    for (const auto& action : binding.actions) {
                        ImGui::BulletText("%s", actionDescription(action).c_str());
                    }
                }
                ImGui::EndTooltip();
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    if (const auto lastPressed = bindEngine_.lastPressedKey()) {
        ImGui::TextDisabled("Last Raw Input key: %s (scan 0x%02X%s)",
                            localizedKeyName(*lastPressed).c_str(), lastPressed->scanCode,
                            lastPressed->extended == ExtendedFlag::E0 ? " E0" :
                            lastPressed->extended == ExtendedFlag::E1 ? " E1" : "");
        ImGui::SameLine();
        if (ImGui::SmallButton("Select last hardware key")) {
            selectedKey_ = *lastPressed;
            selectedModifiers_ = ModifierNone;
        }
    }
}

void App::renderActionEditor() {
    ImGui::SeparatorText("Binding workspace");
    if (!selectedKey_.has_value()) {
        ImGui::TextDisabled("Select a key above to assign one or more actions.");
        return;
    }

    ImGui::TextColored(ImVec4(0.98F, 0.67F, 0.28F, 1.0F), "SELECTED KEY  %s",
                       localizedKeyName(*selectedKey_).c_str());
    ImGui::SameLine();
    modifierCheckbox("Ctrl", ModifierCtrl, selectedModifiers_);
    ImGui::SameLine();
    modifierCheckbox("Shift", ModifierShift, selectedModifiers_);
    ImGui::SameLine();
    modifierCheckbox("Alt", ModifierAlt, selectedModifiers_);
    ImGui::SameLine();
    modifierCheckbox("Win", ModifierWin, selectedModifiers_);

    Binding* binding = selectedBinding();
    if (binding == nullptr) {
        ImGui::TextDisabled("No actions for this key combination.");
    } else {
        int removeIndex = -1;
        for (std::size_t index = 0; index < binding->actions.size(); ++index) {
            ImGui::PushID(static_cast<int>(index));
            ImGui::BulletText("%s", actionDescription(binding->actions[index]).c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) {
                removeIndex = static_cast<int>(index);
            }
            ImGui::PopID();
        }
        if (removeIndex >= 0) {
            binding->actions.erase(binding->actions.begin() + removeIndex);
            if (binding->actions.empty()) {
                auto& bindings = activeProfile().bindings;
                bindings.erase(std::remove_if(bindings.begin(), bindings.end(), [this](const Binding& item) {
                    return item.key == *selectedKey_ && item.modifiers == selectedModifiers_;
                }), bindings.end());
            }
            save();
        }
    }

    ImGui::Spacing();
    ImGui::SetNextItemWidth(220.0F);
    const std::string soundPreview = sounds_.empty()
                                         ? "No audio files in Sounds"
                                         : toUtf8(sounds_[static_cast<std::size_t>(selectedSound_)]
                                                      .lexically_relative(soundsDirectory()).wstring());
    if (ImGui::BeginCombo("Sound", soundPreview.c_str())) {
        for (std::size_t index = 0; index < sounds_.size(); ++index) {
            const std::string label = toUtf8(sounds_[index].lexically_relative(soundsDirectory()).wstring());
            if (ImGui::Selectable(label.c_str(), selectedSound_ == static_cast<int>(index))) {
                selectedSound_ = static_cast<int>(index);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(sounds_.empty());
    if (ImGui::Button("Add sound")) {
        addAction({ActionType::PlaySound,
                   sounds_[static_cast<std::size_t>(selectedSound_)]
                       .lexically_relative(soundsDirectory()).wstring(),
                   0});
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Rescan sounds")) {
        if (!audio_.reloadSoundFiles()) {
            settings_.outputDeviceId = audio_.selectedDeviceId();
            settings_.outputDeviceName = audio_.selectedDeviceName();
            save();
        }
        refreshSounds();
    }

    ImGui::SameLine();
    if (ImGui::Button("Add file / app")) {
        std::array<wchar_t, 32768> path{};
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = window_;
        dialog.lpstrFile = path.data();
        dialog.nMaxFile = static_cast<DWORD>(path.size());
        dialog.lpstrFilter = L"All files\0*.*\0";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT;
        if (GetOpenFileNameW(&dialog)) {
            addAction({ActionType::LaunchFile, path.data(), 0});
        }
    }

    ImGui::SetNextItemWidth(180.0F);
    if (ImGui::BeginCombo("Media action", MediaKeys[static_cast<std::size_t>(selectedMediaKey_)].name)) {
        for (std::size_t index = 0; index < MediaKeys.size(); ++index) {
            if (ImGui::Selectable(MediaKeys[index].name, selectedMediaKey_ == static_cast<int>(index))) {
                selectedMediaKey_ = static_cast<int>(index);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add media key")) {
        addAction({ActionType::SendKey, {}, MediaKeys[static_cast<std::size_t>(selectedMediaKey_)].virtualKey});
    }
}

void App::refreshSounds() {
    sounds_.clear();
    std::error_code error;
    std::filesystem::create_directories(soundsDirectory(), error);
    for (std::filesystem::recursive_directory_iterator iterator(soundsDirectory(), error), end;
         !error && iterator != end; iterator.increment(error)) {
        if (!iterator->is_regular_file(error)) {
            continue;
        }
        std::wstring extension = iterator->path().extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](const wchar_t value) {
            return static_cast<wchar_t>(towlower(value));
        });
        if (extension == L".wav" || extension == L".mp3" || extension == L".flac") {
            sounds_.push_back(iterator->path());
        }
    }
    std::sort(sounds_.begin(), sounds_.end());
    selectedSound_ = 0;
}

void App::minimizeToTray() {
    addTrayIcon();
    if (trayIconAdded_) {
        ShowWindow(window_, SW_HIDE);
        SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
    } else {
        logMessage(L"The tray icon could not be created; the window was kept visible.");
    }
}

void App::restoreFromTray() {
    removeTrayIcon();
    ShowWindow(window_, SW_RESTORE);
    SetForegroundWindow(window_);
}

void App::addTrayIcon() {
    if (trayIconAdded_ || window_ == nullptr) {
        return;
    }
    trayIcon_ = {};
    trayIcon_.cbSize = sizeof(trayIcon_);
    trayIcon_.hWnd = window_;
    trayIcon_.uID = TrayIconId;
    trayIcon_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    trayIcon_.uCallbackMessage = WindowMessageTray;
    trayIcon_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(trayIcon_.szTip, L"KeyboardManager");
    trayIconAdded_ = Shell_NotifyIconW(NIM_ADD, &trayIcon_) != FALSE;
    if (trayIconAdded_) {
        trayIcon_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &trayIcon_);
    }
}

void App::removeTrayIcon() {
    if (trayIconAdded_) {
        Shell_NotifyIconW(NIM_DELETE, &trayIcon_);
        trayIconAdded_ = false;
    }
}

void App::showTrayMenu() {
    POINT cursor{};
    GetCursorPos(&cursor);
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }
    AppendMenuW(menu, MF_STRING, CommandShow, L"Open KeyboardManager");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CommandExit, L"Exit");
    SetForegroundWindow(window_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
                   cursor.x, cursor.y, 0, window_, nullptr);
    DestroyMenu(menu);
}

void App::requestExit() {
    save();
    exitRequested_ = true;
    removeTrayIcon();
    if (window_ != nullptr) {
        DestroyWindow(window_);
    }
}

void App::save() {
    static_cast<void>(configStore_.save(settings_));
}

Profile& App::activeProfile() {
    const auto profile = std::find_if(settings_.profiles.begin(), settings_.profiles.end(),
                                      [this](const Profile& item) {
                                          return item.name == settings_.activeProfile;
                                      });
    return profile == settings_.profiles.end() ? settings_.profiles.front() : *profile;
}

Binding* App::selectedBinding() {
    if (!selectedKey_.has_value()) {
        return nullptr;
    }
    auto& bindings = activeProfile().bindings;
    const auto binding = std::find_if(bindings.begin(), bindings.end(), [this](const Binding& item) {
        return item.key == *selectedKey_ && item.modifiers == selectedModifiers_;
    });
    return binding == bindings.end() ? nullptr : &*binding;
}

Binding& App::ensureSelectedBinding() {
    if (Binding* binding = selectedBinding()) {
        return *binding;
    }
    auto& bindings = activeProfile().bindings;
    bindings.push_back({*selectedKey_, selectedModifiers_, {}});
    return bindings.back();
}

bool App::selectedKeyHasBinding(const KeyCode& keyCode) const {
    const auto profile = std::find_if(settings_.profiles.begin(), settings_.profiles.end(),
                                      [this](const Profile& item) {
                                          return item.name == settings_.activeProfile;
                                      });
    if (profile == settings_.profiles.end()) {
        return false;
    }
    return std::any_of(profile->bindings.begin(), profile->bindings.end(), [&keyCode](const Binding& binding) {
        return binding.key == keyCode && !binding.actions.empty();
    });
}

void App::addAction(Action action) {
    ensureSelectedBinding().actions.push_back(std::move(action));
    save();
}

} // namespace km
