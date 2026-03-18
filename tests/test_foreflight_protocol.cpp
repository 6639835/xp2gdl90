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
