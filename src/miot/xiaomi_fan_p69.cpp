#include "xiaomi_fan_p69.h"

namespace {

const char kStateParams[] =
    "[{\"did\":\"power\",\"siid\":2,\"piid\":1},"
    "{\"did\":\"mode\",\"siid\":2,\"piid\":3},"
    "{\"did\":\"speed\",\"siid\":2,\"piid\":5},"
    "{\"did\":\"horizontal_swing\",\"siid\":2,\"piid\":6},"
    "{\"did\":\"horizontal_angle\",\"siid\":2,\"piid\":7}]";

} // namespace

XiaomiFanP69::XiaomiFanP69(MiioTransport &transport) : transport_(transport) {}

bool XiaomiFanP69::readInt(const String &payload, const char *did, int &value) const
{
    String marker = String(F("\"did\":\"")) + did + '"';
    int position = payload.indexOf(marker);
    if (position < 0) return false;
    position = payload.indexOf(F("\"value\":"), position);
    if (position < 0) return false;
    position += 8;
    value = payload.substring(position).toInt();
    return true;
}

bool XiaomiFanP69::readBool(const String &payload, const char *did, bool &value) const
{
    String marker = String(F("\"did\":\"")) + did + '"';
    int position = payload.indexOf(marker);
    if (position < 0) return false;
    position = payload.indexOf(F("\"value\":"), position);
    if (position < 0) return false;
    position += 8;
    if (payload.startsWith(F("true"), position)) {
        value = true;
        return true;
    }
    if (payload.startsWith(F("false"), position)) {
        value = false;
        return true;
    }
    return false;
}

bool XiaomiFanP69::responseSucceeded(const String &payload) const
{
    return payload.indexOf(F("\"code\":0")) >= 0;
}

MiioResult XiaomiFanP69::readState(XiaomiFanP69State &state)
{
    MiioResult result = transport_.request(F("get_properties"), kStateParams);
    if (!result.ok()) return result;

    int mode = 0;
    int speed = 0;
    int horizontalAngle = 0;
    if (!readBool(result.payload, "power", state.power) ||
        !readInt(result.payload, "mode", mode) ||
        !readInt(result.payload, "speed", speed) ||
        !readBool(result.payload, "horizontal_swing", state.horizontalSwing) ||
        !readInt(result.payload, "horizontal_angle", horizontalAngle)) {
        return {MiioError::InvalidPacket, result.payload};
    }
    state.mode = static_cast<uint8_t>(mode);
    state.speed = static_cast<uint8_t>(constrain(speed, 1, 100));
    state.horizontalAngle = static_cast<uint8_t>(horizontalAngle);
    return result;
}

MiioResult XiaomiFanP69::setSpeed(uint8_t speed)
{
    speed = constrain(speed, 1, 100);
    String params = F("[{\"did\":\"set.2.5\",\"siid\":2,\"piid\":5,\"value\":");
    params += speed;
    params += F("}]");
    MiioResult result = transport_.request(F("set_properties"), params);
    if (result.ok() && !responseSucceeded(result.payload)) result.error = MiioError::DeviceError;
    return result;
}

MiioResult XiaomiFanP69::turnLeft()
{
    MiioResult result = transport_.request(
        F("action"), F("{\"did\":\"call.2.4\",\"siid\":2,\"aiid\":4,\"in\":[]}"));
    if (result.ok() && !responseSucceeded(result.payload)) result.error = MiioError::DeviceError;
    return result;
}

MiioResult XiaomiFanP69::turnRight()
{
    MiioResult result = transport_.request(
        F("action"), F("{\"did\":\"call.2.5\",\"siid\":2,\"aiid\":5,\"in\":[]}"));
    if (result.ok() && !responseSucceeded(result.payload)) result.error = MiioError::DeviceError;
    return result;
}
