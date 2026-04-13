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

TEST_CASE("IPv4 validator accepts dotted-quad addresses only") {
  ASSERT_TRUE(xp2gdl90::protocol::IsValidIpv4Address("127.0.0.1"));
  ASSERT_TRUE(xp2gdl90::protocol::IsValidIpv4Address("255.255.255.255"));
  ASSERT_TRUE(xp2gdl90::protocol::IsValidIpv4Address("192.168.001.010"));

  ASSERT_TRUE(!xp2gdl90::protocol::IsValidIpv4Address(""));
  ASSERT_TRUE(!xp2gdl90::protocol::IsValidIpv4Address("300.1.1.1"));
  ASSERT_TRUE(!xp2gdl90::protocol::IsValidIpv4Address("1.2.3"));
  ASSERT_TRUE(!xp2gdl90::protocol::IsValidIpv4Address("1.2.3.4.5"));
  ASSERT_TRUE(!xp2gdl90::protocol::IsValidIpv4Address("1..3.4"));
  ASSERT_TRUE(!xp2gdl90::protocol::IsValidIpv4Address("1.2.3.-1"));
  ASSERT_TRUE(!xp2gdl90::protocol::IsValidIpv4Address("1.2.3.4 "));
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
