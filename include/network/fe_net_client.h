#ifndef FE_NET_CLIENT_H
#define FE_NET_CLIENT_H

#include "core/utils/fe_types.h"
#include "core/containers/fe_buffer.h" // fe_buffer_t for send/recv queues
#include "core/memory/fe_memory_manager.h" // For memory management
#include "network/fe_net_socket.h"      // Underlying socket layer

// --- Ağ İstemci Durumları ---
typedef enum fe_client_state {
    FE_CLIENT_STATE_DISCONNECTED,      // Bağlı değil
    FE_CLIENT_STATE_RESOLVING_ADDRESS, // Adres çözümleniyor
    FE_CLIENT_STATE_CONNECTING,        // Bağlanmaya çalışılıyor (non-blocking connect)
    FE_CLIENT_STATE_CONNECTED,         // Başarıyla bağlı
    FE_CLIENT_STATE_DISCONNECTING,     // Bağlantı kesiliyor (graceful shutdown)
    FE_CLIENT_STATE_ERROR              // Bir hata oluştu ve istemci hatalı durumda
} fe_client_state_t;

// --- Geri Çağırma Fonksiyonları (Callbacks) ---
// Bu callback'ler, istemci olayları hakkında üst katmanları bilgilendirmek için kullanılır.

/**
 * @brief İstemci sunucuya başarıyla bağlandığında çağrılır.
 * @param user_data fe_net_client_init fonksiyonunda sağlanan kullanıcı verisi.
 */
typedef void (*PFN_fe_client_on_connected)(void* user_data);

/**
 * @brief İstemci sunucuyla bağlantısı kesildiğinde çağrılır (beklenmedik şekilde veya kasıtlı).
 * @param user_data fe_net_client_init fonksiyonunda sağlanan kullanıcı verisi.
 * @param error_code Bağlantı kesilmesine neden olan hata kodu (eğer bir hata varsa), FE_NET_SUCCESS başarılı kesilmeyi gösterir.
 */
typedef void (*PFN_fe_client_on_disconnected)(void* user_data, fe_net_error_t error_code);

/**
 * @brief Sunucudan veri alındığında çağrılır.
 * @param user_data fe_net_client_init fonksiyonunda sağlanan kullanıcı verisi.
 * @param data Alınan veri tamponunun başlangıcı.
 * @param size Alınan veri boyutu (byte).
 */
typedef void (*PFN_fe_client_on_data_received)(void* user_data, const void* data, size_t size);

/**
 * @brief İstemcide bir ağ hatası oluştuğunda çağrılır (bağlantı hatası, gönderme hatası vb.).
 * @param user_data fe_net_client_init fonksiyonunda sağlanan kullanıcı verisi.
 * @param error_code Oluşan hatanın kodu.
 */
typedef void (*PFN_fe_client_on_error)(void* user_data, fe_net_error_t error_code);

// --- Ağ İstemci Yapısı ---
typedef struct fe_net_client {
    fe_client_state_t state;
    fe_socket_t* tcp_socket;           // Temel TCP soketi

    fe_buffer_t send_buffer;           // Giden veriler için tampon
    fe_buffer_t recv_buffer;           // Gelen veriler için tampon

    fe_string_t host_name;             // Bağlanılacak sunucunun host adı/IP'si
    uint16_t port;                     // Bağlanılacak sunucunun portu

    // Geri çağırma fonksiyonları
    PFN_fe_client_on_connected on_connected_callback;
    PFN_fe_client_on_disconnected on_disconnected_callback;
    PFN_fe_client_on_data_received on_data_received_callback;
    PFN_fe_client_on_error on_error_callback;

    void* user_data;                   // Geri çağırma fonksiyonlarına geçirilecek kullanıcı verisi

    // Dahili kullanım için
    fe_array_t resolved_addresses;     // Çözümlenen IP adresleri (geçici)
    size_t current_addr_index;         // Bağlanmaya çalışılan adresin indeksi
} fe_net_client_t;

/**
 * @brief Yeni bir ağ istemcisi örneği oluşturur ve başlatır.
 *
 * @param client Başlatılacak fe_net_client_t yapısının işaretçisi.
 * @param send_buffer_capacity Giden veri tamponunun kapasitesi (byte).
 * @param recv_buffer_capacity Gelen veri tamponunun kapasitesi (byte).
 * @param user_data Geri çağırma fonksiyonlarına geçirilecek isteğe bağlı kullanıcı verisi.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_net_client_init(fe_net_client_t* client, size_t send_buffer_capacity, size_t recv_buffer_capacity, void* user_data);

/**
 * @brief Bir ağ istemcisi örneğini temizler ve ilişkili tüm kaynakları serbest bırakır.
 * Bağlantıyı keser ve soketi kapatır.
 * @param client Temizlenecek fe_net_client_t yapısının işaretçisi.
 */
void fe_net_client_shutdown(fe_net_client_t* client);

/**
 * @brief Ağ istemcisini belirli bir host adına ve porta bağlanmaya başlatır.
 * Bu non-blocking bir işlemdir. fe_net_client_update düzenli olarak çağrılmalıdır.
 *
 * @param client Bağlanacak istemcinin işaretçisi.
 * @param host_name Sunucunun host adı (örn. "localhost", "192.168.1.1").
 * @param port Sunucunun port numarası.
 * @return fe_net_error_t Başarı durumunu döner. FE_NET_WOULD_BLOCK bağlantının devam ettiğini belirtir.
 */
fe_net_error_t fe_net_client_connect(fe_net_client_t* client, const char* host_name, uint16_t port);

/**
 * @brief İstemci bağlantısını kesme isteği gönderir.
 * Bu non-blocking bir işlemdir. fe_net_client_update düzenli olarak çağrılmalıdır.
 * @param client Bağlantısı kesilecek istemcinin işaretçisi.
 * @return fe_net_error_t Başarı durumunu döner.
 */
fe_net_error_t fe_net_client_disconnect(fe_net_client_t* client);

/**
 * @brief İstemcinin güncel durumunu işler (veri gönderme/alma, bağlantı durumu güncelleme).
 * Ana oyun döngüsünde veya bir ağ iş parçacığında düzenli olarak çağrılmalıdır.
 * @param client Güncellenecek istemcinin işaretçisi.
 */
void fe_net_client_update(fe_net_client_t* client);

/**
 * @brief Veri gönderim kuyruğuna veri ekler. Veri hemen gönderilmeyebilir,
 * fe_net_client_update çağrıldığında gönderilmeye çalışılır.
 *
 * @param client Verinin ekleneceği istemcinin işaretçisi.
 * @param data Gönderilecek veri.
 * @param size Gönderilecek veri boyutu.
 * @return bool Başarılı ise true, aksi takdirde false (örn. send_buffer dolu).
 */
bool fe_net_client_send_data(fe_net_client_t* client, const void* data, size_t size);

/**
 * @brief İstemcinin geçerli durumunu döndürür.
 * @param client İstemcinin işaretçisi.
 * @return fe_client_state_t İstemcinin geçerli durumu.
 */
fe_client_state_t fe_net_client_get_state(const fe_net_client_t* client);

/**
 * @brief İstemcinin bağlı olduğu sunucunun uzak IP adresini döndürür.
 * Sadece FE_CLIENT_STATE_CONNECTED durumundayken geçerlidir.
 * @param client İstemcinin işaretçisi.
 * @return const fe_ip_address_t* Uzak adresin işaretçisi, bağlı değilse veya hata varsa NULL.
 */
const fe_ip_address_t* fe_net_client_get_remote_address(const fe_net_client_t* client);

// --- Geri Çağırma Ayarlayıcıları ---
/** @brief Bağlantı geri çağırmasını ayarlar. */
void fe_net_client_set_on_connected_callback(fe_net_client_t* client, PFN_fe_client_on_connected callback);
/** @brief Bağlantı kesilme geri çağırmasını ayarlar. */
void fe_net_client_set_on_disconnected_callback(fe_net_client_t* client, PFN_fe_client_on_disconnected callback);
/** @brief Veri alma geri çağırmasını ayarlar. */
void fe_net_client_set_on_data_received_callback(fe_net_client_t* client, PFN_fe_client_on_data_received callback);
/** @brief Hata geri çağırmasını ayarlar. */
void fe_net_client_set_on_error_callback(fe_net_client_t* client, PFN_fe_client_on_error callback);

#endif // FE_NET_CLIENT_H
