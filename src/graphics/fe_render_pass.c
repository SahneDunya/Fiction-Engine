#include "graphics/fe_render_pass.h"
#include "include/utils/fe_logger.h"
#include "include/memory/fe_memory_manager.h" // Bellek tahsisi için

// --- API'ya Özgü Render Pass Başlıkları (İleri Bildirimler) ---
// Bu, fe_vk_render_pass.h'te tanımlanan gerçek Vulkan implementasyonunu çağıracaktır.
#ifdef FE_BUILD_VULKAN_RENDERER
#include "graphics/vulkan/fe_vk_render_pass.h" // fe_vk_render_pass_create, fe_vk_render_pass_destroy vb.
#endif

// --- Dahili Render Pass Yapısı (Opaque Handle'ın İçeriği) ---
// Bu yapı API'ya özgü render pass nesnesini ve diğer gerekli bilgileri tutar.
// Kullanıcılar bu yapının içeriğini doğrudan görmez veya erişmez.
struct fe_render_pass {
    fe_graphics_api_t api_type;
    // API'ya özgü render pass nesnesi için genel bir işaretçi
    // Örneğin: Vulkan için VkRenderPass, DirectX için ID3D12RootSignature'a karşılık gelebilir.
    void* api_handle;

    // Oluşturma bilgileri (yeniden oluşturma veya hata ayıklama için kopyalanabilir)
    // fe_render_pass_create_info_t create_info; // Gerekirse kopyalanabilir
};


// --- Fonksiyon İşaretçileri Tablosu (API Bağımsızlığı İçin) ---
// Her bir grafik API'sı için render pass fonksiyonlarının bir tablosu.
static struct {
    fe_render_pass_t* (*create_func)(const fe_render_pass_create_info_t*);
    void (*destroy_func)(fe_render_pass_t*);
    void (*begin_func)(fe_render_pass_t*, void*, const float*, uint32_t);
    void (*end_func)(fe_render_pass_t*);
    bool is_initialized; // Bu sistemin başlatılıp başlatılmadığını belirtir
} fe_render_pass_api_functions;

// --- Dahili Başlatma/Kapatma (fe_renderer tarafından çağrılır) ---
// Bu fonksiyonlar fe_renderer tarafından dahili olarak çağrılacaktır.
// Direkt dışarıya açılmaz.
void fe_render_pass_init_api_functions(fe_graphics_api_t api_type) {
    if (fe_render_pass_api_functions.is_initialized) {
        FE_LOG_WARN("Render pass API functions already initialized.");
        return;
    }

    switch (api_type) {
        case FE_GRAPHICS_API_VULKAN:
#ifdef FE_BUILD_VULKAN_RENDERER
            fe_render_pass_api_functions.create_func = fe_vk_render_pass_create;
            fe_render_pass_api_functions.destroy_func = fe_vk_render_pass_destroy;
            fe_render_pass_api_functions.begin_func = fe_vk_render_pass_begin;
            fe_render_pass_api_functions.end_func = fe_vk_render_pass_end;
            fe_render_pass_api_functions.is_initialized = true;
            FE_LOG_DEBUG("Render Pass functions set for Vulkan API.");
#else
            FE_LOG_ERROR("Vulkan renderer not built, cannot set Render Pass functions for Vulkan.");
#endif
            break;
        case FE_GRAPHICS_API_DIRECTX:
#ifdef FE_BUILD_DIRECTX_RENDERER
            // fe_render_pass_api_functions.create_func = fe_dx_render_pass_create;
            // ... diğer DirectX fonksiyon işaretçileri
            FE_LOG_ERROR("DirectX Render Pass implementation not yet available.");
#endif
            break;
        case FE_GRAPHICS_API_METAL:
#ifdef FE_BUILD_METAL_RENDERER
            // fe_render_pass_api_functions.create_func = fe_mt_render_pass_create;
            // ... diğer Metal fonksiyon işaretçileri
            FE_LOG_ERROR("Metal Render Pass implementation not yet available.");
#endif
            break;
        default:
            FE_LOG_ERROR("Unsupported graphics API for Render Pass function setup: %d", api_type);
            break;
    }
}

// Bu da fe_renderer tarafından çağrılır
void fe_render_pass_shutdown_api_functions() {
    if (!fe_render_pass_api_functions.is_initialized) {
        FE_LOG_WARN("Render pass API functions not initialized, nothing to shutdown.");
        return;
    }
    memset(&fe_render_pass_api_functions, 0, sizeof(fe_render_pass_api_functions));
    FE_LOG_DEBUG("Render Pass API functions cleared.");
}


// --- Render Pass Fonksiyonları Uygulaması ---

fe_render_pass_t* fe_render_pass_create(const fe_render_pass_create_info_t* create_info) {
    if (!fe_render_pass_api_functions.is_initialized || fe_render_pass_api_functions.create_func == NULL) {
        FE_LOG_ERROR("Render Pass system not initialized or create function not set.");
        return NULL;
    }
    if (create_info == NULL) {
        FE_LOG_ERROR("fe_render_pass_create: create_info is NULL.");
        return NULL;
    }

    // Gerçek API'ya özgü oluşturma fonksiyonunu çağır
    fe_render_pass_t* rp = fe_render_pass_api_functions.create_func(create_info);
    if (rp == NULL) {
        FE_LOG_ERROR("Failed to create API-specific render pass.");
        return NULL;
    }
    FE_LOG_INFO("Render Pass created successfully.");
    return rp;
}

void fe_render_pass_destroy(fe_render_pass_t* render_pass) {
    if (!fe_render_pass_api_functions.is_initialized || fe_render_pass_api_functions.destroy_func == NULL) {
        FE_LOG_WARN("Render Pass system not initialized or destroy function not set. Cannot destroy.");
        return;
    }
    if (render_pass == NULL) {
        FE_LOG_WARN("Attempted to destroy NULL render pass.");
        return;
    }

    // Gerçek API'ya özgü yok etme fonksiyonunu çağır
    fe_render_pass_api_functions.destroy_func(render_pass);
    FE_LOG_INFO("Render Pass destroyed.");
}

void fe_render_pass_begin(fe_render_pass_t* render_pass, void* framebuffer,
                          const float* clear_values, uint32_t clear_value_count) {
    if (!fe_render_pass_api_functions.is_initialized || fe_render_pass_api_functions.begin_func == NULL) {
        FE_LOG_ERROR("Render Pass system not initialized or begin function not set. Cannot begin pass.");
        return;
    }
    if (render_pass == NULL || framebuffer == NULL) {
        FE_LOG_ERROR("fe_render_pass_begin: render_pass or framebuffer is NULL.");
        return;
    }
    // Gerçek API'ya özgü başlangıç fonksiyonunu çağır
    fe_render_pass_api_functions.begin_func(render_pass, framebuffer, clear_values, clear_value_count);
}

void fe_render_pass_end(fe_render_pass_t* render_pass) {
    if (!fe_render_pass_api_functions.is_initialized || fe_render_pass_api_functions.end_func == NULL) {
        FE_LOG_ERROR("Render Pass system not initialized or end function not set. Cannot end pass.");
        return;
    }
    if (render_pass == NULL) {
        FE_LOG_ERROR("fe_render_pass_end: render_pass is NULL.");
        return;
    }
    // Gerçek API'ya özgü bitiş fonksiyonunu çağır
    fe_render_pass_api_functions.end_func(render_pass);
}

// TODO: Bu fonksiyonun fe_renderer.c'de mi yoksa fe_render_pass.c'de mi kalacağına karar verilmeli.
// Şimdilik API bağımsızlığını korumak için burada tutuldu.
// Log seviyesi enum'ını string'e çevirmek için geçici bir yardımcı fonksiyon.
// fe_logger.c'deki fonksiyonun dışarıya açılmadığı varsayılır.
// Normalde bu tür bir utility fonksiyonu fe_utils.h gibi bir yerde tanımlanabilir.
const char* fe_log_level_to_string(fe_log_level_t level); // fe_logger.h'den forward declaration

char* fe_image_format_to_string_debug(fe_image_format_t format) {
    switch (format) {
        case FE_IMAGE_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
        case FE_IMAGE_FORMAT_B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
        case FE_IMAGE_FORMAT_R16G16B16A16_SFLOAT: return "R16G16B16A16_SFLOAT";
        case FE_IMAGE_FORMAT_D32_SFLOAT: return "D32_SFLOAT";
        case FE_IMAGE_FORMAT_D24_UNORM_S8_UINT: return "D24_UNORM_S8_UINT";
        default: return "UNKNOWN_FORMAT";
    }
}
