#ifndef XP2GDL90_ENCODER_SUPPORT_H
#define XP2GDL90_ENCODER_SUPPORT_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gdl90::internal {

std::vector<uint8_t> PrepareMessage(const std::vector<uint8_t>& payload);
void AppendBigEndian16(std::vector<uint8_t>& buffer, uint16_t value);
void AppendBigEndian32(std::vector<uint8_t>& buffer, uint32_t value);
void AppendBigEndian64(std::vector<uint8_t>& buffer, uint64_t value);
void AppendFixedText(std::vector<uint8_t>& buffer,
                     const std::string& value,
                     size_t width);
void Pack24Bit(std::vector<uint8_t>& buffer, uint32_t value);

}  // namespace gdl90::internal

#endif  // XP2GDL90_ENCODER_SUPPORT_H
