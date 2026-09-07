#pragma once

#include <Arduino.h>

enum class FanConnectionStatus : uint8_t {
    Disabled,
    Connecting,
    Online,
    Error,
};

struct FanControllerSnapshot {
    FanConnectionStatus status = FanConnectionStatus::Disabled;
    bool stateValid = false;
    bool power = false;
    uint8_t speed = 1;
    int8_t lastError = 0;
};

void fan_controller_begin();
void fan_controller_request_speed(uint8_t speed);
void fan_controller_request_turn_steps(int8_t steps);
void fan_controller_cancel_turns();
FanControllerSnapshot fan_controller_snapshot();
