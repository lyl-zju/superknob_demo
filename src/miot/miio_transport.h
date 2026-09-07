#pragma once

#include <Arduino.h>
#include <WiFiUdp.h>

enum class MiioError : uint8_t {
    None,
    InvalidConfig,
    SocketError,
    Timeout,
    InvalidPacket,
    ChecksumMismatch,
    CryptoError,
    DeviceError,
};

struct MiioResult {
    MiioError error;
    String payload;

    MiioResult(MiioError resultError = MiioError::None,
               const String &resultPayload = String())
        : error(resultError), payload(resultPayload) {}

    bool ok() const { return error == MiioError::None; }
};

class MiioTransport {
public:
    MiioTransport();

    bool begin(const IPAddress &deviceIp, const char *tokenHex);
    MiioResult request(const String &method, const String &paramsJson);
    void resetSession();

private:
    static constexpr uint16_t kPort = 54321;
    static constexpr size_t kHeaderSize = 32;
    static constexpr size_t kMaxPacketSize = 1024;

    bool decodeToken(const char *tokenHex);
    bool deriveKeyAndIv();
    bool ensureSession();
    bool hello();
    uint32_t currentDeviceTimestamp() const;
    MiioResult exchange(const String &json);
    bool encrypt(const uint8_t *plain, size_t plainLength,
                 uint8_t *cipher, size_t &cipherLength) const;
    bool decrypt(const uint8_t *cipher, size_t cipherLength,
                 uint8_t *plain, size_t &plainLength) const;
    void md5(const uint8_t *data, size_t length, uint8_t output[16]) const;

    WiFiUDP udp_;
    IPAddress deviceIp_;
    uint8_t token_[16]{};
    uint8_t key_[16]{};
    uint8_t iv_[16]{};
    uint8_t deviceId_[4]{};
    uint32_t deviceTimestamp_ = 0;
    uint32_t sessionMillis_ = 0;
    uint32_t requestId_ = 1;
    bool configured_ = false;
    bool socketStarted_ = false;
    bool sessionReady_ = false;
};
