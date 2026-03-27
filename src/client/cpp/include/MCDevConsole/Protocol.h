#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace MCDevConsole::Protocol {

constexpr std::uint16_t TYPE_STDOUT_LOG = 1;
constexpr std::uint16_t TYPE_STDERR_LOG = 2;
constexpr std::uint16_t TYPE_EXEC_CLIENT = 3;
constexpr std::uint16_t TYPE_EXEC_SERVER = 4;

constexpr std::size_t TYPE_ID_SIZE = 2;
constexpr std::size_t SIZE_FIELD_SIZE = 4;
constexpr std::size_t HEADER_SIZE = TYPE_ID_SIZE + SIZE_FIELD_SIZE;
constexpr std::size_t MAX_PACKET_SIZE = 1024 * 1024;

struct Packet {
    std::uint16_t type_id = 0;
    std::vector<std::uint8_t> data;
};

using PacketHandler = void(*)(const Packet& packet);

[[nodiscard]] std::vector<std::uint8_t> SerializePacket(const Packet& packet);
[[nodiscard]] bool TryDeserializePacket(const std::vector<std::uint8_t>& buffer, Packet& packet, std::size_t& consumed_size);
[[nodiscard]] const char* GetTypeName(std::uint16_t type_id) noexcept;

} // namespace MCDevConsole::Protocol
