#ifndef FE_NET_PACKET_H
#define FE_NET_PACKET_H

#include "core/utils/fe_types.h"
#include "core/memory/fe_memory_manager.h" // For FE_MALLOC, FE_FREE, etc.

// --- Sabitler ve Makrolar ---

// Paket başlığının boyutu (byte cinsinden)
#define FE_NET_PACKET_HEADER_SIZE 8 
// Paket başlığı: 4 byte Packet Type ID + 4 byte Payload Size

// Paket boyutunun maksimum değeri (payload + header)
// Genel olarak, 1500 civarı Ethernet MTU'suna uygun bir değer tercih edilir.
// Uygulamanın ihtiyacına göre ayarlanabilir.
#define FE_NET_MAX_PACKET_SIZE 1400 

// --- Endianness (Bayt Sıralaması) Makroları ---
// Ağ iletişimi genellikle Network Byte Order (Big Endian) kullanır.
// Sistemin endianness'ına göre dönüştürme yapılması gerekir.

#if defined(_MSC_VER) // Microsoft Visual C++
#include <winsock2.h> // ntohl, htonl
#elif defined(__GNUC__) || defined(__clang__) // GCC ve Clang
#include <arpa/inet.h> // ntohl, htonl (Linux/Unix)
#endif

// Ağ Bayt Sıralamasına (Big Endian) dönüştürme
#ifndef FE_HTONL
#define FE_HTONL(x) htonl(x)
#endif
#ifndef FE_NTOHL
#define FE_NTOHL(x) ntohl(x)
#endif
#ifndef FE_HTONS
#define FE_HTONS(x) htons(x)
#endif
#ifndef FE_NTOHS
#define FE_NTOHS(x) ntohs(x)
#endif

// --- Paket Türleri (Örnek) ---
// Uygulamanızın ihtiyaçlarına göre bu ID'ler genişletilebilir.
// Genellikle bir enum ile temsil edilir ve 4 byte (uint32_t) olarak gönderilir.
typedef enum fe_packet_type {
    FE_PACKET_TYPE_UNKNOWN = 0,
    FE_PACKET_TYPE_CLIENT_HELLO = 1,     // İstemciden ilk bağlantı paketi
    FE_PACKET_TYPE_SERVER_WELCOME = 2,   // Sunucudan hoş geldin paketi
    FE_PACKET_TYPE_PLAYER_MOVE = 3,      // Oyuncu hareket güncellemesi
    FE_PACKET_TYPE_CHAT_MESSAGE = 4,     // Sohbet mesajı
    FE_PACKET_TYPE_DISCONNECT = 5,       // Bağlantı kesme isteği
    FE_PACKET_TYPE_HEARTBEAT = 6,        // Bağlantıyı canlı tutma paketi
    // ... daha fazla oyun içi paket türü eklenebilir
    FE_PACKET_TYPE_MAX_VALUE             // Paket türlerinin toplam sayısı
} fe_packet_type_t;

// --- Paket Başlığı Yapısı ---
// Ağ üzerinden gönderilecek paketin ilk kısmı
// Bu yapı, doğrudan belleğe yazılıp okunmamalıdır.
// Seri hale getirme/seri halden çıkarma fonksiyonları kullanılmalıdır.
typedef struct fe_net_packet_header {
    uint32_t type;     // Paketin türü (fe_packet_type_t)
    uint32_t payload_size; // Paketin veri kısmının boyutu (başlık hariç)
} fe_net_packet_header_t;

// --- Paket Yapısı ---
// Ağ üzerinden gönderilecek/alınacak veriyi ve meta bilgilerini tutar.
// Bu yapı, dinamik olarak tahsis edilen belleği yönetebilir.
typedef struct fe_net_packet {
    fe_net_packet_header_t header; // Paketin başlık bilgileri
    uint8_t* payload;              // Paketin ana veri kısmı (dinamik olarak tahsis edilir)
    size_t capacity;               // Payload tamponunun toplam kapasitesi
} fe_net_packet_t;

/**
 * @brief Yeni bir ağ paketi nesnesi oluşturur ve başlatır.
 *
 * @param packet Başlatılacak fe_net_packet_t yapısının işaretçisi.
 * @param initial_capacity Paket yükü (payload) için başlangıç kapasitesi. 0 olabilir.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_net_packet_init(fe_net_packet_t* packet, size_t initial_capacity);

/**
 * @brief Bir ağ paketi nesnesini temizler ve ilişkili tüm kaynakları (payload belleği) serbest bırakır.
 *
 * @param packet Temizlenecek fe_net_packet_t yapısının işaretçisi.
 */
void fe_net_packet_destroy(fe_net_packet_t* packet);

/**
 * @brief Bir paketin yük (payload) kapasitesini ayarlar veya yeniden boyutlandırır.
 * Mevcut veriler korunur.
 *
 * @param packet İşlem yapılacak fe_net_packet_t yapısının işaretçisi.
 * @param new_capacity Yeni kapasite (byte).
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_net_packet_resize_payload(fe_net_packet_t* packet, size_t new_capacity);

/**
 * @brief Bir paketin başlık ve yükünü (payload) ham bayt dizisine seri hale getirir.
 * Bu fonksiyon, paketi ağ üzerinden göndermeye hazır hale getirir.
 *
 * @param packet Seri hale getirilecek fe_net_packet_t yapısının işaretçisi.
 * @param buffer Seri hale getirilmiş paketin yazılacağı hedef tampon.
 * Bu tamponun boyutu FE_NET_PACKET_HEADER_SIZE + packet->header.payload_size olmalıdır.
 * @param buffer_size Hedef tamponun boyutu. Minimum FE_NET_PACKET_HEADER_SIZE + packet->header.payload_size olmalıdır.
 * @return size_t Başarıyla seri hale getirilen toplam bayt sayısı (başlık + yük). Hata durumunda 0 döner.
 */
size_t fe_net_packet_serialize(const fe_net_packet_t* packet, uint8_t* buffer, size_t buffer_size);

/**
 * @brief Ham bayt dizisini bir fe_net_packet_t yapısına seri halden çıkarır.
 * Bu fonksiyon, ağdan alınan bayt dizisini işlenebilir bir paket haline getirir.
 *
 * @param packet Seri halden çıkarılacak fe_net_packet_t yapısının işaretçisi.
 * Paketin payload'u bu fonksiyon içinde yeniden tahsis edilebilir.
 * @param buffer Seri halden çıkarılacak ham bayt dizisi.
 * @param buffer_size Ham bayt dizisinin toplam boyutu. En az FE_NET_PACKET_HEADER_SIZE olmalıdır.
 * @return bool Başarılı ise true, aksi takdirde false (örn. geçersiz boyut, bellek tahsis hatası).
 */
bool fe_net_packet_deserialize(fe_net_packet_t* packet, const uint8_t* buffer, size_t buffer_size);

/**
 * @brief Paketin yük (payload) kısmına veri yazar. Yük boyutu buna göre güncellenir.
 * Eğer yazılan veri mevcut kapasiteyi aşarsa, payload otomatik olarak yeniden boyutlandırılır.
 *
 * @param packet Verinin yazılacağı fe_net_packet_t yapısının işaretçisi.
 * @param data Yazılacak verinin başlangıcı.
 * @param size Yazılacak verinin boyutu (byte).
 * @return bool Başarılı ise true, aksi takdirde false (bellek tahsis hatası vb.).
 */
bool fe_net_packet_set_payload(fe_net_packet_t* packet, const void* data, size_t size);

/**
 * @brief Paketin türünü ayarlar.
 * @param packet fe_net_packet_t yapısının işaretçisi.
 * @param type Ayarlanacak fe_packet_type_t değeri.
 */
void fe_net_packet_set_type(fe_net_packet_t* packet, fe_packet_type_t type);

/**
 * @brief Paketin türünü döndürür.
 * @param packet fe_net_packet_t yapısının işaretçisi.
 * @return fe_packet_type_t Paketin türü.
 */
fe_packet_type_t fe_net_packet_get_type(const fe_net_packet_t* packet);

/**
 * @brief Paketin yük (payload) boyutunu döndürür.
 * @param packet fe_net_packet_t yapısının işaretçisi.
 * @return uint32_t Paketin yük boyutu.
 */
uint32_t fe_net_packet_get_payload_size(const fe_net_packet_t* packet);

/**
 * @brief Paketin yük (payload) verisine işaretçi döndürür.
 * @param packet fe_net_packet_t yapısının işaretçisi.
 * @return const uint8_t* Yük verisine sabit işaretçi. Eğer yük yoksa veya paket geçersizse NULL.
 */
const uint8_t* fe_net_packet_get_payload_ptr(const fe_net_packet_t* packet);

// --- Yardımcı Fonksiyonlar: Veri Seri Hale Getirme/Seri Halden Çıkarma ---
// Bu fonksiyonlar, farklı veri türlerini ağ bayt sıralamasına uygun şekilde
// bir tampona yazmak veya bir tampondaki veriyi okumak için kullanılır.
// Bir 'yazma' veya 'okuma' işaretçisi (offset) alarak çalışırlar.

/**
 * @brief Bir uint8_t değerini tampondaki belirli bir ofsete yazar.
 * @param buffer Yazılacak tampon.
 * @param offset Yazılacak ofset.
 * @param value Yazılacak değer.
 * @return size_t Yazılan bayt sayısı (1).
 */
size_t fe_net_write_uint8(uint8_t* buffer, size_t offset, uint8_t value);

/**
 * @brief Bir uint16_t değerini tampondaki belirli bir ofsete yazar (Network Byte Order).
 * @param buffer Yazılacak tampon.
 * @param offset Yazılacak ofset.
 * @param value Yazılacak değer.
 * @return size_t Yazılan bayt sayısı (2).
 */
size_t fe_net_write_uint16(uint8_t* buffer, size_t offset, uint16_t value);

/**
 * @brief Bir uint32_t değerini tampondaki belirli bir ofsete yazar (Network Byte Order).
 * @param buffer Yazılacak tampon.
 * @param offset Yazılacak ofset.
 * @param value Yazılacak değer.
 * @return size_t Yazılan bayt sayısı (4).
 */
size_t fe_net_write_uint32(uint8_t* buffer, size_t offset, uint32_t value);

/**
 * @brief Bir uint64_t değerini tampondaki belirli bir ofsete yazar (Network Byte Order).
 * @param buffer Yazılacak tampon.
 * @param offset Yazılacak ofset.
 * @param value Yazılacak değer.
 * @return size_t Yazılan bayt sayısı (8).
 */
size_t fe_net_write_uint64(uint8_t* buffer, size_t offset, uint64_t value);

/**
 * @brief Bir float değerini tampondaki belirli bir ofsete yazar (Network Byte Order).
 * IEEE 754 tek duyarlıklı formatında baytların kopyalanması beklenir.
 * @param buffer Yazılacak tampon.
 * @param offset Yazılacak ofset.
 * @param value Yazılacak değer.
 * @return size_t Yazılan bayt sayısı (4).
 */
size_t fe_net_write_float(uint8_t* buffer, size_t offset, float value);

/**
 * @brief Bir string'i tampondaki belirli bir ofsete yazar. String boyutu önce yazılır (uint16_t).
 * @param buffer Yazılacak tampon.
 * @param offset Yazılacak ofset.
 * @param str Yazılacak string.
 * @param max_len String için maksimum yazılabilir uzunluk (null sonlandırıcı hariç).
 * @return size_t Yazılan toplam bayt sayısı (boyut + string verisi). 0 ise hata.
 */
size_t fe_net_write_string(uint8_t* buffer, size_t offset, const char* str, size_t max_len);

/**
 * @brief Bir uint8_t değerini tampondaki belirli bir ofsetten okur.
 * @param buffer Okunacak tampon.
 * @param offset Okunacak ofset.
 * @param value_out Okunan değerin yazılacağı işaretçi.
 * @return size_t Okunan bayt sayısı (1). 0 ise okuma başarısız.
 */
size_t fe_net_read_uint8(const uint8_t* buffer, size_t offset, uint8_t* value_out);

/**
 * @brief Bir uint16_t değerini tampondaki belirli bir ofsetten okur (Network Byte Order).
 * @param buffer Okunacak tampon.
 * @param offset Okunacak ofset.
 * @param value_out Okunan değerin yazılacağı işaretçi.
 * @return size_t Okunan bayt sayısı (2). 0 ise okuma başarısız.
 */
size_t fe_net_read_uint16(const uint8_t* buffer, size_t offset, uint16_t* value_out);

/**
 * @brief Bir uint32_t değerini tampondaki belirli bir ofsetten okur (Network Byte Order).
 * @param buffer Okunacak tampon.
 * @param offset Okunacak ofset.
 * @param value_out Okunan değerin yazılacağı işaretçi.
 * @return size_t Okunan bayt sayısı (4). 0 ise okuma başarısız.
 */
size_t fe_net_read_uint32(const uint8_t* buffer, size_t offset, uint32_t* value_out);

/**
 * @brief Bir uint64_t değerini tampondaki belirli bir ofsetten okur (Network Byte Order).
 * @param buffer Okunacak tampon.
 * @param offset Okunacak ofset.
 * @param value_out Okunan değerin yazılacağı işaretçi.
 * @return size_t Okunan bayt sayısı (8). 0 ise okuma başarısız.
 */
size_t fe_net_read_uint64(const uint8_t* buffer, size_t offset, uint64_t* value_out);

/**
 * @brief Bir float değerini tampondaki belirli bir ofsetten okur (Network Byte Order).
 * IEEE 754 tek duyarlıklı formatında baytların kopyalanması beklenir.
 * @param buffer Okunacak tampon.
 * @param offset Okunacak ofset.
 * @param value_out Okunan değerin yazılacağı işaretçi.
 * @return size_t Okunan bayt sayısı (4). 0 ise okuma başarısız.
 */
size_t fe_net_read_float(const uint8_t* buffer, size_t offset, float* value_out);

/**
 * @brief Bir string'i tampondaki belirli bir ofsetten okur. String boyutu önce okunur (uint16_t).
 * @param buffer Okunacak tampon.
 * @param offset Okunacak ofset.
 * @param str_out Okunan stringin yazılacağı tampon. Null sonlandırıcı ile yazılır.
 * @param max_len Hedef tamponun maksimum kapasitesi (null sonlandırıcı dahil).
 * @return size_t Okunan toplam bayt sayısı (boyut + string verisi). 0 ise hata veya tampon yetersiz.
 */
size_t fe_net_read_string(const uint8_t* buffer, size_t offset, char* str_out, size_t max_len);

#endif // FE_NET_PACKET_H
