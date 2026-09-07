#include "fan_controller.h"

#include <WiFi.h>

#include "miot/miio_transport.h"
#include "miot/xiaomi_fan_p69.h"

#if __has_include("secrets.h")
#include "secrets.h"
#define FAN_HAS_LOCAL_SECRETS 1
#else
#define FAN_HAS_LOCAL_SECRETS 0
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define FAN_IP ""
#define FAN_TOKEN ""
#endif

namespace {

constexpr uint32_t kSpeedCoalesceMs = 220;
constexpr uint32_t kDirectionIntervalMs = 300;
constexpr uint32_t kReconnectIntervalMs = 5000;
constexpr int8_t kMaxPendingTurnSteps = 24;

TaskHandle_t controllerTaskHandle = nullptr;
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
FanControllerSnapshot snapshot;
volatile int pendingSpeed = -1;
volatile int pendingTurnSteps = 0;

bool localConfigLooksValid()
{
    return FAN_HAS_LOCAL_SECRETS && strlen(WIFI_SSID) > 0 && strlen(FAN_IP) > 0 &&
           strlen(FAN_TOKEN) == 32 && strcmp(FAN_TOKEN, "PUT_32_HEX_TOKEN_HERE") != 0;
}

void setStatus(FanConnectionStatus status, MiioError error = MiioError::None)
{
    bool changed = false;
    portENTER_CRITICAL(&stateMux);
    changed = snapshot.status != status || snapshot.lastError != static_cast<int8_t>(error);
    snapshot.status = status;
    snapshot.lastError = static_cast<int8_t>(error);
    portEXIT_CRITICAL(&stateMux);
    if (changed) {
        Serial.printf("[fan] status=%u error=%u\n",
                      static_cast<unsigned>(status), static_cast<unsigned>(error));
    }
}

void updateState(const XiaomiFanP69State &state)
{
    portENTER_CRITICAL(&stateMux);
    snapshot.stateValid = true;
    snapshot.power = state.power;
    snapshot.speed = state.speed;
    snapshot.status = FanConnectionStatus::Online;
    snapshot.lastError = 0;
    portEXIT_CRITICAL(&stateMux);
    Serial.printf("[fan] online power=%u speed=%u\n", state.power, state.speed);
}

void controllerTask(void *)
{
    if (!localConfigLooksValid()) {
        setStatus(FanConnectionStatus::Disabled, MiioError::InvalidConfig);
        vTaskDelete(nullptr);
        return;
    }

    IPAddress fanIp;
    if (!fanIp.fromString(FAN_IP)) {
        setStatus(FanConnectionStatus::Disabled, MiioError::InvalidConfig);
        vTaskDelete(nullptr);
        return;
    }

    MiioTransport transport;
    XiaomiFanP69 fan(transport);
    uint32_t lastReconnectAttempt = 0;
    uint32_t lastSpeedRequestAt = 0;
    uint32_t lastDirectionAt = 0;
    bool transportReady = false;

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    setStatus(FanConnectionStatus::Connecting);

    for (;;) {
        const uint32_t now = millis();
        if (WiFi.status() != WL_CONNECTED) {
            transportReady = false;
            setStatus(FanConnectionStatus::Connecting);
            if (now - lastReconnectAttempt >= kReconnectIntervalMs) {
                lastReconnectAttempt = now;
                WiFi.disconnect();
                WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            }
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(250));
            continue;
        }

        if (!transportReady) {
            transportReady = transport.begin(fanIp, FAN_TOKEN);
            if (!transportReady) {
                setStatus(FanConnectionStatus::Error, MiioError::SocketError);
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            XiaomiFanP69State initialState;
            MiioResult result = fan.readState(initialState);
            if (result.ok()) {
                updateState(initialState);
            } else {
                setStatus(FanConnectionStatus::Error, result.error);
                transportReady = false;
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
        }

        int requestedSpeed = -1;
        int requestedTurns = 0;
        portENTER_CRITICAL(&stateMux);
        requestedSpeed = pendingSpeed;
        requestedTurns = pendingTurnSteps;
        portEXIT_CRITICAL(&stateMux);

        if (requestedSpeed >= 1 && now - lastSpeedRequestAt >= kSpeedCoalesceMs) {
            portENTER_CRITICAL(&stateMux);
            requestedSpeed = pendingSpeed;
            pendingSpeed = -1;
            portEXIT_CRITICAL(&stateMux);
            MiioResult result = fan.setSpeed(static_cast<uint8_t>(requestedSpeed));
            lastSpeedRequestAt = millis();
            if (result.ok()) {
                portENTER_CRITICAL(&stateMux);
                snapshot.speed = static_cast<uint8_t>(requestedSpeed);
                snapshot.stateValid = true;
                snapshot.status = FanConnectionStatus::Online;
                snapshot.lastError = 0;
                portEXIT_CRITICAL(&stateMux);
                Serial.printf("[fan] speed applied=%d\n", requestedSpeed);
            } else {
                setStatus(FanConnectionStatus::Error, result.error);
                transport.resetSession();
            }
        }

        // Direction commands are intentionally not consumed until physical step
        // size and a safe interval have been measured on the real fan.
        (void)requestedTurns;
        (void)lastDirectionAt;

        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
    }
}

} // namespace

void fan_controller_begin()
{
    if (controllerTaskHandle != nullptr) return;
    xTaskCreate(controllerTask, "fan_network", 8192, nullptr, 1, &controllerTaskHandle);
}

void fan_controller_request_speed(uint8_t speed)
{
    speed = constrain(speed, 1, 100);
    portENTER_CRITICAL(&stateMux);
    pendingSpeed = speed;
    portEXIT_CRITICAL(&stateMux);
    if (controllerTaskHandle != nullptr) xTaskNotifyGive(controllerTaskHandle);
}

void fan_controller_request_turn_steps(int8_t steps)
{
    portENTER_CRITICAL(&stateMux);
    pendingTurnSteps = constrain(pendingTurnSteps + steps,
                                 -kMaxPendingTurnSteps, kMaxPendingTurnSteps);
    portEXIT_CRITICAL(&stateMux);
    if (controllerTaskHandle != nullptr) xTaskNotifyGive(controllerTaskHandle);
}

void fan_controller_cancel_turns()
{
    portENTER_CRITICAL(&stateMux);
    pendingTurnSteps = 0;
    portEXIT_CRITICAL(&stateMux);
}

FanControllerSnapshot fan_controller_snapshot()
{
    FanControllerSnapshot copy;
    portENTER_CRITICAL(&stateMux);
    copy = snapshot;
    portEXIT_CRITICAL(&stateMux);
    return copy;
}
