#include "test_harness.h"

#include <cstdint>
#include <vector>

#include "xp2gdl90/protocol_utils.h"

TEST_CASE("SanitizeCallsign uppercases and removes unsupported characters") {
  ASSERT_EQ(std::string("N123 AB"),
            xp2gdl90::protocol::SanitizeCallsign("n123-ab!"));
  ASSERT_EQ(std::string("ABCD1234"),
            xp2gdl90::protocol::SanitizeCallsign("abcd1234xyz"));
}

TEST_CASE("SanitizeCallsign trims trailing spaces and allows empty fallback") {
  ASSERT_EQ(std::string("ABC DEF"),
            xp2gdl90::protocol::SanitizeCallsign("ABC_DEF "));
  ASSERT_EQ(std::string(""),
            xp2gdl90::protocol::SanitizeCallsign("!!!"));
}

TEST_CASE("Protocol field validators enforce published ranges") {
  ASSERT_TRUE(xp2gdl90::protocol::IsValidNic(11));
  ASSERT_TRUE(!xp2gdl90::protocol::IsValidNic(12));
  ASSERT_TRUE(xp2gdl90::protocol::IsValidNacp(11));
  ASSERT_TRUE(!xp2gdl90::protocol::IsValidNacp(12));
  ASSERT_TRUE(xp2gdl90::protocol::IsValidEmitterCategory(39));
  ASSERT_TRUE(!xp2gdl90::protocol::IsValidEmitterCategory(40));
}

TEST_CASE("Ownship position validity requires finite in-range coordinates") {
  ASSERT_TRUE(xp2gdl90::protocol::HasValidOwnshipPosition(37.6189, -122.375));
  ASSERT_TRUE(!xp2gdl90::protocol::HasValidOwnshipPosition(91.0, 0.0));
  ASSERT_TRUE(!xp2gdl90::protocol::HasValidOwnshipPosition(0.0, 181.0));
}

TEST_CASE("ForeFlight discovery parser requires nested GDL90 port") {
  const std::vector<uint8_t> packet{
      '{', '"', 'A', 'p', 'p', '"', ':', '"', 'F', 'o', 'r', 'e', 'F', 'l',
      'i', 'g', 'h', 't', '"', ',', '"', 'G', 'D', 'L', '9', '0', '"', ':',
      '{', '"', 'p', 'o', 'r', 't', '"', ':', '4', '0', '0', '0', '}', '}'};
  uint16_t port = 0;
  ASSERT_TRUE(
      xp2gdl90::protocol::ParseForeFlightDiscoveryBroadcast(packet, &port));
  ASSERT_EQ(static_cast<uint16_t>(4000), port);
}

TEST_CASE("ForeFlight discovery parser rejects top-level port and bad app") {
  const std::vector<uint8_t> top_level_port{
      '{', '"', 'A', 'p', 'p', '"', ':', '"', 'F', 'o', 'r', 'e', 'F', 'l',
      'i', 'g', 'h', 't', '"', ',', '"', 'p', 'o', 'r', 't', '"', ':', '4',
      '0', '0', '0', '}'};
  uint16_t port = 0;
  ASSERT_TRUE(!xp2gdl90::protocol::ParseForeFlightDiscoveryBroadcast(
      top_level_port, &port));

  const std::vector<uint8_t> wrong_app{
      '{', '"', 'A', 'p', 'p', '"', ':', '"', 'N', 'o', 't', 'F', 'F', '"',
      ',', '"', 'G', 'D', 'L', '9', '0', '"', ':', '{', '"', 'p', 'o', 'r',
      't', '"', ':', '4', '0', '0', '0', '}', '}'};
  ASSERT_TRUE(!xp2gdl90::protocol::ParseForeFlightDiscoveryBroadcast(
      wrong_app, &port));
}
