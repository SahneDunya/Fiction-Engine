#include "core/fe_application.h"
#include "core/utils/fe_logger.h"      // Loglama için
#include "core/time/fe_time.h"          // Zaman fonksiyonları için (fe_get_time, fe_sleep)
#include "platform/fe_platform.h"       // Platforma özgü işlemler (pencere oluşturma vb.)

// --- Harici Modül Bağımlılıkları (Varsayım) ---
// Bu modüllerin .h dosyalarını include etmemiz ve fe_application_module_t
// tipinde global değişkenlerini fe_application_register_module ile kaydetmemiz gerekecek.
// Örneğin:
// #include "modules/graphics/fe_renderer.h"
// #include "modules/input/fe_input_system.h"
// #include "modules/audio/fe_audio_system.h"
// ...
// Bu örnekte, bunları doğrudan çağırmak yerine, kaydettikleri interface'leri kullanacağız.

// --- Uygulama Durumu (Global, Tekil) ---
// Uygulama durumu genellikle tekil (singleton) bir nesne olarak yönetilir.
static fe_application_state_t g_app_state;

// --- Uygulama Olay Dinleyici Implementasyonu ---
bool fe_application_on_event(fe_event_type type, struct fe_event_context* context) {
    // FE_LOG_DEBUG("Application received event: %d", type); // Debug için çok fazla olabilir

    // Uygulama kapatma olayını işle
    if (type == FE_EVENT_TYPE_WINDOW_CLOSED || type == FE_EVENT_TYPE_KEY_PRESSED) {
        if (type == FE_EVENT_TYPE_KEY_PRESSED) {
            fe_key_event_context_t* key_context = (fe_key_event_context_t*)context;
            if (key_context->key_code == FE_KEY_ESCAPE) { // ESC tuşuna basıldığında uygulamayı kapat
                FE_LOG_INFO("Escape key pressed. Quitting application...");
                fe_application_quit();
                return true; // Olay işlendi
            }
        } else if (type == FE_EVENT_TYPE_WINDOW_CLOSED) {
            FE_LOG_INFO("Window close requested. Quitting application...");
            fe_application_quit();
            return true; // Olay işlendi
        }
    }

    // Olayı kayıtlı tüm modüllere dağıt
    for (size_t i = 0; i < g_app_state.module_count; ++i) {
        if (g_app_state.modules[i]->is_active() && g_app_state.modules[i]->on_event) {
            if (g_app_state.modules[i]->on_event(type, context)) {
                // Eğer bir modül olayı tamamen işlediyse, başka modüllere göndermeyi durdurabiliriz.
                // Bu, olay yayıcısının (event dispatcher) tasarımına bağlıdır.
                // Şimdilik tüm modüllere gitmesine izin verelim.
            }
        }
    }

    return false; // Olay işlenmedi (veya başka bir modül tarafından işlenmesi bekleniyor)
}

// --- Uygulama Fonksiyonları Implementasyonları ---

bool fe_application_init(const fe_application_config_t* config) {
    if (!config) {
        FE_LOG_FATAL("Application config is NULL!");
        return false;
    }

    // Bellek Yöneticisi ve Loglayıcı başlatma (en erken bunlar başlar)
    // fe_memory_manager_init(); // Genellikle main fonksiyonunda çağrılır
    // fe_logger_init();         // Genellikle main fonksiyonunda çağrılır

    FE_LOG_INFO("--- Initializing Fiction Engine Application ---");

    // Yapılandırmayı kopyala
    g_app_state.config = *config;
    g_app_state.is_running = false;
    g_app_state.delta_time = 0.0f;
    g_app_state.frame_count = 0;
    g_app_state.last_frame_time = fe_get_time(); // Zamanlayıcıyı başlat

    // Platform sistemini başlat
    if (!fe_platform_init()) {
        FE_LOG_FATAL("Failed to initialize platform layer!");
        return false;
    }

    // Ana pencereyi oluştur
    g_app_state.main_window = fe_platform_create_window(
        g_app_state.config.app_name,
        g_app_state.config.window_width,
        g_app_state.config.window_height,
        g_app_state.config.fullscreen
    );
    if (!g_app_state.main_window) {
        FE_LOG_FATAL("Failed to create main window!");
        fe_platform_shutdown();
        return false;
    }

    // Pencere olayları için ana uygulama olay dinleyicisini kaydet (platforma özgü olabilir)
    // fe_platform_set_event_callback(fe_application_on_event); // Varsayımsal bir platform fonksiyonu

    // Kayıtlı modülleri başlat
    for (size_t i = 0; i < g_app_state.module_count; ++i) {
        fe_application_module_t* module = g_app_state.modules[i];
        if (module && module->initialize) {
            FE_LOG_INFO("Initializing module: %s...", module->name);
            if (!module->initialize()) {
                FE_LOG_FATAL("Failed to initialize module: %s!", module->name);
                // Başarısız olan modülden önceki modülleri kapat
                for (size_t j = 0; j < i; ++j) {
                    if (g_app_state.modules[j]->shutdown) {
                        g_app_state.modules[j]->shutdown();
                    }
                }
                fe_platform_destroy_window(g_app_state.main_window);
                fe_platform_shutdown();
                return false;
            }
        }
    }

    g_app_state.is_running = true;
    FE_LOG_INFO("Fiction Engine Application initialized successfully.");
    return true;
}

void fe_application_run(void) {
    FE_LOG_INFO("Entering application run loop...");

    while (g_app_state.is_running) {
        // Zamanı güncelle
        double current_time = fe_get_time();
        g_app_state.delta_time = (float)(current_time - g_app_state.last_frame_time);
        g_app_state.last_frame_time = current_time;

        // Hedef kare hızına ulaşmak için bekle (varsa)
        if (g_app_state.config.target_frame_rate > 0) {
            float target_frame_duration = 1.0f / g_app_state.config.target_frame_rate;
            if (g_app_state.delta_time < target_frame_duration) {
                float sleep_time_sec = target_frame_duration - g_app_state.delta_time;
                // fe_sleep milisaniye bekler, bu yüzden saniyeyi milisaniyeye çevir
                fe_sleep((uint32_t)(sleep_time_sec * 1000.0f)); 
                
                // Uyuduktan sonra delta_time'ı yeniden hesapla
                // Bu, gerçek zamanlı simülasyonlar için daha doğru bir yaklaşımdır.
                current_time = fe_get_time();
                g_app_state.delta_time = (float)(current_time - g_app_state.last_frame_time);
                g_app_state.last_frame_time = current_time;
            }
        }

        // Platform olaylarını işle (örn. pencere olayları, girdi)
        // Bu, fe_application_on_event'i tetikleyecektir.
        fe_platform_pump_messages(); 

        // Modülleri güncelle
        for (size_t i = 0; i < g_app_state.module_count; ++i) {
            fe_application_module_t* module = g_app_state.modules[i];
            if (module && module->is_active() && module->update) {
                module->update(g_app_state.delta_time);
            }
        }

        // Pencereyi güncelleyin (örn. takas arabellekleri)
        if (g_app_state.main_window) {
            fe_platform_window_swap_buffers(g_app_state.main_window);
        }
        
        g_app_state.frame_count++;
        // FE_LOG_DEBUG("Frame: %llu, Delta Time: %.4fms, FPS: %.2f", g_app_state.frame_count, g_app_state.delta_time * 1000.0f, 1.0f / g_app_state.delta_time);
    }

    FE_LOG_INFO("Exiting application run loop.");
}

void fe_application_quit(void) {
    g_app_state.is_running = false;
    FE_LOG_INFO("Application quit requested.");
}

void fe_application_shutdown(void) {
    FE_LOG_INFO("--- Shutting down Fiction Engine Application ---");

    // Modülleri ters sırada kapat
    for (int i = (int)g_app_state.module_count - 1; i >= 0; --i) {
        fe_application_module_t* module = g_app_state.modules[i];
        if (module && module->shutdown) {
            FE_LOG_INFO("Shutting down module: %s...", module->name);
            module->shutdown();
        }
    }

    // Ana pencereyi yok et
    if (g_app_state.main_window) {
        fe_platform_destroy_window(g_app_state.main_window);
        g_app_state.main_window = NULL;
    }

    // Platform sistemini kapat
    fe_platform_shutdown();

    // Uygulama durumunu temizle
    memset(&g_app_state, 0, sizeof(fe_application_state_t));

    FE_LOG_INFO("Fiction Engine Application shut down successfully.");

    // Bellek Yöneticisi ve Loglayıcı kapatma (en son bunlar kapanır)
    // fe_logger_shutdown();         // Genellikle main fonksiyonunda çağrılır
    // fe_memory_manager_shutdown(); // Genellikle main fonksiyonunda çağrılır
}

bool fe_application_register_module(fe_application_module_t* module) {
    if (!module) {
        FE_LOG_ERROR("Cannot register NULL module.");
        return false;
    }
    if (g_app_state.module_count >= FE_MAX_APPLICATION_MODULES) {
        FE_LOG_ERROR("Module limit reached (%d). Cannot register module: %s", FE_MAX_APPLICATION_MODULES, module->name);
        return false;
    }

    // Modülün gerekli fonksiyon işaretçilerine sahip olup olmadığını kontrol et (isteğe bağlı ama iyi bir pratik)
    if (!module->name || !module->initialize || !module->update || !module->shutdown || !module->is_active) {
        FE_LOG_ERROR("Module '%s' is missing required function pointers.", module->name ? module->name : "UNKNOWN");
        return false;
    }

    g_app_state.modules[g_app_state.module_count++] = module;
    FE_LOG_INFO("Registered module: %s", module->name);
    return true;
}

const fe_application_state_t* fe_application_get_state(void) {
    return &g_app_state;
}
