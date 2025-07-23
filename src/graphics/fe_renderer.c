#include "graphics/fe_renderer.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h" // Bellek yönetimi için
// Diğer render bileşenleri için başlıklar (gelecekte eklenecek)
 #include "graphics/renderer/fe_render_pass.h"
 #include "graphics/material/fe_material_editor.h"

// --- API'ya Özgü Renderleyici Başlıkları (İleri Bildirimler) ---
// Bunlar gerçekte kendi .h ve .c dosyalarında uygulanacaktır.
// Bu dosya, bu API'lara soyut bir arayüz sağlar.
#ifdef FE_BUILD_VULKAN_RENDERER
#include "graphics/vulkan/fe_vk_renderer.h" // fe_vk_renderer_init, fe_vk_renderer_shutdown vb.
#endif

#ifdef FE_BUILD_DIRECTX_RENDERER
 #include "graphics/directx/fe_dx_renderer.h" // DirectX renderer fonksiyonları
#endif

#ifdef FE_BUILD_METAL_RENDERER
 #include "graphics/metal/fe_mt_renderer.h" // Metal renderer fonksiyonları
#endif

// --- Dahili Renderleyici Durumu ---
typedef struct fe_renderer_state {
    fe_graphics_api_t active_api;
    fe_renderer_config_t config;
    bool is_initialized;

    // Platforma özgü renderleyici fonksiyon işaretçileri
    // Bu, API bağımsızlığı sağlamanın anahtarıdır.
    fe_renderer_error_t (*api_init)(const fe_renderer_config_t*);
    void (*api_shutdown)(void);
    fe_renderer_error_t (*api_begin_frame)(float);
    void (*api_render_scene)(void);
    fe_renderer_error_t (*api_end_frame)(void);
    fe_renderer_error_t (*api_on_window_resize)(uint32_t, uint32_t);

    // Diğer global renderleyici verileri (örneğin, görünüm/projeksiyon matrisleri, ışıklar)
    // Bu kısım daha sonra doldurulacaktır.
} fe_renderer_state_t;

static fe_renderer_state_t g_fe_renderer_state; // Global renderleyici durumu

// --- Renderleyici Kontrol Fonksiyonları Uygulaması ---

fe_renderer_error_t fe_renderer_init(const fe_renderer_config_t* config) {
    if (g_fe_renderer_state.is_initialized) {
        FE_LOG_WARN("Renderer already initialized. Shutting down and re-initializing.");
        fe_renderer_shutdown(); // Yeniden başlatmadan önce kapat
    }

    if (config == NULL) {
        FE_LOG_ERROR("Renderer initialization failed: config is NULL.");
        return FE_RENDERER_INVALID_PARAMETER;
    }

    g_fe_renderer_state.config = *config; // Yapılandırmayı kopyala
    g_fe_renderer_state.active_api = config->api_type;

    // Seçilen API'ye göre fonksiyon işaretçilerini ata
    switch (config->api_type) {
        case FE_GRAPHICS_API_VULKAN:
#ifdef FE_BUILD_VULKAN_RENDERER
            g_fe_renderer_state.api_init = fe_vk_renderer_init;
            g_fe_renderer_state.api_shutdown = fe_vk_renderer_shutdown;
            g_fe_renderer_state.api_begin_frame = fe_vk_renderer_begin_frame;
            g_fe_renderer_state.api_render_scene = fe_vk_renderer_render_scene;
            g_fe_renderer_state.api_end_frame = fe_vk_renderer_end_frame;
            g_fe_renderer_state.api_on_window_resize = fe_vk_renderer_on_window_resize;
            FE_LOG_INFO("Selected graphics API: Vulkan.");
#else
            FE_LOG_ERROR("Vulkan renderer is not built for this configuration.");
            return FE_RENDERER_API_NOT_SUPPORTED;
#endif
            break;
        case FE_GRAPHICS_API_DIRECTX:
#ifdef FE_BUILD_DIRECTX_RENDERER
             g_fe_renderer_state.api_init = fe_dx_renderer_init;
            // ... (Diğer DirectX fonksiyon işaretçileri)
            FE_LOG_INFO("Selected graphics API: DirectX.");
#else
            FE_LOG_ERROR("DirectX renderer is not built for this configuration. "
                         "Please ensure FE_BUILD_DIRECTX_RENDERER is defined.");
            return FE_RENDERER_API_NOT_SUPPORTED;
#endif
            break;
        case FE_GRAPHICS_API_METAL:
#ifdef FE_BUILD_METAL_RENDERER
             g_fe_renderer_state.api_init = fe_mt_renderer_init;
            // ... (Diğer Metal fonksiyon işaretçileri)
            FE_LOG_INFO("Selected graphics API: Metal.");
#else
            FE_LOG_ERROR("Metal renderer is not built for this configuration. "
                         "Please ensure FE_BUILD_METAL_RENDERER is defined.");
            return FE_RENDERER_API_NOT_SUPPORTED;
#endif
            break;
        default:
            FE_LOG_ERROR("Unsupported graphics API selected: %d", config->api_type);
            return FE_RENDERER_API_NOT_SUPPORTED;
    }

    // Seçilen API'nın init fonksiyonunu çağır
    if (g_fe_renderer_state.api_init == NULL) {
        FE_LOG_ERROR("Renderer API init function is NULL for API type %d. Internal error.", config->api_type);
        return FE_RENDERER_UNKNOWN_ERROR;
    }

    fe_renderer_error_t api_err = g_fe_renderer_state.api_init(config);
    if (api_err != FE_RENDERER_SUCCESS) {
        FE_LOG_CRITICAL("Failed to initialize graphics API (%s). Error: %d",
                        fe_log_level_to_string(FE_LOG_LEVEL_ERROR), api_err); // TODO: error code to string
        // API'ya özgü init başarısız olursa fonksiyon işaretçilerini temizle
        memset(&g_fe_renderer_state.api_init, 0, sizeof(void*) * 6); // 6 fonksiyon işaretçisini sıfırla
        g_fe_renderer_state.is_initialized = false;
        return api_err;
    }

    g_fe_renderer_state.is_initialized = true;
    FE_LOG_INFO("Renderer initialized successfully with %s.", fe_log_level_to_string(FE_LOG_LEVEL_INFO)); // TODO: enum to API name string
    return FE_RENDERER_SUCCESS;
}

void fe_renderer_shutdown() {
    if (!g_fe_renderer_state.is_initialized) {
        FE_LOG_WARN("Renderer not initialized, nothing to shut down.");
        return;
    }

    if (g_fe_renderer_state.api_shutdown) {
        g_fe_renderer_state.api_shutdown();
        FE_LOG_INFO("Graphics API shut down.");
    }

    memset(&g_fe_renderer_state, 0, sizeof(fe_renderer_state_t)); // Durumu sıfırla
    FE_LOG_INFO("Renderer shut down.");
}

fe_renderer_error_t fe_renderer_begin_frame(float delta_time) {
    if (!g_fe_renderer_state.is_initialized) {
        FE_LOG_ERROR("Renderer not initialized, cannot begin frame.");
        return FE_RENDERER_NOT_INITIALIZED;
    }
    if (g_fe_renderer_state.api_begin_frame == NULL) {
        FE_LOG_ERROR("Renderer API begin_frame function is NULL.");
        return FE_RENDERER_UNKNOWN_ERROR;
    }
    return g_fe_renderer_state.api_begin_frame(delta_time);
}

void fe_renderer_render_scene() {
    if (!g_fe_renderer_state.is_initialized) {
        FE_LOG_ERROR("Renderer not initialized, cannot render scene.");
        return;
    }
    if (g_fe_renderer_state.api_render_scene == NULL) {
        FE_LOG_ERROR("Renderer API render_scene function is NULL.");
        return;
    }
    // Burada yüksek seviyeli render komutları çağrılacak.
    // Örneğin, renderleyici alt sistemlere (görselleştirme, gölgeleme, vb.) çağrı yapabilir.
    // Bu örnekte doğrudan API'ya özgü render_scene çağrılır.
    g_fe_renderer_state.api_render_scene();
}

fe_renderer_error_t fe_renderer_end_frame() {
    if (!g_fe_renderer_state.is_initialized) {
        FE_LOG_ERROR("Renderer not initialized, cannot end frame.");
        return FE_RENDERER_NOT_INITIALIZED;
    }
    if (g_fe_renderer_state.api_end_frame == NULL) {
        FE_LOG_ERROR("Renderer API end_frame function is NULL.");
        return FE_RENDERER_UNKNOWN_ERROR;
    }
    return g_fe_renderer_state.api_end_frame();
}

fe_renderer_error_t fe_renderer_on_window_resize(uint32_t new_width, uint32_t new_height) {
    if (!g_fe_renderer_state.is_initialized) {
        FE_LOG_ERROR("Renderer not initialized, cannot handle window resize.");
        return FE_RENDERER_NOT_INITIALIZED;
    }
    if (g_fe_renderer_state.api_on_window_resize == NULL) {
        FE_LOG_ERROR("Renderer API on_window_resize function is NULL.");
        return FE_RENDERER_UNKNOWN_ERROR;
    }
    if (new_width == 0 || new_height == 0) {
        FE_LOG_WARN("Window resized to zero dimension (%ux%u). Skipping resize.", new_width, new_height);
        return FE_RENDERER_INVALID_PARAMETER;
    }

    g_fe_renderer_state.config.window_width = new_width;
    g_fe_renderer_state.config.window_height = new_height;

    FE_LOG_INFO("Window resized to %ux%u. Notifying graphics API.", new_width, new_height);
    return g_fe_renderer_state.api_on_window_resize(new_width, new_height);
}

// --- Renderleyici Bilgi Fonksiyonları Uygulaması ---

bool fe_renderer_is_initialized() {
    return g_fe_renderer_state.is_initialized;
}

fe_graphics_api_t fe_renderer_get_active_api() {
    return g_fe_renderer_state.active_api;
}

void fe_renderer_print_stats() {
    if (!g_fe_renderer_state.is_initialized) {
        FE_LOG_WARN("Renderer not initialized, cannot print stats.");
        return;
    }
    FE_LOG_INFO("--- Fiction Engine Renderer Stats ---");
    const char* api_name = "Unknown";
    switch (g_fe_renderer_state.active_api) {
        case FE_GRAPHICS_API_VULKAN: api_name = "Vulkan"; break;
        case FE_GRAPHICS_API_DIRECTX: api_name = "DirectX"; break;
        case FE_GRAPHICS_API_METAL: api_name = "Metal"; break;
        default: break;
    }
    FE_LOG_INFO("  Active API: %s", api_name);
    FE_LOG_INFO("  Window Resolution: %ux%u",
                g_fe_renderer_state.config.window_width, g_fe_renderer_state.config.window_height);
    FE_LOG_INFO("  VSync Enabled: %s", g_fe_renderer_state.config.vsync_enabled ? "Yes" : "No");
#ifdef FE_DEBUG_BUILD
    FE_LOG_INFO("  Validation Layers: %s", g_fe_renderer_state.config.enable_validation_layers ? "Yes" : "No");
#endif
    // API'ya özgü istatistikleri burada çağırabiliriz
     if (g_fe_renderer_state.active_api == FE_GRAPHICS_API_VULKAN && g_fe_renderer_state.api_print_stats) {
          ((void (*)(void))g_fe_renderer_state.api_print_stats)(); // Örnek olarak
     }
    FE_LOG_INFO("-------------------------------------");
}
