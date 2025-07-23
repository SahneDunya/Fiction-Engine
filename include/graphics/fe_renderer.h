#ifndef FE_RENDERER_H
#define FE_RENDERER_H

#include <stdbool.h>
#include <stdint.h> // uint32_t için

// --- Renderleyici Hata Kodları ---
typedef enum fe_renderer_error {
    FE_RENDERER_SUCCESS = 0,
    FE_RENDERER_NOT_INITIALIZED,
    FE_RENDERER_API_NOT_SUPPORTED,
    FE_RENDERER_DEVICE_CREATION_FAILED,
    FE_RENDERER_SWAPCHAIN_FAILED,
    FE_RENDERER_COMMAND_BUFFER_FAILED,
    FE_RENDERER_RENDER_PASS_FAILED,
    FE_RENDERER_FRAMEBUFFER_FAILED,
    FE_RENDERER_RESOURCE_CREATION_FAILED,
    FE_RENDERER_INVALID_PARAMETER,
    FE_RENDERER_OUT_OF_MEMORY,
    FE_RENDERER_UNKNOWN_ERROR
} fe_renderer_error_t;

// --- Desteklenen Grafik API'ları ---
typedef enum fe_graphics_api {
    FE_GRAPHICS_API_VULKAN = 0,
    FE_GRAPHICS_API_DIRECTX, // Windows için DirectX 12 varsayılacak
    FE_GRAPHICS_API_METAL,   // macOS/iOS için
    FE_GRAPHICS_API_UNKNOWN
} fe_graphics_api_t;

// --- Renderleyici Yapılandırma ---
typedef struct fe_renderer_config {
    fe_graphics_api_t api_type;          // Kullanılacak grafik API'sı
    uint32_t window_width;               // Pencere genişliği
    uint32_t window_height;              // Pencere yüksekliği
    bool vsync_enabled;                  // VSync etkinleştirilsin mi?
    bool enable_validation_layers;       // Geliştirme için doğrulama katmanlarını etkinleştir (sadece DEBUG modunda)
    const char* application_name;        // Uygulama adı (API'lar için gerekli olabilir)
} fe_renderer_config_t;


// --- Renderleyici Kontrol Fonksiyonları ---

/**
 * @brief Renderleyici sistemini başlatır.
 * Belirtilen grafik API'sını seçer ve temel grafik kaynaklarını (cihaz, takas zinciri vb.) oluşturur.
 *
 * @param config Renderleyici yapılandırma ayarları.
 * @return fe_renderer_error_t Başarılı olursa FE_RENDERER_SUCCESS, aksi takdirde bir hata kodu.
 */
fe_renderer_error_t fe_renderer_init(const fe_renderer_config_t* config);

/**
 * @brief Renderleyici sistemini kapatır.
 * Tüm grafik kaynaklarını serbest bırakır ve API bağlantısını keser.
 */
void fe_renderer_shutdown();

/**
 * @brief Renderleme döngüsünün bir karesini başlatır.
 * Komut arabelleği kaydını başlatır, takas zincirinden bir sonraki görüntüyü alır.
 *
 * @param delta_time Geçen delta süresi (saniye).
 * @return fe_renderer_error_t Başarılı olursa FE_RENDERER_SUCCESS, aksi takdirde bir hata kodu.
 */
fe_renderer_error_t fe_renderer_begin_frame(float delta_time);

/**
 * @brief Sahneyi renderler.
 * Geometrik verileri, materyalleri, ışıkları vb. işleme komutlarını kaydeder.
 * Bu fonksiyon, alt seviye render pass'leri ve çizim komutlarını çağıracaktır.
 */
void fe_renderer_render_scene();

/**
 * @brief Renderleme döngüsünün bir karesini sonlandırır.
 * Komut arabelleğini gönderir ve render edilmiş görüntüyü ekrana sunar.
 *
 * @return fe_renderer_error_t Başarılı olursa FE_RENDERER_SUCCESS, aksi takdirde bir hata kodu.
 */
fe_renderer_error_t fe_renderer_end_frame();

/**
 * @brief Pencere boyutu değiştiğinde takas zincirini yeniden oluşturur.
 *
 * @param new_width Yeni pencere genişliği.
 * @param new_height Yeni pencere yüksekliği.
 * @return fe_renderer_error_t Başarılı olursa FE_RENDERER_SUCCESS, aksi takdirde bir hata kodu.
 */
fe_renderer_error_t fe_renderer_on_window_resize(uint32_t new_width, uint32_t new_height);

// --- Renderleyici Bilgi Fonksiyonları ---

/**
 * @brief Renderleyicinin başlatılıp başlatılmadığını kontrol eder.
 * @return bool Başlatıldıysa true, aksi takdirde false.
 */
bool fe_renderer_is_initialized();

/**
 * @brief Şu anda kullanılan grafik API'sını döndürür.
 * @return fe_graphics_api_t Aktif grafik API'sı.
 */
fe_graphics_api_t fe_renderer_get_active_api();

/**
 * @brief Renderleyicinin dahili durumunu veya istatistiklerini yazdırır.
 */
void fe_renderer_print_stats();

#endif // FE_RENDERER_H
