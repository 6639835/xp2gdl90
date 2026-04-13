#include "test_harness.h"

#include <array>
#include <cerrno>
#include <cstdint>
#include <string>
#include <vector>

#include "xp2gdl90/udp_broadcaster.h"

#if defined(XP2GDL90_ENABLE_SOCKET_OPS_TESTS)
namespace udp {
namespace detail {
int xp2gdl90_test_default_socket_ops_startup();
void xp2gdl90_test_default_socket_ops_cleanup();
uintptr_t xp2gdl90_test_default_socket_ops_create_socket(int domain, int type,
                                                        int protocol);
}  // namespace detail
}  // namespace udp
#endif

namespace {

struct FakeSocketOps final : udp::detail::SocketOps {
  int startup_result = 0;
  uintptr_t create_socket_result = udp::UDPBroadcaster::kInvalidSocket;
  int setsockopt_result = 0;
  int inet_pton_result = 1;
  intptr_t sendto_result = -1;
  int close_result = 0;
  int last_error_value = 0;

  int startup_calls = 0;
  int cleanup_calls = 0;
  int create_socket_calls = 0;
  int setsockopt_calls = 0;
  int inet_pton_calls = 0;
  int sendto_calls = 0;
  int close_calls = 0;

  uintptr_t last_closed_socket = udp::UDPBroadcaster::kInvalidSocket;

  int Startup() override {
    ++startup_calls;
    return startup_result;
  }

  void Cleanup() override { ++cleanup_calls; }

  uintptr_t CreateSocket(int, int, int) override {
    ++create_socket_calls;
    return create_socket_result;
  }

  int SetSockOpt(uintptr_t, int, int, const void*, size_t) override {
    ++setsockopt_calls;
    return setsockopt_result;
  }

  int InetPton(int, const char*, void*) override {
    ++inet_pton_calls;
    return inet_pton_result;
  }

  intptr_t SendTo(uintptr_t, const void*, size_t, int, const void*, size_t) override {
    ++sendto_calls;
    return sendto_result;
  }

  int CloseSocket(uintptr_t socket_handle) override {
    ++close_calls;
    last_closed_socket = socket_handle;
    return close_result;
  }

  int LastError() override { return last_error_value; }
};

}  // namespace

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

TEST_CASE("UDPBroadcaster initialize returns true when already initialized") {
  FakeSocketOps ops;
  ops.create_socket_result = 42;
  ops.setsockopt_result = 0;

  udp::UDPBroadcaster broadcaster("127.0.0.1", 4000, &ops);
  ASSERT_TRUE(broadcaster.initialize());
  ASSERT_TRUE(broadcaster.initialize());
  ASSERT_EQ(1, ops.create_socket_calls);
  broadcaster.close();
}

TEST_CASE("UDPBroadcaster reports socket creation failure") {
  FakeSocketOps ops;
  ops.create_socket_result = udp::UDPBroadcaster::kInvalidSocket;
  ops.last_error_value = EACCES;

  udp::UDPBroadcaster broadcaster("127.0.0.1", 4000, &ops);
  ASSERT_TRUE(!broadcaster.initialize());
  ASSERT_TRUE(broadcaster.getLastError().find("Socket creation failed") !=
              std::string::npos);
}

TEST_CASE("UDPBroadcaster reports broadcast option failure and closes socket") {
  FakeSocketOps ops;
  ops.create_socket_result = 42;
  ops.setsockopt_result = -1;
  ops.last_error_value = EPERM;

  udp::UDPBroadcaster broadcaster("127.0.0.1", 4000, &ops);
  ASSERT_TRUE(!broadcaster.initialize());
  ASSERT_TRUE(broadcaster.getLastError().find("Failed to set SO_BROADCAST") !=
              std::string::npos);
  ASSERT_EQ(1, ops.close_calls);
  ASSERT_EQ(static_cast<uintptr_t>(42), ops.last_closed_socket);
}

TEST_CASE("UDPBroadcaster reports sendto failure") {
  FakeSocketOps ops;
  ops.create_socket_result = 42;
  ops.setsockopt_result = 0;
  ops.inet_pton_result = 1;
  ops.sendto_result = -1;
  ops.last_error_value = EINVAL;

  udp::UDPBroadcaster broadcaster("127.0.0.1", 4000, &ops);
  ASSERT_TRUE(broadcaster.initialize());

  std::array<uint8_t, 2> data{{0xAA, 0xBB}};
  ASSERT_EQ(-1, broadcaster.send(data.data(), data.size()));
  ASSERT_TRUE(broadcaster.getLastError().find("sendto failed") !=
              std::string::npos);

  broadcaster.close();
  ASSERT_TRUE(!broadcaster.isInitialized());
}

#if defined(XP2GDL90_ENABLE_SOCKET_OPS_TESTS)
TEST_CASE("Default socket ops startup and cleanup helpers are callable") {
  ASSERT_EQ(0, udp::detail::xp2gdl90_test_default_socket_ops_startup());
  udp::detail::xp2gdl90_test_default_socket_ops_cleanup();
}

TEST_CASE("Default socket ops create socket failure path works") {
  const uintptr_t invalid =
      udp::detail::xp2gdl90_test_default_socket_ops_create_socket(-1, -1, -1);
  ASSERT_EQ(udp::UDPBroadcaster::kInvalidSocket, invalid);
}
#endif

TEST_CASE("UDPBroadcaster forwards payload when send succeeds") {
  FakeSocketOps ops;
  ops.create_socket_result = 42;
  ops.setsockopt_result = 0;
  ops.inet_pton_result = 1;
  ops.sendto_result = 4;

  udp::UDPBroadcaster broadcaster("127.0.0.1", 4000, &ops);
  ASSERT_TRUE(broadcaster.initialize());

  const std::vector<uint8_t> payload{0x10, 0x20, 0x30, 0x40};
  ASSERT_EQ(static_cast<int>(payload.size()), broadcaster.send(payload));

  broadcaster.close();
  ASSERT_TRUE(!broadcaster.isInitialized());
  ASSERT_EQ(1, ops.sendto_calls);
}

TEST_CASE("UDPBroadcaster updates target with setter") {
  udp::UDPBroadcaster broadcaster("127.0.0.1", 4000);
  broadcaster.setTarget("192.168.0.255", 4900);
  ASSERT_EQ(std::string("192.168.0.255"), broadcaster.getTargetIp());
  ASSERT_EQ(static_cast<uint16_t>(4900), broadcaster.getTargetPort());
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
