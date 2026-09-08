#pragma once

#include "Types.hpp"

namespace km {

class AudioEngine;

class ActionExecutor {
public:
    virtual ~ActionExecutor() = default;
    virtual void execute(const Action& action) noexcept = 0;
};

class WindowsActionExecutor final : public ActionExecutor {
public:
    explicit WindowsActionExecutor(AudioEngine& audio) noexcept;

    void execute(const Action& action) noexcept override;

private:
    AudioEngine& audio_;
};

} // namespace km
