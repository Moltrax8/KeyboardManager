#pragma once

#include <Windows.h>

namespace km {

class BindEngine;

class RawInputWindow final {
public:
    RawInputWindow() = default;
    ~RawInputWindow();

    RawInputWindow(const RawInputWindow&) = delete;
    RawInputWindow& operator=(const RawInputWindow&) = delete;

    [[nodiscard]] bool create(HINSTANCE instance, BindEngine& engine) noexcept;

private:
    static LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    void processInput(HRAWINPUT input) noexcept;

    HWND window_{};
    HINSTANCE instance_{};
    BindEngine* engine_{};
};

} // namespace km
