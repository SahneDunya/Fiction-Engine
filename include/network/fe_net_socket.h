#ifndef FE_NET_SOCKET_H
#define FE_NET_SOCKET_H

#include "core/utils/fe_types.h" // fe_string_t, bool için
#include "core/containers/fe_array.h" // IP adreslerini saklamak için
#include "core/memory/fe_memory_manager.h" // Bellek yönetimi için

// Platforma özgü tanımlamalar
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h> // getaddrinfo için
    // Link Winsock Library
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET fe_socket_handle_t;
    #define FE_INVALID_SOCKET INVALID_SOCKET
    #define FE_SOCKET_ERROR SOCKET_ERROR
#else // POSIX (Linux, macOS, etc.)
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>     // getaddrinfo için
    #include <unistd.h>    // close için
    #include <fcntl.h>     // fcntl için
    typedef int fe_socket_handle_t;
    #define FE_INVALID_SOCKET -1
    #define FE_SOCKET_ERROR -1
#endif

// --- Ağ Hata Kodları ---
typedef enum fe_net_error {
    FE_NET_SUCCESS = 0,
    FE_NET_INVALID_ARGUMENT,
    FE_NET_INIT_FAILED,          // Ağ alt sistemi başlatılamadı
    FE_NET_SOCKET_CREATE_FAILED, // Soket oluşturulamadı
    FE_NET_BIND_FAILED,          // Soket bağlanamadı
    FE_NET_LISTEN_FAILED,        // Soket dinlemeye alınamadı
    FE_NET_ACCEPT_FAILED,        // Bağlantı kabul edilemedi
    FE_NET_CONNECT_FAILED,       // Bağlantı kurulamadı
    FE_NET_SEND_FAILED,          // Veri gönderilemedi
    FE_NET_RECV_FAILED,          // Veri alınamadı
    FE_NET_WOULD_BLOCK,          // Bloklamayan sokette işlem beklemede (EWOULDBLOCK/WSAEWOULDBLOCK)
    FE_NET_NO_DATA,              // Alma işlemi sırasında veri yok
    FE_NET_RESOLVE_FAILED,       // Host adı çözümlenemedi
    FE_NET_SET_OPT_FAILED,       // Soket seçeneği ayarlanamadı
    FE_NET_ADDR_INFO_FAILED,     // Adres bilgisi alınamadı
    FE_NET_NOT_INITIALIZED,      // Ağ alt sistemi başlatılmadı
    FE_NET_UNKNOWN_ERROR         // Bilinmeyen hata
} fe_net_error_t;

// --- Soket Tipleri ---
typedef enum fe_socket_type {
    FE_SOCKET_TYPE_TCP, // Güvenilir, bağlantı odaklı (Stream)
    FE_SOCKET_TYPE_UDP  // Güvenilmez, bağlantısız (Datagram)
} fe_socket_type_t;

// --- IP Adresi Yapısı ---
typedef struct fe_ip_address {
    fe_string_t ip_string; // Örn: "127.0.0.1", "::1"
    uint16_t port;
    struct sockaddr_storage addr; // Adresin ham yapısı (IPv4 veya IPv6)
    socklen_t addr_len;
    bool is_ipv6;
} fe_ip_address_t;

// --- Soket Yapısı ---
typedef struct fe_socket {
    fe_socket_handle_t handle; // OS soket tanıtıcısı
    fe_socket_type_t type;     // TCP veya UDP
    bool is_blocking;          // Bloklayan mı, bloklamayan mı
    bool is_bound;             // Bağlı mı
    bool is_listening;         // Dinlemede mi (sadece TCP)
    bool is_connected;         // Bağlı mı (sadece TCP)
    fe_ip_address_t local_addr; // Kendi adresi
    fe_ip_address_t remote_addr; // Bağlı olduğu uzak adres (sadece TCP)
} fe_socket_t;

// --- Genel Ağ Başlatma/Kapatma ---

/**
 * @brief Ağ alt sistemini başlatır. Windows'da Winsock'u başlatır.
 * Diğer platformlarda genellikle gerek yoktur ancak uyumluluk için çağrılabilir.
 * Uygulama başlangıcında bir kez çağrılmalıdır.
 * @return fe_net_error_t Başarı durumunu döner.
 */
fe_net_error_t fe_net_init();

/**
 * @brief Ağ alt sistemini kapatır. Windows'da Winsock'u temizler.
 * Uygulama kapanışında bir kez çağrılmalıdır.
 */
void fe_net_shutdown();

// --- IP Adresi Fonksiyonları ---

/**
 * @brief Bir host adı ve port için IP adreslerini çözer.
 * Çözümlenen adresler 'out_addresses' dizisine eklenir.
 *
 * @param host_name Çözümlenecek host adı (örn. "localhost", "example.com").
 * @param port Çözümlenecek port numarası.
 * @param out_addresses Çözümlenen IP adreslerinin ekleneceği fe_array_t (fe_ip_address_t tipinde).
 * @param socket_type İstenen soket tipi (FE_SOCKET_TYPE_TCP veya FE_SOCKET_TYPE_UDP).
 * @return fe_net_error_t Başarı durumunu döner.
 */
fe_net_error_t fe_net_resolve_address(const char* host_name, uint16_t port, fe_array_t* out_addresses, fe_socket_type_t socket_type);

/**
 * @brief fe_ip_address_t yapısını temizler ve ilişkili belleği serbest bırakır.
 * @param addr Temizlenecek adresin işaretçisi.
 */
void fe_ip_address_destroy(fe_ip_address_t* addr);

// --- Soket Oluşturma/Kapatma ---

/**
 * @brief Yeni bir soket oluşturur.
 *
 * @param type Soket tipi (TCP veya UDP).
 * @param is_blocking Soket bloklayan modda mı (true) yoksa bloklamayan modda mı (false) olacak.
 * @return fe_socket_t* Yeni oluşturulan soket nesnesinin işaretçisi, hata durumunda NULL.
 */
fe_socket_t* fe_net_socket_create(fe_socket_type_t type, bool is_blocking);

/**
 * @brief Bir soketi kapatır ve ilişkili belleği serbest bırakır.
 *
 * @param sock Kapatılacak soketin işaretçisi.
 */
void fe_net_socket_destroy(fe_socket_t* sock);

// --- Soket Bağlama/Dinleme/Bağlantı Kurma ---

/**
 * @brief Bir soketi belirli bir IP adresine ve porta bağlar.
 *
 * @param sock Bağlanacak soket.
 * @param address Soketin bağlanacağı IP adresi ve port.
 * @return fe_net_error_t Başarı durumunu döner.
 */
fe_net_error_t fe_net_socket_bind(fe_socket_t* sock, const fe_ip_address_t* address);

/**
 * @brief TCP soketini gelen bağlantıları dinlemeye başlar.
 * Sadece FE_SOCKET_TYPE_TCP tipli soketler için geçerlidir.
 *
 * @param sock Dinlenecek soket.
 * @param backlog Beklemedeki bağlantı kuyruğunun maksimum boyutu.
 * @return fe_net_error_t Başarı durumunu döner.
 */
fe_net_error_t fe_net_socket_listen(fe_socket_t* sock, int backlog);

/**
 * @brief Gelen bir TCP bağlantısını kabul eder.
 * Sadece dinlemede olan FE_SOCKET_TYPE_TCP tipli soketler için geçerlidir.
 * Bloklamayan modda `FE_NET_WOULD_BLOCK` dönebilir.
 *
 * @param listener_sock Dinleyen soket.
 * @param out_accepted_addr Kabul edilen bağlantının uzak adresi (isteğe bağlı, NULL olabilir).
 * @return fe_socket_t* Kabul edilen yeni bağlantının soket nesnesinin işaretçisi, hata durumunda NULL.
 */
fe_socket_t* fe_net_socket_accept(fe_socket_t* listener_sock, fe_ip_address_t* out_accepted_addr);

/**
 * @brief Uzak bir adrese TCP bağlantısı kurar.
 * Sadece FE_SOCKET_TYPE_TCP tipli soketler için geçerlidir.
 * Bloklamayan modda `FE_NET_WOULD_BLOCK` veya `FE_NET_CONNECT_FAILED` (eğer bağlantı hala devam ediyorsa) dönebilir.
 *
 * @param sock Bağlantı kuracak soket.
 * @param remote_address Bağlanılacak uzak adres.
 * @return fe_net_error_t Başarı durumunu döner.
 */
fe_net_error_t fe_net_socket_connect(fe_socket_t* sock, const fe_ip_address_t* remote_address);

/**
 * @brief Bloklamayan TCP bağlantısının tamamlanıp tamamlanmadığını kontrol eder.
 *
 * @param sock Kontrol edilecek soket.
 * @return fe_net_error_t Bağlantı başarılı ise FE_NET_SUCCESS, devam ediyorsa FE_NET_WOULD_BLOCK, hata varsa ilgili hata kodu.
 */
fe_net_error_t fe_net_socket_check_connect(fe_socket_t* sock);


// --- Veri Gönderme/Alma ---

/**
 * @brief Bağlantı odaklı soket üzerinden veri gönderir (TCP).
 * Bloklamayan modda `FE_NET_WOULD_BLOCK` dönebilir.
 *
 * @param sock Veri gönderecek soket.
 * @param data Gönderilecek veri tamponu.
 * @param size Gönderilecek veri boyutu (byte).
 * @param out_sent_bytes Gönderilen gerçek byte sayısı (isteğe bağlı, NULL olabilir).
 * @return fe_net_error_t Başarı durumunu döner.
 */
fe_net_error_t fe_net_socket_send(fe_socket_t* sock, const void* data, size_t size, size_t* out_sent_bytes);

/**
 * @brief Bağlantı odaklı soketten veri alır (TCP).
 * Bloklamayan modda `FE_NET_NO_DATA` veya `FE_NET_WOULD_BLOCK` dönebilir.
 * Bağlantı kapatıldıysa 0 byte dönebilir.
 *
 * @param sock Veri alacak soket.
 * @param buffer Verinin alınacağı tampon.
 * @param buffer_size Tamponun maksimum boyutu (byte).
 * @param out_received_bytes Alınan gerçek byte sayısı (isteğe bağlı, NULL olabilir).
 * @return fe_net_error_t Başarı durumunu döner.
 */
fe_net_error_t fe_net_socket_receive(fe_socket_t* sock, void* buffer, size_t buffer_size, size_t* out_received_bytes);

/**
 * @brief Bağlantısız soket üzerinden veri gönderir (UDP).
 *
 * @param sock Veri gönderecek soket.
 * @param data Gönderilecek veri tamponu.
 * @param size Gönderilecek veri boyutu (byte).
 * @param remote_address Verinin gönderileceği uzak adres.
 * @param out_sent_bytes Gönderilen gerçek byte sayısı (isteğe bağlı, NULL olabilir).
 * @return fe_net_error_t Başarı durumunu döner.
 */
fe_net_error_t fe_net_socket_send_to(fe_socket_t* sock, const void* data, size_t size, const fe_ip_address_t* remote_address, size_t* out_sent_bytes);

/**
 * @brief Bağlantısız soketten veri alır (UDP).
 * Bloklamayan modda `FE_NET_NO_DATA` veya `FE_NET_WOULD_BLOCK` dönebilir.
 *
 * @param sock Veri alacak soket.
 * @param buffer Verinin alınacağı tampon.
 * @param buffer_size Tamponun maksimum boyutu (byte).
 * @param out_sender_address Verinin geldiği uzak adres (isteğe bağlı, NULL olabilir).
 * @param out_received_bytes Alınan gerçek byte sayısı (isteğe bağlı, NULL olabilir).
 * @return fe_net_error_t Başarı durumunu döner.
 */
fe_net_error_t fe_net_socket_receive_from(fe_socket_t* sock, void* buffer, size_t buffer_size, fe_ip_address_t* out_sender_address, size_t* out_received_bytes);

// --- Soket Seçenekleri ---

/**
 * @brief Soket blocking modunu ayarlar.
 *
 * @param sock Ayarlanacak soket.
 * @param blocking Bloklayan ise true, bloklamayan ise false.
 * @return fe_net_error_t Başarı durumunu döner.
 */
fe_net_error_t fe_net_socket_set_blocking(fe_socket_t* sock, bool blocking);

/**
 * @brief Soket için SO_REUSEADDR seçeneğini ayarlar.
 * Aynı portun birden fazla soket tarafından kullanılmasını sağlar (genellikle sunucu tarafında).
 *
 * @param sock Ayarlanacak soket.
 * @param enable Etkinleştirmek için true, devre dışı bırakmak için false.
 * @return fe_net_error_t Başarı durumunu döner.
 */
fe_net_error_t fe_net_socket_set_reuse_address(fe_socket_t* sock, bool enable);

/**
 * @brief Soketin kendi yerel adresini alır.
 * @param sock Soket.
 * @param out_address Yerel adresin yazılacağı yapı.
 * @return fe_net_error_t Başarı durumunu döner.
 */
fe_net_error_t fe_net_socket_get_local_address(fe_socket_t* sock, fe_ip_address_t* out_address);

/**
 * @brief Soketin bağlı olduğu uzak adresi alır (sadece TCP için geçerli).
 * @param sock Soket.
 * @param out_address Uzak adresin yazılacağı yapı.
 * @return fe_net_error_t Başarı durumunu döner.
 */
fe_net_error_t fe_net_socket_get_remote_address(fe_socket_t* sock, fe_ip_address_t* out_address);


#endif // FE_NET_SOCKET_H
