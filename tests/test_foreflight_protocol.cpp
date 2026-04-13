#include "test_harness.h"

#include <cstdint>
#include <vector>

#include "xp2gdl90/foreflight_protocol.h"

TEST_CASE("ForeFlight discovery parser requires nested GDL90 port") {
  const std::vector<uint8_t> packet{
      '{', '"', 'A', 'p', 'p', '"', ':', '"', 'F', 'o', 'r', 'e', 'F', 'l',
      'i', 'g', 'h', 't', '"', ',', '"', 'G', 'D', 'L', '9', '0', '"', ':',
      '{', '"', 'p', 'o', 'r', 't', '"', ':', '4', '0', '0', '0', '}', '}'};
  uint16_t port = 0;
  ASSERT_TRUE(xp2gdl90::foreflight::ParseDiscoveryBroadcast(packet, &port));
  ASSERT_EQ(static_cast<uint16_t>(4000), port);
}

TEST_CASE("ForeFlight discovery parser rejects top-level port and bad app") {
  const std::vector<uint8_t> top_level_port{
      '{', '"', 'A', 'p', 'p', '"', ':', '"', 'F', 'o', 'r', 'e', 'F', 'l',
      'i', 'g', 'h', 't', '"', ',', '"', 'p', 'o', 'r', 't', '"', ':', '4',
      '0', '0', '0', '}'};
  uint16_t port = 0;
  ASSERT_TRUE(
      !xp2gdl90::foreflight::ParseDiscoveryBroadcast(top_level_port, &port));

  const std::vector<uint8_t> wrong_app{
      '{', '"', 'A', 'p', 'p', '"', ':', '"', 'N', 'o', 't', 'F', 'F', '"',
      ',', '"', 'G', 'D', 'L', '9', '0', '"', ':', '{', '"', 'p', 'o', 'r',
      't', '"', ':', '4', '0', '0', '0', '}', '}'};
  ASSERT_TRUE(!xp2gdl90::foreflight::ParseDiscoveryBroadcast(wrong_app, &port));
}

TEST_CASE("ForeFlight discovery parser rejects malformed packets and bad ports") {
  uint16_t port = 0;
  const std::vector<uint8_t> empty_packet;
  ASSERT_TRUE(!xp2gdl90::foreflight::ParseDiscoveryBroadcast(empty_packet, &port));
  ASSERT_TRUE(!xp2gdl90::foreflight::ParseDiscoveryBroadcast(
      std::vector<uint8_t>{'{', '}'}, nullptr));

  const std::vector<uint8_t> array_root{'[', '1', ']'};
  ASSERT_TRUE(!xp2gdl90::foreflight::ParseDiscoveryBroadcast(array_root, &port));

  const std::vector<uint8_t> missing_gdl90{
      '{', '"', 'A', 'p', 'p', '"', ':', '"', 'F', 'o', 'r', 'e', 'F', 'l',
      'i', 'g', 'h', 't', '"', '}'};
  ASSERT_TRUE(!xp2gdl90::foreflight::ParseDiscoveryBroadcast(missing_gdl90, &port));

  const std::vector<uint8_t> bad_port{
      '{', '"', 'A', 'p', 'p', '"', ':', '"', 'F', 'o', 'r', 'e', 'F', 'l',
      'i', 'g', 'h', 't', '"', ',', '"', 'G', 'D', 'L', '9', '0', '"', ':',
      '{', '"', 'p', 'o', 'r', 't', '"', ':', '0', '}', '}'};
  ASSERT_TRUE(!xp2gdl90::foreflight::ParseDiscoveryBroadcast(bad_port, &port));
}
