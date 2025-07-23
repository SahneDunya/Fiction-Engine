#include "network/fe_net_packet.h"
#include "core/utils/fe_logger.h" // For logging
#include "core/memory/fe_memory_manager.h" // For memory management

#include <string.h> // For memcpy, strlen
#include <stdio.h>  // For snprintf (debugging)

// --- Helper for endianness conversion (if not provided by system headers) ---
// Note: On Windows, winsock2.h provides these. On Linux/Unix, arpa/inet.h provides.
// For other platforms or bare-metal, custom implementations might be needed.
#if !defined(_MSC_VER) && !defined(__GNUC__) && !defined(__clang__)
// Basic fallback for systems without standard network byte order functions
// This assumes little-endian system for these definitions. Adjust if system is big-endian.
#ifndef FE_HTONL
uint32_t __fe_hton_uint32(uint32_t x) {
    uint8_t *s = (uint8_t*)&x;
    return (uint32_t)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
}
#define FE_HTONL(x) __fe_hton_uint32(x)
#endif
#ifndef FE_NTOHL
uint32_t __fe_ntoh_uint32(uint32_t x) {
    uint8_t *s = (uint8_t*)&x;
    return (uint32_t)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
}
#define FE_NTOHL(x) __fe_ntoh_uint32(x)
#endif
#ifndef FE_HTONS
uint16_t __fe_hton_uint16(uint16_t x) {
    uint8_t *s = (uint8_t*)&x;
    return (uint16_t)(s[0] << 8 | s[1]);
}
#define FE_HTONS(x) __fe_hton_uint16(x)
#endif
#ifndef FE_NTOHS
uint16_t __fe_ntoh_uint16(uint16_t x) {
    uint8_t *s = (uint8_t*)&x;
    return (uint16_t)(s[0] << 8 | s[1]);
}
#define FE_NTOHS(x) __fe_ntoh_uint16(x)
#endif
#endif // Endianness fallback

// --- fe_net_packet_t Fonksiyonları ---

bool fe_net_packet_init(fe_net_packet_t* packet, size_t initial_capacity) {
    if (!packet) {
        FE_LOG_ERROR("fe_net_packet_init: Packet pointer is NULL.");
        return false;
    }
    memset(packet, 0, sizeof(fe_net_packet_t)); // Her şeyi sıfırla

    if (initial_capacity > 0) {
        if (initial_capacity > FE_NET_MAX_PACKET_SIZE - FE_NET_PACKET_HEADER_SIZE) {
            FE_LOG_WARN("Requested initial packet payload capacity %zu exceeds max allowed payload size %zu. Clamping.",
                        initial_capacity, FE_NET_MAX_PACKET_SIZE - FE_NET_PACKET_HEADER_SIZE);
            initial_capacity = FE_NET_MAX_PACKET_SIZE - FE_NET_PACKET_HEADER_SIZE;
        }
        packet->payload = (uint8_t*)FE_MALLOC(initial_capacity, FE_MEM_TYPE_NETWORK_PACKET);
        if (!packet->payload) {
            FE_LOG_ERROR("fe_net_packet_init: Failed to allocate payload buffer of size %zu.", initial_capacity);
            return false;
        }
        packet->capacity = initial_capacity;
    }
    packet->header.type = FE_PACKET_TYPE_UNKNOWN;
    packet->header.payload_size = 0; // Başlangıçta payload boyutu 0
    return true;
}

void fe_net_packet_destroy(fe_net_packet_t* packet) {
    if (!packet) return;

    if (packet->payload) {
        FE_FREE(packet->payload, FE_MEM_TYPE_NETWORK_PACKET);
        packet->payload = NULL;
    }
    packet->capacity = 0;
    packet->header.type = FE_PACKET_TYPE_UNKNOWN;
    packet->header.payload_size = 0;
    // memset(packet, 0, sizeof(fe_net_packet_t)); // Opsiyonel: Tamamen sıfırla
}

bool fe_net_packet_resize_payload(fe_net_packet_t* packet, size_t new_capacity) {
    if (!packet) {
        FE_LOG_ERROR("fe_net_packet_resize_payload: Packet pointer is NULL.");
        return false;
    }

    if (new_capacity > FE_NET_MAX_PACKET_SIZE - FE_NET_PACKET_HEADER_SIZE) {
        FE_LOG_WARN("Requested packet payload capacity %zu exceeds max allowed payload size %zu. Clamping.",
                    new_capacity, FE_NET_MAX_PACKET_SIZE - FE_NET_PACKET_HEADER_SIZE);
        new_capacity = FE_NET_MAX_PACKET_SIZE - FE_NET_PACKET_HEADER_SIZE;
    }

    if (new_capacity == packet->capacity) {
        return true; // Boyut aynı, hiçbir şey yapma
    }

    uint8_t* new_payload = (uint8_t*)FE_REALLOC(packet->payload, new_capacity, FE_MEM_TYPE_NETWORK_PACKET);
    if (!new_payload) {
        FE_LOG_ERROR("fe_net_packet_resize_payload: Failed to reallocate payload to %zu bytes.", new_capacity);
        return false;
    }
    packet->payload = new_payload;
    packet->capacity = new_capacity;

    // Eğer payload boyutu kapasiteden büyükse, düzelt
    if (packet->header.payload_size > new_capacity) {
        packet->header.payload_size = (uint32_t)new_capacity;
        FE_LOG_WARN("Packet payload truncated due to resize. New payload size: %u", packet->header.payload_size);
    }
    return true;
}

size_t fe_net_packet_serialize(const fe_net_packet_t* packet, uint8_t* buffer, size_t buffer_size) {
    if (!packet || !buffer) {
        FE_LOG_ERROR("fe_net_packet_serialize: Invalid arguments (NULL packet or buffer).");
        return 0;
    }

    size_t total_packet_size = FE_NET_PACKET_HEADER_SIZE + packet->header.payload_size;
    if (buffer_size < total_packet_size) {
        FE_LOG_ERROR("fe_net_packet_serialize: Buffer too small (%zu bytes) for packet (%zu bytes).", buffer_size, total_packet_size);
        return 0;
    }
    if (total_packet_size > FE_NET_MAX_PACKET_SIZE) {
        FE_LOG_ERROR("fe_net_packet_serialize: Packet size (%zu) exceeds max allowed (%d).", total_packet_size, FE_NET_MAX_PACKET_SIZE);
        return 0;
    }

    size_t offset = 0;
    // Başlık: type
    fe_net_write_uint32(buffer, offset, packet->header.type);
    offset += sizeof(uint32_t);

    // Başlık: payload_size
    fe_net_write_uint32(buffer, offset, packet->header.payload_size);
    offset += sizeof(uint32_t);

    // Payload (eğer varsa)
    if (packet->payload && packet->header.payload_size > 0) {
        memcpy(buffer + offset, packet->payload, packet->header.payload_size);
        offset += packet->header.payload_size;
    }

    return offset; // Toplam yazılan bayt sayısı
}

bool fe_net_packet_deserialize(fe_net_packet_t* packet, const uint8_t* buffer, size_t buffer_size) {
    if (!packet || !buffer) {
        FE_LOG_ERROR("fe_net_packet_deserialize: Invalid arguments (NULL packet or buffer).");
        return false;
    }
    if (buffer_size < FE_NET_PACKET_HEADER_SIZE) {
        FE_LOG_ERROR("fe_net_packet_deserialize: Buffer too small (%zu bytes) to read header (%zu bytes).", buffer_size, FE_NET_PACKET_HEADER_SIZE);
        return false;
    }

    size_t offset = 0;
    uint32_t temp_type, temp_payload_size;

    // Başlık: type
    if (fe_net_read_uint32(buffer, offset, &temp_type) == 0) return false;
    offset += sizeof(uint32_t);

    // Başlık: payload_size
    if (fe_net_read_uint32(buffer, offset, &temp_payload_size) == 0) return false;
    offset += sizeof(uint32_t);

    // Gelen paketin boyut kontrolü
    if (temp_payload_size > (buffer_size - FE_NET_PACKET_HEADER_SIZE)) {
        FE_LOG_ERROR("fe_net_packet_deserialize: Declared payload size (%u) exceeds actual buffer remaining size (%zu).",
                     temp_payload_size, buffer_size - FE_NET_PACKET_HEADER_SIZE);
        return false;
    }
    if (FE_NET_PACKET_HEADER_SIZE + temp_payload_size > FE_NET_MAX_PACKET_SIZE) {
        FE_LOG_ERROR("fe_net_packet_deserialize: Total packet size (%zu) exceeds max allowed (%d).",
                     (size_t)FE_NET_PACKET_HEADER_SIZE + temp_payload_size, FE_NET_MAX_PACKET_SIZE);
        return false;
    }
    
    // Paketi yeniden başlat veya kapasiteyi ayarla
    if (packet->payload) {
        fe_net_packet_destroy(packet); // Önceki payload'ı temizle
    }
    if (!fe_net_packet_init(packet, temp_payload_size)) { // Yeni payload için kapasiteyi ayarla
        FE_LOG_ERROR("fe_net_packet_deserialize: Failed to initialize packet for deserialization.");
        return false;
    }
    packet->header.type = (fe_packet_type_t)temp_type;
    packet->header.payload_size = temp_payload_size;

    // Payload (eğer varsa)
    if (temp_payload_size > 0) {
        memcpy(packet->payload, buffer + offset, temp_payload_size);
    }
    
    return true;
}

bool fe_net_packet_set_payload(fe_net_packet_t* packet, const void* data, size_t size) {
    if (!packet || !data) {
        FE_LOG_ERROR("fe_net_packet_set_payload: Invalid arguments (NULL packet or data).");
        return false;
    }

    if (size > FE_NET_MAX_PACKET_SIZE - FE_NET_PACKET_HEADER_SIZE) {
        FE_LOG_ERROR("fe_net_packet_set_payload: Payload size %zu exceeds max allowed payload size %zu.",
                     size, FE_NET_MAX_PACKET_SIZE - FE_NET_PACKET_HEADER_SIZE);
        return false;
    }

    if (size > packet->capacity) {
        // Kapasite yetersiz, yeniden boyutlandır
        if (!fe_net_packet_resize_payload(packet, size)) {
            FE_LOG_ERROR("fe_net_packet_set_payload: Failed to resize payload to %zu bytes.", size);
            return false;
        }
    } else if (size == 0) { // If payload is set to 0 size, free memory if it's large
        if (packet->capacity > 0) {
            fe_net_packet_resize_payload(packet, 0); // Free memory
        }
    }


    if (packet->payload && size > 0) {
        memcpy(packet->payload, data, size);
    }
    packet->header.payload_size = (uint32_t)size;
    return true;
}

void fe_net_packet_set_type(fe_net_packet_t* packet, fe_packet_type_t type) {
    if (packet) {
        packet->header.type = type;
    }
}

fe_packet_type_t fe_net_packet_get_type(const fe_net_packet_t* packet) {
    if (!packet) {
        return FE_PACKET_TYPE_UNKNOWN;
    }
    return packet->header.type;
}

uint32_t fe_net_packet_get_payload_size(const fe_net_packet_t* packet) {
    if (!packet) {
        return 0;
    }
    return packet->header.payload_size;
}

const uint8_t* fe_net_packet_get_payload_ptr(const fe_net_packet_t* packet) {
    if (!packet || !packet->payload || packet->header.payload_size == 0) {
        return NULL;
    }
    return packet->payload;
}

// --- Yardımcı Seri Hale Getirme/Seri Halden Çıkarma Fonksiyonları ---

size_t fe_net_write_uint8(uint8_t* buffer, size_t offset, uint8_t value) {
    if (!buffer) return 0;
    buffer[offset] = value;
    return sizeof(uint8_t);
}

size_t fe_net_write_uint16(uint8_t* buffer, size_t offset, uint16_t value) {
    if (!buffer) return 0;
    uint16_t net_value = FE_HTONS(value);
    memcpy(buffer + offset, &net_value, sizeof(uint16_t));
    return sizeof(uint16_t);
}

size_t fe_net_write_uint32(uint8_t* buffer, size_t offset, uint32_t value) {
    if (!buffer) return 0;
    uint32_t net_value = FE_HTONL(value);
    memcpy(buffer + offset, &net_value, sizeof(uint32_t));
    return sizeof(uint32_t);
}

size_t fe_net_write_uint64(uint8_t* buffer, size_t offset, uint64_t value) {
    if (!buffer) return 0;
    // uint64_t için standart htonl/ntohl karşılığı olmayabilir, manuel dönüştürme gerekebilir
    // Veya sistemin __BYTE_ORDER__ kontrol edilerek özel makro yazılabilir.
    // Şimdilik sadece memcpy yapalım ve platform bağımsızlığı için dikkatli olunması gerektiğini not edelim.
    // Doğru çözüm: https://stackoverflow.com/questions/3082531/uint64-t-endianness-conversion
    
    // Genellikle 64-bit değerler için endian dönüşümü manuel olarak yapılır:
    uint664_t net_value = 0;
    for (int i = 0; i < 8; ++i) {
        net_value |= ((uint64_t)((value >> (i * 8)) & 0xFF)) << ((7 - i) * 8);
    }
    memcpy(buffer + offset, &net_value, sizeof(uint64_t));
    return sizeof(uint64_t);
}

size_t fe_net_write_float(uint8_t* buffer, size_t offset, float value) {
    if (!buffer) return 0;
    // Float değerleri IEEE 754 standardında genellikle 4 bayttır.
    // Endianness dönüşümü için genellikle baytlarını doğrudan kopyalarız
    // ve uzaktan gelen baytların aynı IEEE 754 formatında okunmasını bekleriz.
    // Ancak bazı sistemlerde float'ın iç temsili farklı olabilir, bu durumda
    // daha karmaşık bir seri hale getirme gerekebilir (örn. string'e dönüştürme).
    // Basitlik için, baytlarını kopyalayalım:
    memcpy(buffer + offset, &value, sizeof(float));
    return sizeof(float);
}

size_t fe_net_write_string(uint8_t* buffer, size_t offset, const char* str, size_t max_len) {
    if (!buffer || !str) return 0;

    size_t str_len = strlen(str);
    if (str_len > max_len) {
        FE_LOG_WARN("String to write is too long (%zu bytes), truncating to %zu bytes.", str_len, max_len);
        str_len = max_len;
    }

    // Önce string uzunluğunu (uint16_t olarak) yaz
    size_t bytes_written = fe_net_write_uint16(buffer, offset, (uint16_t)str_len);
    if (bytes_written == 0) return 0;
    offset += bytes_written;

    // Sonra string verisini yaz
    memcpy(buffer + offset, str, str_len);
    bytes_written += str_len;

    return bytes_written;
}


size_t fe_net_read_uint8(const uint8_t* buffer, size_t offset, uint8_t* value_out) {
    if (!buffer || !value_out) return 0;
    *value_out = buffer[offset];
    return sizeof(uint8_t);
}

size_t fe_net_read_uint16(const uint8_t* buffer, size_t offset, uint16_t* value_out) {
    if (!buffer || !value_out) return 0;
    uint16_t net_value;
    memcpy(&net_value, buffer + offset, sizeof(uint16_t));
    *value_out = FE_NTOHS(net_value);
    return sizeof(uint16_t);
}

size_t fe_net_read_uint32(const uint8_t* buffer, size_t offset, uint32_t* value_out) {
    if (!buffer || !value_out) return 0;
    uint32_t net_value;
    memcpy(&net_value, buffer + offset, sizeof(uint32_t));
    *value_out = FE_NTOHL(net_value);
    return sizeof(uint32_t);
}

size_t fe_net_read_uint64(const uint8_t* buffer, size_t offset, uint64_t* value_out) {
    if (!buffer || !value_out) return 0;
    uint64_t net_value;
    memcpy(&net_value, buffer + offset, sizeof(uint64_t));
    
    // Manual endian conversion for 64-bit value:
    uint64_t host_value = 0;
    for (int i = 0; i < 8; ++i) {
        host_value |= ((uint64_t)((net_value >> (i * 8)) & 0xFF)) << ((7 - i) * 8);
    }
    *value_out = host_value;
    return sizeof(uint64_t);
}

size_t fe_net_read_float(const uint8_t* buffer, size_t offset, float* value_out) {
    if (!buffer || !value_out) return 0;
    // Float değerleri için doğrudan kopyalama:
    memcpy(value_out, buffer + offset, sizeof(float));
    return sizeof(float);
}

size_t fe_net_read_string(const uint8_t* buffer, size_t offset, char* str_out, size_t max_len) {
    if (!buffer || !str_out || max_len == 0) return 0;

    uint16_t str_len;
    size_t bytes_read = fe_net_read_uint16(buffer, offset, &str_len);
    if (bytes_read == 0) return 0;
    offset += bytes_read;

    // Hedef tamponun yeterince büyük olup olmadığını kontrol et (null sonlandırıcı dahil)
    if (str_len >= max_len) {
        FE_LOG_WARN("Received string length (%hu) exceeds output buffer capacity (%zu). Truncating.", str_len, max_len);
        str_len = (uint16_t)(max_len - 1); // Null sonlandırıcı için yer bırak
    }

    memcpy(str_out, buffer + offset, str_len);
    str_out[str_len] = '\0'; // Null sonlandırıcı ekle
    bytes_read += str_len;

    return bytes_read;
}
