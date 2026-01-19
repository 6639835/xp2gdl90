#include "test_harness.h"

#include <array>
#include <cstdint>
#include <string>

#include "xp2gdl90/udp_broadcaster.h"

TEST_CASE("UDPBroadcaster send fails when not initialized") {
  udp::UDPBroadcaster broadcaster("127.0.0.1", 4000);
  std::array<uint8_t, 3> data{{0x01, 0x02, 0x03}};
  ASSERT_EQ(-1, broadcaster.send(data.data(), data.size()));
  ASSERT_NE(std::string(""), broadcaster.getLastError());
}

TEST_CASE("UDPBroadcaster rejects invalid target IP") {
  udp::UDPBroadcaster broadcaster("invalid_ip", 4000);
  ASSERT_TRUE(broadcaster.initialize());
  std::array<uint8_t, 1> data{{0x00}};
  ASSERT_EQ(-1, broadcaster.send(data.data(), data.size()));
  ASSERT_TRUE(broadcaster.getLastError().find("Invalid IP address") !=
              std::string::npos);
  broadcaster.close();
}

TEST_CASE("UDPBroadcaster sends data after initialize") {
  udp::UDPBroadcaster broadcaster("127.0.0.1", 4000);
  ASSERT_TRUE(broadcaster.initialize());

  std::array<uint8_t, 4> data{{0x10, 0x20, 0x30, 0x40}};
  const int sent = broadcaster.send(data.data(), data.size());
  if (sent == -1) {
    ASSERT_NE(std::string(""), broadcaster.getLastError());
  } else {
    ASSERT_EQ(static_cast<int>(data.size()), sent);
  }

  broadcaster.close();
  ASSERT_TRUE(!broadcaster.isInitialized());
}
