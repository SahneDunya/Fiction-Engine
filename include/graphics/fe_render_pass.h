#ifndef FE_RENDER_PASS_H
#define FE_RENDER_PASS_H

#include <stdint.h>
#include <stdbool.h>

#include "graphics/renderer/fe_renderer.h" // fe_renderer_error_t için

// --- Ek Türler (Kendi dosyalarına taşınabilir) ---
// Bu örnek için burada tanımlanmıştır.

// Renk formatları (Vulkan/DirectX/Metal uyumlu)
typedef enum fe_image_format {
    FE_IMAGE_FORMAT_UNDEFINED = 0,
    FE_IMAGE_FORMAT_R8G8B8A8_UNORM,  // 32-bit RGBA (yaygın)
    FE_IMAGE_FORMAT_B8G8R8A8_UNORM,  // 32-bit BGRA (Windows'ta yaygın)
    FE_IMAGE_FORMAT_R16G16B16A16_SFLOAT, // HDR veya yüksek hassasiyet için
    FE_IMAGE_FORMAT_D32_SFLOAT,      // 32-bit Derinlik
    FE_IMAGE_FORMAT_D24_UNORM_S8_UINT, // 24-bit Derinlik + 8-bit Stencil
    // Daha fazla format eklenebilir
    FE_IMAGE_FORMAT_COUNT
} fe_image_format_t;

// Yükleme operasyonları
typedef enum fe_attachment_load_op {
    FE_ATTACHMENT_LOAD_OP_LOAD = 0,    // Önceki içeriği yükle
    FE_ATTACHMENT_LOAD_OP_CLEAR,   // Yüklemeden önce temizle
    FE_ATTACHMENT_LOAD_OP_DONT_CARE // Önceki içeriği önemseme
} fe_attachment_load_op_t;

// Depolama operasyonları
typedef enum fe_attachment_store_op {
    FE_ATTACHMENT_STORE_OP_STORE = 0,   // İçeriği belleğe kaydet
    FE_ATTACHMENT_STORE_OP_DONT_CARE // İçeriği kaydetme
} fe_attachment_store_op_t;

// --- Render Pass Yapıları ---

// Render Pass Ek (Attachment) Tanımı
// Bir render pass'in kullandığı bir görüntüyü (renk, derinlik, stencil) tanımlar.
typedef struct fe_render_pass_attachment {
    fe_image_format_t format;             // Ek görüntünün formatı
    fe_attachment_load_op_t load_op;      // Renk/derinlik verilerinin nasıl yükleneceği
    fe_attachment_store_op_t store_op;     // Renk/derinlik verilerinin nasıl depolanacağı
    fe_attachment_load_op_t stencil_load_op;  // Stencil verilerinin nasıl yükleneceği (eğer varsa)
    fe_attachment_store_op_t stencil_store_op; // Stencil verilerinin nasıl depolanacağı (eğer varsa)
    // İlk düzen, nihai düzen vb. gibi API'ya özgü alanlar burada tanımlanmamıştır.
    // Bunlar API'ya özgü render pass yapısına eklenecektir.
} fe_render_pass_attachment_t;

// Alt Geçiş (Subpass) Referansı
// Bir alt geçiş içindeki bir ekin (attachment) kullanımı.
typedef struct fe_subpass_attachment_ref {
    uint32_t attachment_index; // fe_render_pass_create'e geçilen ek dizisindeki indeks
    // Ek kullanım düzeni (API'ya özgü, örn: VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    // Bu, API'ya özgü yapıda tutulur.
} fe_subpass_attachment_ref_t;

// Alt Geçiş (Subpass) Tanımı
// Bir render pass içindeki bağımsız bir işlem adımı.
typedef struct fe_subpass_description {
    uint32_t input_attachment_count;
    fe_subpass_attachment_ref_t* input_attachments; // Input eklentileri (önceki alt geçişlerden)

    uint32_t color_attachment_count;
    fe_subpass_attachment_ref_t* color_attachments; // Renk eklentileri (renderlenen çıktılar)

    fe_subpass_attachment_ref_t* depth_stencil_attachment; // Derinlik/Stencil eklentisi (opsiyonel)

    // Preserve eklentileri, resolve eklentileri vb. daha sonra eklenebilir.
} fe_subpass_description_t;

// Render Geçişi (Render Pass) Yapılandırması
// Bir render pass'in tam tanımı.
typedef struct fe_render_pass_create_info {
    uint32_t attachment_count;
    fe_render_pass_attachment_t* attachments; // Bu render pass'in kullandığı tüm ekler

    uint32_t subpass_count;
    fe_subpass_description_t* subpasses; // Bu render pass'in alt geçişleri

    // Subpass bağımlılıkları daha sonra eklenebilir
    // uint32_t dependency_count;
    // fe_subpass_dependency_t* dependencies;
} fe_render_pass_create_info_t;

// Render Pass Opaque Handle
// Gerçek render pass nesnesine bir tutaç. API bağımlı detaylar bu tutaç içinde gizlenir.
// Örneğin: VkRenderPass, ID3D12RootSignature (DirectX'te benzer bir rol)
typedef struct fe_render_pass fe_render_pass_t; // İleriye dönük bildirim

// --- Render Pass Fonksiyonları ---

/**
 * @brief Yeni bir render pass oluşturur.
 * Bu fonksiyon, alt seviye grafik API'sını kullanarak bir render pass nesnesi yaratır.
 *
 * @param create_info Render pass'in oluşturulması için gerekli yapılandırma.
 * @return fe_render_pass_t* Oluşturulan render pass nesnesine işaretçi, başarısız olursa NULL.
 */
fe_render_pass_t* fe_render_pass_create(const fe_render_pass_create_info_t* create_info);

/**
 * @brief Bir render pass'i yok eder ve ilişkili grafik kaynaklarını serbest bırakır.
 *
 * @param render_pass Yok edilecek render pass nesnesi.
 */
void fe_render_pass_destroy(fe_render_pass_t* render_pass);

// --- Render Pass'i kullanmak için API bağımsız fonksiyonlar ---
// Bu fonksiyonlar fe_renderer.c tarafından dahili olarak çağrılacak veya
// daha yüksek seviyeli renderleme komutları tarafından kullanılacaktır.
// Bunlar, API'ya özgü render pass'i başlatma ve bitirme fonksiyonlarına wrapper'lardır.

/**
 * @brief Belirtilen bir komut arabelleğine bir render pass'in başlatma komutunu kaydeder.
 *
 * @param render_pass Başlatılacak render pass.
 * @param framebuffer Bu render pass için kullanılacak framebuffer.
 * @param clear_values Temizleme operasyonları için kullanılacak renk/derinlik değerleri dizisi.
 * @param clear_value_count clear_values dizisindeki eleman sayısı.
 */
void fe_render_pass_begin(fe_render_pass_t* render_pass, void* framebuffer,
                          const float* clear_values, uint32_t clear_value_count); // clear_values array RGBA (4 floats)

/**
 * @brief Belirtilen bir komut arabelleğine bir render pass'in bitirme komutunu kaydeder.
 *
 * @param render_pass Bitirilecek render pass.
 */
void fe_render_pass_end(fe_render_pass_t* render_pass);

#endif // FE_RENDER_PASS_H
