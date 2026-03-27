#include "MCDevConsole/Protocol.h"

#include <cstring>

namespace MCDevConsole::Protocol {

std::vector<std::uint8_t> SerializePacket(const Packet& packet) {
    std::vector<std::uint8_t> result;
    result.reserve(HEADER_SIZE + packet.data.size());

    std::uint16_t type_id_be = (packet.type_id >> 8) | ((packet.type_id & 0xFF) << 8);
    std::uint32_t size_be = static_cast<std::uint32_t>(packet.data.size());
    size_be = ((size_be & 0xFF000000) >> 24) |
              ((size_be & 0x00FF0000) >> 8) |
              ((size_be & 0x0000FF00) << 8) |
              ((size_be & 0x000000FF) << 24);

    const auto* type_ptr = reinterpret_cast<const std::uint8_t*>(&type_id_be);
    result.insert(result.end(), type_ptr, type_ptr + TYPE_ID_SIZE);

    const auto* size_ptr = reinterpret_cast<const std::uint8_t*>(&size_be);
    result.insert(result.end(), size_ptr, size_ptr + SIZE_FIELD_SIZE);

    result.insert(result.end(), packet.data.begin(), packet.data.end());

    return result;
}

bool TryDeserializePacket(const std::vector<std::uint8_t>& buffer, Packet& packet, std::size_t& consumed_size) {
    if (buffer.size() < HEADER_SIZE) {
        consumed_size = 0;
        return false;
    }

    std::uint16_t type_id_be = 0;
    std::memcpy(&type_id_be, buffer.data(), TYPE_ID_SIZE);
    packet.type_id = (type_id_be >> 8) | ((type_id_be & 0xFF) << 8);

    std::uint32_t size_be = 0;
    std::memcpy(&size_be, buffer.data() + TYPE_ID_SIZE, SIZE_FIELD_SIZE);
    std::uint32_t data_size = ((size_be & 0xFF000000) >> 24) |
                              ((size_be & 0x00FF0000) >> 8) |
                              ((size_be & 0x0000FF00) << 8) |
                              ((size_be & 0x000000FF) << 24);

    if (data_size > MAX_PACKET_SIZE) {
        consumed_size = 0;
        return false;
    }

    const std::size_t total_size = HEADER_SIZE + data_size;
    if (buffer.size() < total_size) {
        consumed_size = 0;
        return false;
    }

    packet.data.assign(buffer.begin() + HEADER_SIZE, buffer.begin() + total_size);
    consumed_size = total_size;
    return true;
}

const char* GetTypeName(std::uint16_t type_id) noexcept {
    switch (type_id) {
    case TYPE_STDOUT_LOG:
        return "STDOUT_LOG";
    case TYPE_STDERR_LOG:
        return "STDERR_LOG";
    case TYPE_EXEC_CLIENT:
        return "EXEC_CLIENT";
    case TYPE_EXEC_SERVER:
        return "EXEC_SERVER";
    default:
        return "UNKNOWN";
    }
}

} // namespace MCDevConsole::Protocol
