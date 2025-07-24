#ifndef FE_APPLICATION_H
#define FE_APPLICATION_H

#include "core/utils/fe_types.h"       // Temel tipler (bool, float, uint64_t, size_t vb.)
#include "core/events/fe_event.h"      // Olay işleme sistemi için
#include "core/memory/fe_memory_manager.h" // Bellek yönetimi

// --- İleri Bildirimler ---
// Bu başlık dosyasının bağımlılıklarını azaltmak için kullanılır.
// Tam yapı tanımları ilgili modüllerin kendi başlık dosyalarında yer alır.
struct fe_window;   // fe_window.h'den gelir
struct fe_renderer; // fe_renderer.h'den gelir
struct fe_input;    // fe_input.h'den gelir
struct fe_audio;    // fe_audio.h'den gelir
struct fe_logger;   // fe_logger.h'den gelir

// --- Uygulama Yapılandırma Yapısı ---
/**
 * @brief Uygulamanın başlangıç ayarlarını içerir.
 */
typedef struct fe_application_config {
    char        app_name[256];      // Uygulamanın adı
    int         window_width;       // Pencere genişliği (piksel)
    int         window_height;      // Pencere yüksekliği (piksel)
    bool        fullscreen;         // Tam ekran modunda başlasın mı?
    bool        vsync_enabled;      // VSync etkinleştirilsin mi?
    float       target_frame_rate;  // Hedef kare hızı (0 ise sınırsız)
    // Diğer yapılandırma ayarları buraya eklenebilir (örn: log seviyesi, varsayılan sahne vb.)
} fe_application_config_t;

// --- Uygulama Modülü Arayüzü ---
/**
 * @brief Oyun motorunun farklı alt sistemlerini temsil eden soyut modül arayüzü.
 * Her modül bu fonksiyon işaretçilerini implemente etmelidir.
 */
typedef struct fe_application_module {
    // Modülün adı (debug/loglama için)
    const char* name;

    // Modülü başlatır. Başarılı ise true döner.
    bool (*initialize)(void);

    // Modülü her karede günceller. delta_time, son kare bitişinden bu yana geçen süredir.
    void (*update)(float delta_time);

    // Modülü kapatır ve tüm kaynaklarını serbest bırakır.
    void (*shutdown)(void);

    // Modülün bir olay dinleyicisi varsa, olayı işler.
    // fe_event_context_t* context, olayın içeriğini taşır.
    bool (*on_event)(fe_event_type type, struct fe_event_context* context);

    // Modülün etkin olup olmadığını döndürür.
    bool (*is_active)(void);
} fe_application_module_t;

// --- Uygulama Durumu Yapısı ---
/**
 * @brief Fiction Engine uygulamasının ana durumu.
 */
typedef struct fe_application_state {
    fe_application_config_t config;        // Uygulama yapılandırması
    struct fe_window* main_window;   // Ana pencereye işaretçi
    bool                    is_running;    // Uygulama döngüsü çalışıyor mu?
    float                   delta_time;    // Son kare bitişinden bu yana geçen süre
    uint64_t                frame_count;   // Toplam kare sayısı
    double                  last_frame_time; // Son karenin başlangıç zamanı (saniye)

    // Modüllerin listesi (örn. statik bir dizi veya dinamik bir konteyner)
    // Burada basitlik için sabit boyutlu bir dizi kullanacağız.
    // Daha büyük bir motor için dinamik bir dizi veya harita tercih edilebilir.
#define FE_MAX_APPLICATION_MODULES 16
    fe_application_module_t* modules[FE_MAX_APPLICATION_MODULES];
    size_t                   module_count;

    // Diğer global durum değişkenleri buraya eklenebilir
    // fe_event_dispatcher* event_dispatcher; // Eğer global bir dispatcher varsa
    // fe_renderer* renderer;
    // fe_input* input_system;
    // fe_audio* audio_system;
    // ...
} fe_application_state_t;


// --- Uygulama Fonksiyonları ---

/**
 * @brief Fiction Engine uygulamasını başlatır ve temel sistemleri kurar.
 *
 * @param config Uygulama yapılandırması.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_application_init(const fe_application_config_t* config);

/**
 * @brief Uygulamanın ana döngüsünü çalıştırır.
 * Bu fonksiyon çağrıldıktan sonra uygulama çalışmaya başlar ve kapanana kadar döngüde kalır.
 */
void fe_application_run(void);

/**
 * @brief Uygulama döngüsünü durdurmak için sinyal verir.
 * Uygulama bir sonraki uygun zamanda kapanacaktır.
 */
void fe_application_quit(void);

/**
 * @brief Uygulama kapatıldığında tüm kaynakları serbest bırakır.
 */
void fe_application_shutdown(void);

/**
 * @brief Uygulamaya yeni bir modül kaydeder.
 * Modüllerin fe_application_init'ten önce kaydedilmesi gereklidir.
 *
 * @param module Kaydedilecek modül arayüzü.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_application_register_module(fe_application_module_t* module);

/**
 * @brief Uygulamanın mevcut durumunu döndürür.
 *
 * @return const fe_application_state_t* Uygulama durumu işaretçisi.
 */
const fe_application_state_t* fe_application_get_state(void);


// --- Dahili Uygulama Olay Dinleyici Fonksiyonu ---
// Pencere ve diğer sistemlerden gelen olayları merkezi olarak işlemek için kullanılır.
bool fe_application_on_event(fe_event_type type, struct fe_event_context* context);


#endif // FE_APPLICATION_H
