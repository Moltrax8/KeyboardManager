#include "KeyboardLayouts.hpp"

#include "Paths.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <format>

namespace km {
namespace {

KeySpec key(const std::uint16_t scanCode, const float width = 1.0F,
            const ExtendedFlag extended = ExtendedFlag::None) {
    return {{scanCode, extended}, width, false};
}

KeySpec gap(const float width = 0.5F) {
    return {{}, width, true};
}

KeyboardLayout fullSize() {
    return {
        {key(0x01), gap(), key(0x3B), key(0x3C), key(0x3D), key(0x3E), gap(),
         key(0x3F), key(0x40), key(0x41), key(0x42), gap(), key(0x43), key(0x44),
         key(0x57), key(0x58), gap(), key(0x37, 1.0F, ExtendedFlag::E0),
         key(0x46), key(0x45, 1.0F, ExtendedFlag::E1)},
        {key(0x29), key(0x02), key(0x03), key(0x04), key(0x05), key(0x06), key(0x07),
         key(0x08), key(0x09), key(0x0A), key(0x0B), key(0x0C), key(0x0D), key(0x0E, 2.0F),
         gap(), key(0x52, 1.0F, ExtendedFlag::E0), key(0x47, 1.0F, ExtendedFlag::E0),
         key(0x49, 1.0F, ExtendedFlag::E0), gap(), key(0x45), key(0x35, 1.0F, ExtendedFlag::E0),
         key(0x37), key(0x4A)},
        {key(0x0F, 1.5F), key(0x10), key(0x11), key(0x12), key(0x13), key(0x14), key(0x15),
         key(0x16), key(0x17), key(0x18), key(0x19), key(0x1A), key(0x1B), key(0x2B, 1.5F),
         gap(), key(0x53, 1.0F, ExtendedFlag::E0), key(0x4F, 1.0F, ExtendedFlag::E0),
         key(0x51, 1.0F, ExtendedFlag::E0), gap(), key(0x47), key(0x48), key(0x49), key(0x4E)},
        {key(0x3A, 1.75F), key(0x1E), key(0x1F), key(0x20), key(0x21), key(0x22),
         key(0x23), key(0x24), key(0x25), key(0x26), key(0x27), key(0x28), key(0x1C, 2.25F),
         gap(4.0F), key(0x4B), key(0x4C), key(0x4D), gap(1.0F)},
        {key(0x2A, 2.25F), key(0x2C), key(0x2D), key(0x2E), key(0x2F), key(0x30),
         key(0x31), key(0x32), key(0x33), key(0x34), key(0x35), key(0x36, 2.75F),
         gap(1.5F), key(0x48, 1.0F, ExtendedFlag::E0), gap(1.5F), key(0x4F), key(0x50),
         key(0x51), key(0x1C, 1.0F, ExtendedFlag::E0)},
        {key(0x1D, 1.25F), key(0x5B, 1.25F, ExtendedFlag::E0), key(0x38, 1.25F),
         key(0x39, 6.25F), key(0x38, 1.25F, ExtendedFlag::E0),
         key(0x5C, 1.25F, ExtendedFlag::E0), key(0x5D, 1.25F, ExtendedFlag::E0),
         key(0x1D, 1.25F, ExtendedFlag::E0), gap(), key(0x4B, 1.0F, ExtendedFlag::E0),
         key(0x50, 1.0F, ExtendedFlag::E0), key(0x4D, 1.0F, ExtendedFlag::E0),
         gap(), key(0x52, 2.0F), key(0x53)},
    };
}

KeyboardLayout tenkeyless() {
    auto result = fullSize();
    for (std::size_t rowIndex = 1; rowIndex < result.size(); ++rowIndex) {
        auto& row = result[rowIndex];
        if (rowIndex == result.size() - 1) {
            row.resize(row.size() - 3);
            continue;
        }
        const auto firstNumpadGap = std::find_if(row.rbegin(), row.rend(), [](const KeySpec& item) {
            return item.gap && item.width >= 1.5F;
        });
        if (firstNumpadGap != row.rend()) {
            row.erase(firstNumpadGap.base(), row.end());
        } else if (row.size() > 4) {
            row.resize(row.size() - 4);
        }
    }
    return result;
}

KeyboardLayout sixtyPercent() {
    return {
        {key(0x01), key(0x02), key(0x03), key(0x04), key(0x05), key(0x06), key(0x07),
         key(0x08), key(0x09), key(0x0A), key(0x0B), key(0x0C), key(0x0D), key(0x0E, 2.0F)},
        {key(0x0F, 1.5F), key(0x10), key(0x11), key(0x12), key(0x13), key(0x14), key(0x15),
         key(0x16), key(0x17), key(0x18), key(0x19), key(0x1A), key(0x1B), key(0x2B, 1.5F)},
        {key(0x3A, 1.75F), key(0x1E), key(0x1F), key(0x20), key(0x21), key(0x22),
         key(0x23), key(0x24), key(0x25), key(0x26), key(0x27), key(0x28), key(0x1C, 2.25F)},
        {key(0x2A, 2.25F), key(0x2C), key(0x2D), key(0x2E), key(0x2F), key(0x30),
         key(0x31), key(0x32), key(0x33), key(0x34), key(0x35), key(0x36, 2.75F)},
        {key(0x1D, 1.25F), key(0x5B, 1.25F, ExtendedFlag::E0), key(0x38, 1.25F),
         key(0x39, 6.25F), key(0x38, 1.25F, ExtendedFlag::E0),
         key(0x5D, 1.25F, ExtendedFlag::E0), key(0x1D, 1.25F, ExtendedFlag::E0)},
    };
}

} // namespace

const KeyboardLayout& keyboardLayout(const int size) {
    static const std::array<KeyboardLayout, 3> layouts{fullSize(), tenkeyless(), sixtyPercent()};
    return layouts[static_cast<std::size_t>(std::clamp(size, 0, 2))];
}

std::string localizedKeyName(const KeyCode& keyCode) {
    LONG parameter = static_cast<LONG>(keyCode.scanCode) << 16;
    if (keyCode.extended != ExtendedFlag::None) {
        parameter |= 1 << 24;
    }
    wchar_t name[64]{};
    if (GetKeyNameTextW(parameter, name, static_cast<int>(std::size(name))) > 0) {
        return toUtf8(name);
    }
    return std::format("SC {:02X}{}", keyCode.scanCode,
                       keyCode.extended == ExtendedFlag::E0 ? " E0" :
                       keyCode.extended == ExtendedFlag::E1 ? " E1" : "");
}

} // namespace km
