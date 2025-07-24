#include "core/utils/fe_logger.h"      // Loglama sistemi
#include "core/memory/fe_memory_manager.h" // Bellek yönetim sistemi
#include "core/time/fe_time.h"          // Zamanlama fonksiyonları (fe_get_time, fe_sleep)
#include "platform/fe_platform.h"       // Platforma özgü işlemler (pencere, girdi, olaylar)

#include "core/fe_application.h"        // Uygulama yaşam döngüsü yönetimi
#include "core/events/fe_event.h"       // Olay sistemi tipleri

// --- Motor Modülleri İçin Dış Bildirimler ---
// Bu modüllerin .h dosyalarını dahil ederek ve fe_application_module_t
// tipinde global değişkenlerini fe_application_register_module ile kaydetmemiz gerekecek.
// Bu örnekte, bu modüllerin ayrı C dosyalarında tanımlandığını varsayıyoruz
// ve burada sadece bunların başlık dosyalarını dahil ediyoruz.

// Örnek olarak eklediğimiz modüllerin başlıkları:
// #include "modules/graphics/fe_renderer.h" // Eğer fe_renderer_module olarak dışarı aktarılıyorsa
// #include "modules/input/fe_input_system.h" // Eğer fe_input_module olarak dışarı aktarılıyorsa
// #include "modules/audio/fe_audio_system.h" // Eğer fe_audio_module olarak dışarı aktarılıyorsa

// Daha önceki fe_application.c örneğimizde kullandığımız basit örnek modülleri buraya taşıyalım.
// Gerçek bir motor uygulamasında bu modüller kendi .c/.h dosyalarında olurdu.

// Grafik Modülü Tanımı
static bool graphics_initialize(void) { FE_LOG_INFO("Graphics Module: Initialized."); return true; }
static void graphics_update(float dt) { /* Gerçek render komutları buraya gelir */ }
static void graphics_shutdown(void) { FE_LOG_INFO("Graphics Module: Shut down."); }
static bool graphics_on_event(fe_event_type type, struct fe_event_context* context) {
    if (type == FE_EVENT_TYPE_WINDOW_RESIZED) {
        fe_window_resize_event_context_t* res_ctx = (fe_window_resize_event_context_t*)context;
        FE_LOG_INFO("Graphics Module: Window Resized to %dx%d", res_ctx->width, res_ctx->height);
    } else if (type == FE_EVENT_TYPE_KEY_PRESSED) {
        fe_key_event_context_t* key_ctx = (fe_key_event_context_t*)context;
        if (key_ctx->key_code == FE_KEY_G && key_ctx->modifiers & FE_MOD_CONTROL) {
            FE_LOG_INFO("Graphics Module: Ctrl+G pressed! Performing graphics debug action.");
            return true; // Olayı işledik
        }
    }
    return false;
}
static bool graphics_is_active(void) { return true; }

// Grafik modülü struct'ı (global veya bir modül yöneticisine kayıt için)
fe_application_module_t g_graphics_module = {
    .name = "Graphics",
    .initialize = graphics_initialize,
    .update = graphics_update,
    .shutdown = graphics_shutdown,
    .on_event = graphics_on_event,
    .is_active = graphics_is_active
};

// Ses Modülü Tanımı
static bool audio_initialize(void) { FE_LOG_INFO("Audio Module: Initialized."); return true; }
static void audio_update(float dt) { /* Ses çalma, mixer güncellemeleri buraya gelir */ }
static void audio_shutdown(void) { FE_LOG_INFO("Audio Module: Shut down."); }
static bool audio_on_event(fe_event_type type, struct fe_event_context* context) {
    if (type == FE_EVENT_TYPE_KEY_PRESSED) {
        fe_key_event_context_t* key_ctx = (fe_key_event_context_t*)context;
        if (key_ctx->key_code == FE_KEY_M) {
            FE_LOG_INFO("Audio Module: 'M' key pressed. Toggling mute!");
            return true;
        }
    }
    return false;
}
static bool audio_is_active(void) { return true; }

// Ses modülü struct'ı
fe_application_module_t g_audio_module = {
    .name = "Audio",
    .initialize = audio_initialize,
    .update = audio_update,
    .shutdown = audio_shutdown,
    .on_event = audio_on_event,
    .is_active = audio_is_active
};

// --- Uygulama Başlangıç Noktası ---
int main(int argc, char** argv) {
    // 1. Temel Sistemleri Başlatma
    // Bu sistemler, diğer her şeyden önce başlatılmalıdır,
    // çünkü hata ayıklama ve bellek tahsisi için gereklidirler.
    fe_memory_manager_init();
    fe_logger_init();
    FE_LOG_INFO("Fiction Engine Main Entry Point Started.");

    // 2. Uygulama Yapılandırmasını Oluşturma
    // Bu yapılandırma, uygulamanın başlangıç davranışını belirler.
    fe_application_config_t config = {
        .app_name = "Fiction Engine Demo", // Uygulamanızın adı
        .window_width = 1600,            // Pencere başlangıç genişliği
        .window_height = 900,            // Pencere başlangıç yüksekliği
        .fullscreen = false,             // Tam ekran modunda başlasın mı?
        .vsync_enabled = true,           // VSync etkinleştirilsin mi? (genellikle tercih edilir)
        .target_frame_rate = 60.0f       // Hedef kare hızı (0.0f sınırsız demektir)
    };

    // 3. Modülleri Kaydetme
    // Uygulama başlamadan önce tüm motor modüllerini kaydedin.
    // fe_application_init bu modülleri başlatma sırasını yönetecek.
    if (!fe_application_register_module(&g_graphics_module)) {
        FE_LOG_FATAL("Failed to register Graphics Module!");
        goto cleanup; // Hata durumunda doğrudan temizleme bölümüne git
    }
    if (!fe_application_register_module(&g_audio_module)) {
        FE_LOG_FATAL("Failed to register Audio Module!");
        goto cleanup;
    }
    // Diğer tüm motor modülleriniz buraya eklenecektir (örn. Physics, UI, AssetManager, GameLogic vb.)

    // 4. Uygulamayı Başlatma ve Çalıştırma
    if (!fe_application_init(&config)) {
        FE_LOG_FATAL("Failed to initialize Fiction Engine Application!");
        goto cleanup;
    }

    // Uygulamanın ana döngüsüne girin.
    // Bu fonksiyon, uygulama kapatılana kadar bloklayacaktır.
    fe_application_run();

cleanup:
    // 5. Uygulama ve Temel Sistemleri Kapatma
    // Uygulama döngüsü bittiğinde veya bir hata oluştuğunda,
    // tüm kaynakları düzgün bir şekilde serbest bırakın.
    FE_LOG_INFO("Application exiting. Initiating shutdown sequence.");
    fe_application_shutdown(); // Uygulamayı ve tüm kayıtlı modülleri kapatır

    // En son bellek yöneticisini ve loglayıcıyı kapatın.
    fe_logger_shutdown();
    fe_memory_manager_shutdown();

    FE_LOG_INFO("Fiction Engine Application gracefully shut down.");

    return 0;
}
