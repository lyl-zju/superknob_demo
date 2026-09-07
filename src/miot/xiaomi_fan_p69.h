#pragma once

#include "miio_transport.h"

struct XiaomiFanP69State {
    bool power = false;
    uint8_t mode = 0;
    uint8_t speed = 1;
    bool horizontalSwing = false;
    uint8_t horizontalAngle = 120;
};

class XiaomiFanP69 {
public:
    explicit XiaomiFanP69(MiioTransport &transport);

    MiioResult readState(XiaomiFanP69State &state);
    MiioResult setSpeed(uint8_t speed);
    MiioResult turnLeft();
    MiioResult turnRight();

private:
    bool readBool(const String &payload, const char *did, bool &value) const;
    bool readInt(const String &payload, const char *did, int &value) const;
    bool responseSucceeded(const String &payload) const;

    MiioTransport &transport_;
};
