#ifndef FE_NET_SERVER_H
#define FE_NET_SERVER_H

#include "core/utils/fe_types.h"
#include "core/containers/fe_buffer.h"       // fe_buffer_t for send/recv queues
#include "core/containers/fe_array.h"        // fe_array_t for managing clients
#include "core/memory/fe_memory_manager.h"   // For memory management
#include "network/fe_net_socket.h"           // Underlying socket layer

// --- Ağ Sunucusu Durumları ---
typedef enum fe_server_state {
    FE_SERVER_STATE_STOPPED,           // Sunucu durduruldu veya hiç başlatılmadı
    FE_SERVER_STATE_STARTING,          // Başlatılıyor (dinleme soketi oluşturuluyor)
    FE_SERVER_STATE_RUNNING,           // Dinlemede ve istemci bağlantılarını kabul ediyor
    FE_SERVER_STATE_SHUTTING_DOWN,     // Kapanıyor (mevcut bağlantıları sonlandırıyor)
    FE_SERVER_STATE_ERROR              // Bir hata oluştu ve sunucu hatalı durumda
} fe_server_state_t;

// --- İstemci Tanımlayıcı ---
// Her bir bağlı istemciyi benzersiz şekilde temsil eden ID
typedef uint32_t fe_client_id_t;
#define FE_INVALID_CLIENT_ID 0

// --- Bağlı İstemci Yapısı ---
// Sunucunun her bir bağlı istemci için tuttuğu bilgiler
typedef struct fe_connected_client {
    fe_client_id_t id;                 // Benzersiz istemci ID'si
    fe_socket_t* socket;               // Bu istemciye ait TCP soketi
    fe_buffer_t send_buffer;           // Bu istemciye giden veriler için tampon
    fe_buffer_t recv_buffer;           // Bu istemciden gelen veriler için tampon
    bool is_pending_disconnect;        // İstemci bağlantısının kesilmesi bekleniyor mu?
    void* user_data;                   // İstemciye özgü isteğe bağlı kullanıcı verisi
    // İhtiyaca göre başka istemciye özgü durumlar eklenebilir (örn. ping, son aktivite zamanı)
} fe_connected_client_t;

// --- Geri Çağırma Fonksiyonları (Callbacks) ---
// Bu callback'ler, sunucu olayları hakkında üst katmanları bilgilendirmek için kullanılır.

/**
 * @brief Sunucu başarıyla başlatıldığında ve dinlemeye başladığında çağrılır.
 * @param user_data fe_net_server_init fonksiyonunda sağlanan sunucuya özgü kullanıcı verisi.
 */
typedef void (*PFN_fe_server_on_started)(void* user_data);

/**
 * @brief Sunucu durdurulduğunda veya kapanma tamamlandığında çağrılır.
 * @param user_data fe_net_server_init fonksiyonunda sağlanan sunucuya özgü kullanıcı verisi.
 * @param error_code Sunucunun kapanmasına neden olan hata kodu (eğer bir hata varsa), FE_NET_SUCCESS başarılı kapanmayı gösterir.
 */
typedef void (*PFN_fe_server_on_stopped)(void* user_data, fe_net_error_t error_code);

/**
 * @brief Yeni bir istemci sunucuya başarıyla bağlandığında çağrılır.
 * @param user_data fe_net_server_init fonksiyonunda sağlanan sunucuya özgü kullanıcı verisi.
 * @param client_id Bağlanan yeni istemcinin benzersiz ID'si.
 * @param remote_addr Bağlanan istemcinin uzak IP adresi ve portu.
 */
typedef void (*PFN_fe_server_on_client_connected)(void* user_data, fe_client_id_t client_id, const fe_ip_address_t* remote_addr);

/**
 * @brief Bir istemcinin sunucuyla bağlantısı kesildiğinde çağrılır (beklenmedik şekilde veya kasıtlı).
 * @param user_data fe_net_server_init fonksiyonunda sağlanan sunucuya özgü kullanıcı verisi.
 * @param client_id Bağlantısı kesilen istemcinin benzersiz ID'si.
 * @param error_code Bağlantı kesilmesine neden olan hata kodu (eğer bir hata varsa), FE_NET_SUCCESS başarılı kesilmeyi gösterir.
 */
typedef void (*PFN_fe_server_on_client_disconnected)(void* user_data, fe_client_id_t client_id, fe_net_error_t error_code);

/**
 * @brief Bir istemciden veri alındığında çağrılır.
 * @param user_data fe_net_server_init fonksiyonunda sağlanan sunucuya özgü kullanıcı verisi.
 * @param client_id Verinin alındığı istemcinin benzersiz ID'si.
 * @param data Alınan veri tamponunun başlangıcı.
 * @param size Alınan veri boyutu (byte).
 */
typedef void (*PFN_fe_server_on_data_received)(void* user_data, fe_client_id_t client_id, const void* data, size_t size);

/**
 * @brief Sunucuda bir ağ hatası oluştuğunda çağrılır (dinleme hatası, soket hatası vb.).
 * @param user_data fe_net_server_init fonksiyonunda sağlanan sunucuya özgü kullanıcı verisi.
 * @param error_code Oluşan hatanın kodu.
 */
typedef void (*PFN_fe_server_on_error)(void* user_data, fe_net_error_t error_code);


// --- Ağ Sunucusu Yapısı ---
typedef struct fe_net_server {
    fe_server_state_t state;
    fe_socket_t* listen_socket;        // İstemci bağlantılarını kabul etmek için dinleme soketi
    uint16_t port;                     // Sunucunun dinlediği port

    fe_array_t connected_clients;      // Bağlı fe_connected_client_t yapılarını tutan dinamik dizi
    fe_client_id_t next_client_id;     // Bir sonraki atanacak istemci ID'si

    // Geri çağırma fonksiyonları
    PFN_fe_server_on_started on_started_callback;
    PFN_fe_server_on_stopped on_stopped_callback;
    PFN_fe_server_on_client_connected on_client_connected_callback;
    PFN_fe_server_on_client_disconnected on_client_disconnected_callback;
    PFN_fe_server_on_data_received on_data_received_callback;
    PFN_fe_server_on_error on_error_callback;

    void* user_data;                   // Geri çağırma fonksiyonlarına geçirilecek sunucuya özgü kullanıcı verisi

    size_t client_send_buffer_capacity; // Her istemci için gönderim tamponu kapasitesi
    size_t client_recv_buffer_capacity; // Her istemci için alım tamponu kapasitesi

} fe_net_server_t;

/**
 * @brief Yeni bir ağ sunucusu örneği oluşturur ve başlatır.
 *
 * @param server Başlatılacak fe_net_server_t yapısının işaretçisi.
 * @param client_send_buffer_capacity Her bir istemci için giden veri tamponunun kapasitesi (byte).
 * @param client_recv_buffer_capacity Her bir istemci için gelen veri tamponunun kapasitesi (byte).
 * @param max_clients Sunucunun aynı anda kabul edeceği maksimum istemci sayısı.
 * @param user_data Geri çağırma fonksiyonlarına geçirilecek isteğe bağlı sunucuya özgü kullanıcı verisi.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_net_server_init(fe_net_server_t* server, size_t client_send_buffer_capacity, size_t client_recv_buffer_capacity, size_t max_clients, void* user_data);

/**
 * @brief Bir ağ sunucusu örneğini temizler ve ilişkili tüm kaynakları serbest bırakır.
 * Mevcut tüm istemci bağlantılarını keser ve dinleme soketini kapatır.
 * @param server Temizlenecek fe_net_server_t yapısının işaretçisi.
 */
void fe_net_server_shutdown(fe_net_server_t* server);

/**
 * @brief Ağ sunucusunu belirli bir portta dinlemeye başlatır.
 * Bu non-blocking bir işlemdir. fe_net_server_update düzenli olarak çağrılmalıdır.
 *
 * @param server Başlatılacak sunucunun işaretçisi.
 * @param port Sunucunun dinleyeceği port numarası.
 * @param backlog Bağlantı kabul kuyruğunun maksimum uzunluğu.
 * @return fe_net_error_t Başarı durumunu döner. FE_NET_SUCCESS başarılı başlatmayı, diğerleri hatayı belirtir.
 */
fe_net_error_t fe_net_server_start(fe_net_server_t* server, uint16_t port, int backlog);

/**
 * @brief Sunucunun dinlemesini durdurur ve tüm mevcut istemci bağlantılarını sonlandırır.
 * Bu non-blocking bir işlemdir. fe_net_server_update düzenli olarak çağrılmalıdır.
 * @param server Durdurulacak sunucunun işaretçisi.
 * @return fe_net_error_t Başarı durumunu döner.
 */
fe_net_error_t fe_net_server_stop(fe_net_server_t* server);

/**
 * @brief Sunucunun güncel durumunu işler (yeni bağlantıları kabul etme, veri gönderme/alma,
 * bağlantısı kesilen istemcileri temizleme).
 * Ana oyun döngüsünde veya bir ağ iş parçacığında düzenli olarak çağrılmalıdır.
 * @param server Güncellenecek sunucunun işaretçisi.
 */
void fe_net_server_update(fe_net_server_t* server);

/**
 * @brief Belirli bir istemciye veri gönderim kuyruğuna veri ekler. Veri hemen gönderilmeyebilir,
 * fe_net_server_update çağrıldığında gönderilmeye çalışılır.
 *
 * @param server Verinin gönderileceği sunucunun işaretçisi.
 * @param client_id Verinin gönderileceği istemcinin ID'si.
 * @param data Gönderilecek veri.
 * @param size Gönderilecek veri boyutu.
 * @return bool Başarılı ise true, aksi takdirde false (örn. istemci bulunamadı veya send_buffer dolu).
 */
bool fe_net_server_send_data(fe_net_server_t* server, fe_client_id_t client_id, const void* data, size_t size);

/**
 * @brief Belirli bir istemcinin bağlantısını kesme isteği gönderir.
 * Bu non-blocking bir işlemdir. fe_net_server_update düzenli olarak çağrılmalıdır.
 * @param server Sunucunun işaretçisi.
 * @param client_id Bağlantısı kesilecek istemcinin ID'si.
 * @return bool Başarılı ise true, aksi takdirde false (örn. istemci bulunamadı).
 */
bool fe_net_server_disconnect_client(fe_net_server_t* server, fe_client_id_t client_id);

/**
 * @brief Sunucunun geçerli durumunu döndürür.
 * @param server Sunucunun işaretçisi.
 * @return fe_server_state_t Sunucunun geçerli durumu.
 */
fe_server_state_t fe_net_server_get_state(const fe_net_server_t* server);

/**
 * @brief Belirli bir istemciye ait kullanıcı verisini (user_data) döndürür.
 * @param server Sunucunun işaretçisi.
 * @param client_id İstemcinin ID'si.
 * @return void* İstemciye ait kullanıcı verisinin işaretçisi, bulunamazsa NULL.
 */
void* fe_net_server_get_client_user_data(const fe_net_server_t* server, fe_client_id_t client_id);

/**
 * @brief Belirli bir istemciye ait kullanıcı verisini (user_data) ayarlar.
 * @param server Sunucunun işaretçisi.
 * @param client_id İstemcinin ID'si.
 * @param user_data Ayarlanacak kullanıcı verisi.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_net_server_set_client_user_data(fe_net_server_t* server, fe_client_id_t client_id, void* user_data);

/**
 * @brief Bağlı tüm istemcilerin ID'lerini bir diziye kopyalar.
 * @param server Sunucunun işaretçisi.
 * @param client_ids_out Kopyalanacak ID'lerin hedef dizisi. NULL olabilir, bu durumda sadece boyut hesaplanır.
 * @param max_count Çıkış dizisinin maksimum kapasitesi.
 * @return size_t Kopyalanan (veya toplam) istemci ID'lerinin sayısı.
 */
size_t fe_net_server_get_connected_client_ids(const fe_net_server_t* server, fe_client_id_t* client_ids_out, size_t max_count);

// --- Geri Çağırma Ayarlayıcıları ---
/** @brief Sunucu başlatıldığında geri çağırmayı ayarlar. */
void fe_net_server_set_on_started_callback(fe_net_server_t* server, PFN_fe_server_on_started callback);
/** @brief Sunucu durdurulduğunda geri çağırmayı ayarlar. */
void fe_net_server_set_on_stopped_callback(fe_net_server_t* server, PFN_fe_server_on_stopped callback);
/** @brief İstemci bağlandığında geri çağırmayı ayarlar. */
void fe_net_server_set_on_client_connected_callback(fe_net_server_t* server, PFN_fe_server_on_client_connected callback);
/** @brief İstemci bağlantısı kesildiğinde geri çağırmayı ayarlar. */
void fe_net_server_set_on_client_disconnected_callback(fe_net_server_t* server, PFN_fe_server_on_client_disconnected callback);
/** @brief İstemciden veri alındığında geri çağırmayı ayarlar. */
void fe_net_server_set_on_data_received_callback(fe_net_server_t* server, PFN_fe_server_on_data_received callback);
/** @brief Sunucuda hata oluştuğunda geri çağırmayı ayarlar. */
void fe_net_server_set_on_error_callback(fe_net_server_t* server, PFN_fe_server_on_error callback);

#endif // FE_NET_SERVER_H
