#include "miio_transport.h"

#include <mbedtls/aes.h>
#include <mbedtls/md5.h>

namespace {

uint16_t readBe16(const uint8_t *data)
{
    return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}

uint32_t readBe32(const uint8_t *data)
{
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

void writeBe16(uint8_t *data, uint16_t value)
{
    data[0] = static_cast<uint8_t>(value >> 8);
    data[1] = static_cast<uint8_t>(value);
}

void writeBe32(uint8_t *data, uint32_t value)
{
    data[0] = static_cast<uint8_t>(value >> 24);
    data[1] = static_cast<uint8_t>(value >> 16);
    data[2] = static_cast<uint8_t>(value >> 8);
    data[3] = static_cast<uint8_t>(value);
}

int hexValue(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

} // namespace

MiioTransport::MiioTransport() = default;

bool MiioTransport::begin(const IPAddress &deviceIp, const char *tokenHex)
{
    deviceIp_ = deviceIp;
    configured_ = decodeToken(tokenHex) && deriveKeyAndIv();
    if (!configured_) return false;

    if (!socketStarted_) {
        socketStarted_ = udp_.begin(0);
    }
    resetSession();
    return socketStarted_;
}

void MiioTransport::resetSession()
{
    sessionReady_ = false;
    deviceTimestamp_ = 0;
    sessionMillis_ = 0;
}

bool MiioTransport::decodeToken(const char *tokenHex)
{
    if (tokenHex == nullptr || strlen(tokenHex) != 32) return false;
    for (size_t i = 0; i < sizeof(token_); ++i) {
        const int high = hexValue(tokenHex[i * 2]);
        const int low = hexValue(tokenHex[i * 2 + 1]);
        if (high < 0 || low < 0) return false;
        token_[i] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}

bool MiioTransport::deriveKeyAndIv()
{
    md5(token_, sizeof(token_), key_);
    uint8_t input[32];
    memcpy(input, key_, sizeof(key_));
    memcpy(input + sizeof(key_), token_, sizeof(token_));
    md5(input, sizeof(input), iv_);
    return true;
}

void MiioTransport::md5(const uint8_t *data, size_t length, uint8_t output[16]) const
{
    mbedtls_md5_ret(data, length, output);
}

bool MiioTransport::ensureSession()
{
    if (!configured_ || !socketStarted_) return false;
    if (sessionReady_ && millis() - sessionMillis_ < 30000UL) return true;
    return hello();
}

bool MiioTransport::hello()
{
    uint8_t helloPacket[kHeaderSize];
    memset(helloPacket, 0xFF, sizeof(helloPacket));
    helloPacket[0] = 0x21;
    helloPacket[1] = 0x31;
    helloPacket[2] = 0x00;
    helloPacket[3] = 0x20;

    while (udp_.parsePacket() > 0) {
        uint8_t discard[kMaxPacketSize];
        udp_.read(discard, sizeof(discard));
    }

    if (!udp_.beginPacket(deviceIp_, kPort) ||
        udp_.write(helloPacket, sizeof(helloPacket)) != sizeof(helloPacket) ||
        !udp_.endPacket()) {
        return false;
    }

    const uint32_t deadline = millis() + 1500;
    while (static_cast<int32_t>(deadline - millis()) > 0) {
        const int packetSize = udp_.parsePacket();
        if (packetSize >= static_cast<int>(kHeaderSize)) {
            uint8_t response[kMaxPacketSize];
            const int length = udp_.read(response, sizeof(response));
            if (length >= static_cast<int>(kHeaderSize) &&
                response[0] == 0x21 && response[1] == 0x31) {
                memcpy(deviceId_, response + 8, sizeof(deviceId_));
                deviceTimestamp_ = readBe32(response + 12);
                sessionMillis_ = millis();
                sessionReady_ = true;
                return true;
            }
        }
        delay(10);
    }
    return false;
}

uint32_t MiioTransport::currentDeviceTimestamp() const
{
    return deviceTimestamp_ + ((millis() - sessionMillis_) / 1000UL);
}

MiioResult MiioTransport::request(const String &method, const String &paramsJson)
{
    if (!configured_ || !socketStarted_) return {MiioError::InvalidConfig, {}};
    if (!ensureSession()) return {MiioError::Timeout, {}};

    String json;
    json.reserve(method.length() + paramsJson.length() + 48);
    json += F("{\"id\":");
    json += requestId_++;
    json += F(",\"method\":\"");
    json += method;
    json += F("\",\"params\":");
    json += paramsJson;
    json += '}';

    MiioResult result = exchange(json);
    if (!result.ok() && result.error == MiioError::Timeout) {
        resetSession();
        if (ensureSession()) result = exchange(json);
    }
    return result;
}

bool MiioTransport::encrypt(const uint8_t *plain, size_t plainLength,
                            uint8_t *cipher, size_t &cipherLength) const
{
    const uint8_t padding = 16 - (plainLength % 16);
    cipherLength = plainLength + padding;
    if (cipherLength > kMaxPacketSize - kHeaderSize) return false;

    uint8_t padded[kMaxPacketSize - kHeaderSize];
    memcpy(padded, plain, plainLength);
    memset(padded + plainLength, padding, padding);

    uint8_t iv[16];
    memcpy(iv, iv_, sizeof(iv));
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    const int keyResult = mbedtls_aes_setkey_enc(&aes, key_, 128);
    const int cryptResult = keyResult == 0
        ? mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, cipherLength, iv, padded, cipher)
        : keyResult;
    mbedtls_aes_free(&aes);
    return cryptResult == 0;
}

bool MiioTransport::decrypt(const uint8_t *cipher, size_t cipherLength,
                            uint8_t *plain, size_t &plainLength) const
{
    if (cipherLength == 0 || cipherLength % 16 != 0) return false;
    uint8_t iv[16];
    memcpy(iv, iv_, sizeof(iv));
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    const int keyResult = mbedtls_aes_setkey_dec(&aes, key_, 128);
    const int cryptResult = keyResult == 0
        ? mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, cipherLength, iv, cipher, plain)
        : keyResult;
    mbedtls_aes_free(&aes);
    if (cryptResult != 0) return false;

    const uint8_t padding = plain[cipherLength - 1];
    if (padding == 0 || padding > 16 || padding > cipherLength) return false;
    for (size_t i = cipherLength - padding; i < cipherLength; ++i) {
        if (plain[i] != padding) return false;
    }
    plainLength = cipherLength - padding;
    while (plainLength > 0 && plain[plainLength - 1] == '\0') --plainLength;
    return true;
}

MiioResult MiioTransport::exchange(const String &json)
{
    uint8_t plain[kMaxPacketSize - kHeaderSize];
    const size_t jsonLength = json.length();
    if (jsonLength + 1 > sizeof(plain)) return {MiioError::InvalidPacket, {}};
    memcpy(plain, json.c_str(), jsonLength);
    plain[jsonLength] = '\0';

    uint8_t packet[kMaxPacketSize];
    uint8_t *cipher = packet + kHeaderSize;
    size_t cipherLength = 0;
    if (!encrypt(plain, jsonLength + 1, cipher, cipherLength)) {
        return {MiioError::CryptoError, {}};
    }

    memset(packet, 0, kHeaderSize);
    packet[0] = 0x21;
    packet[1] = 0x31;
    const size_t packetLength = kHeaderSize + cipherLength;
    writeBe16(packet + 2, static_cast<uint16_t>(packetLength));
    memcpy(packet + 8, deviceId_, sizeof(deviceId_));
    writeBe32(packet + 12, currentDeviceTimestamp());

    uint8_t checksumInput[kMaxPacketSize];
    memcpy(checksumInput, packet, 16);
    memcpy(checksumInput + 16, token_, sizeof(token_));
    memcpy(checksumInput + 32, cipher, cipherLength);
    md5(checksumInput, 32 + cipherLength, packet + 16);

    while (udp_.parsePacket() > 0) {
        uint8_t discard[kMaxPacketSize];
        udp_.read(discard, sizeof(discard));
    }
    if (!udp_.beginPacket(deviceIp_, kPort) ||
        udp_.write(packet, packetLength) != packetLength ||
        !udp_.endPacket()) {
        return {MiioError::SocketError, {}};
    }

    const uint32_t deadline = millis() + 1800;
    while (static_cast<int32_t>(deadline - millis()) > 0) {
        const int received = udp_.parsePacket();
        if (received >= static_cast<int>(kHeaderSize) && received <= static_cast<int>(kMaxPacketSize)) {
            const int responseLength = udp_.read(packet, sizeof(packet));
            if (responseLength < static_cast<int>(kHeaderSize) ||
                packet[0] != 0x21 || packet[1] != 0x31 ||
                readBe16(packet + 2) != responseLength) {
                return {MiioError::InvalidPacket, {}};
            }

            const size_t responseCipherLength = responseLength - kHeaderSize;
            memcpy(checksumInput, packet, 16);
            memcpy(checksumInput + 16, token_, sizeof(token_));
            memcpy(checksumInput + 32, packet + kHeaderSize, responseCipherLength);
            uint8_t expectedChecksum[16];
            md5(checksumInput, 32 + responseCipherLength, expectedChecksum);
            if (memcmp(expectedChecksum, packet + 16, sizeof(expectedChecksum)) != 0) {
                return {MiioError::ChecksumMismatch, {}};
            }

            size_t decryptedLength = 0;
            if (!decrypt(packet + kHeaderSize, responseCipherLength, plain, decryptedLength)) {
                return {MiioError::CryptoError, {}};
            }
            plain[decryptedLength] = '\0';
            String payload(reinterpret_cast<char *>(plain));
            if (payload.indexOf(F("\"error\"")) >= 0) {
                return {MiioError::DeviceError, payload};
            }
            return {MiioError::None, payload};
        }
        delay(10);
    }
    return {MiioError::Timeout, {}};
}
