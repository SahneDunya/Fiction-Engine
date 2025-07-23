#include "graphics/metal/fe_mt_device.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h" // Bellek yönetimi için

// ARC (Automatic Reference Counting) kullanıldığı için manuel retain/release gerekmez.
// Ancak C API'si ile uyumluluk için fe_malloc/fe_free kullanılır.

// --- Dahili Yardımcı Fonksiyonlar ---

// GPU doğrulamasını etkinleştirir (Sadece macOS/iOS geliştirme için)
static void fe_mt_device_enable_gpu_validation(void) {
#ifdef DEBUG
    // Ortam değişkenini ayarlayarak GPU doğrulamasını etkinleştir.
    // Bu genellikle XCode'da scheme ayarlarından yapılır, ancak koddan da ayarlanabilir.
    // setenv("MTL_ENABLE_DEBUG_INFO", "1", 1);
    // setenv("METAL_DEVICE_WRAPPER_TYPE", "1", 1); // Enable validation layer for some specific issues.

    // Not: Kod içinde doğrudan 'enable_gpu_validation' için bir Metal API çağrısı yoktur.
    // Bu, genellikle Xcode Build Scheme'de "Metal API Validation" veya "GPU Frame Capture" etkinleştirilerek yapılır.
    // Ancak, bazen Metal layer'ın belirli davranışlarını etkilemek için ortam değişkenleri kullanılır.
    FE_LOG_INFO("Metal GPU validation / debug info enabled (usually via Xcode scheme or env vars).");
#else
    FE_LOG_DEBUG("Metal GPU validation is not enabled in release builds.");
#endif
}

// En iyi MTLDevice'ı seçer
id<MTLDevice> fe_mt_device_select_best_device(void) {
    id<MTLDevice> selected_device = nil;

    // Sistemdeki tüm Metal cihazlarını al
    NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();

    if ([devices count] == 0) {
        FE_LOG_CRITICAL("No Metal devices found on this system.");
        return nil;
    }

    FE_LOG_INFO("Available Metal devices:");
    for (id<MTLDevice> device in devices) {
        BOOL is_headless = device.isHeadless; // Başsız (ekransız) cihazlar (örn. sunucu GPU'ları)
        BOOL is_low_power = device.isLowPower; // Düşük güç tüketimli entegre GPU'lar
        BOOL is_removable = device.isRemovable; // Harici GPU (eGPU)

        FE_LOG_INFO("  - %s (Headless: %s, Low Power: %s, Removable: %s)",
                    device.name.UTF8String,
                    is_headless ? "Yes" : "No",
                    is_low_power ? "Yes" : "No",
                    is_removable ? "Yes" : "No");

        // Tercih sırası:
        // 1. Ayrık (Discrete) veya Harici (eGPU)
        // 2. Entegre (Integrated)
        // 3. Yazılım (Software) veya Başsız (Headless) - genellikle bunları atlarız
        if (!is_headless && !is_low_power && !is_removable) { // Genellikle ana ayrık GPU
            selected_device = device; // İlk bulunan "iyi" cihazı seç
            break;
        } else if (is_removable && !selected_device) { // İlk eGPU'yu seç
            selected_device = device;
        } else if (is_low_power && !selected_device) { // İlk entegre GPU'yu seç (eğer başka yoksa)
            selected_device = device;
        }
    }

    if (selected_device) {
        FE_LOG_INFO("Selected Metal device: %s", selected_device.name.UTF8String);
    } else {
        // Eğer hiçbir tercih edilen cihaz bulunamazsa, ilk cihazı dene
        selected_device = devices[0];
        FE_LOG_WARN("No preferred Metal device found, falling back to first available device: %s", selected_device.name.UTF8String);
    }

    return selected_device;
}

// --- Ana Cihaz Yönetim Fonksiyonları Uygulaması ---

fe_mt_device_t* fe_mt_device_create(bool enable_gpu_validation) {
    fe_mt_device_t* mt_device = fe_malloc(sizeof(fe_mt_device_t), FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    if (!mt_device) {
        FE_LOG_CRITICAL("Failed to allocate memory for fe_mt_device_t.");
        return NULL;
    }
    // Objective-C pointer'lar için NULL'a set et
    mt_device->metal_device = nil;
    mt_device->command_queue = nil;
    mt_device->default_library = nil;
    mt_device->default_buffer_resource_options = 0; // Varsayılan değer

#ifdef DEBUG
    if (enable_gpu_validation) {
        fe_mt_device_enable_gpu_validation();
    }
#endif

    // 1. En iyi MTLDevice'ı seç
    mt_device->metal_device = fe_mt_device_select_best_device();
    if (mt_device->metal_device == nil) {
        FE_LOG_CRITICAL("Failed to select a suitable Metal device!");
        fe_mt_device_destroy(mt_device); // Oluşturulan yapıyı temizle
        return NULL;
    }

    // 2. Command Queue Oluştur
    // Her cihaz için genellikle bir veya daha fazla command queue oluşturulur.
    // Bu, GPU'ya komut göndermek için kullanılır.
    mt_device->command_queue = [mt_device->metal_device newCommandQueue];
    if (mt_device->command_queue == nil) {
        FE_LOG_CRITICAL("Failed to create Metal command queue!");
        fe_mt_device_destroy(mt_device);
        return NULL;
    }
    FE_LOG_INFO("Metal command queue created.");

    // 3. Varsayılan Shader Kütüphanesini Yükle
    // Genellikle uygulamayla birlikte derlenmiş shader'lar için varsayılan kütüphane kullanılır.
    // Alternatif olarak, dosya sisteminden veya bellekten shader yükleyebilirsiniz.
    NSError* error = nil;
    mt_device->default_library = [mt_device->metal_device newDefaultLibraryWithBundle:[NSBundle mainBundle] error:&error];
    if (mt_device->default_library == nil) {
        FE_LOG_CRITICAL("Failed to create default Metal library! Error: %s", error.localizedDescription.UTF8String);
        fe_mt_device_destroy(mt_device);
        return NULL;
    }
    FE_LOG_INFO("Default Metal shader library loaded.");

    // Varsayılan tampon kaynak seçeneklerini ayarla (genellikle CPU/GPU paylaşımlı bellek için)
    // Bu seçenek, tamponların GPU tarafından verimli bir şekilde erişilebilmesini sağlar.
    mt_device->default_buffer_resource_options = MTLResourceStorageModeShared;
    // Veya: MTLResourceStorageModeManaged (macOS sadece, daha eski GPU'lar için)
    // Veya: MTLResourceStorageModePrivate (sadece GPU erişimi, daha hızlı)

    FE_LOG_INFO("Metal device initialized successfully (%s).", mt_device->metal_device.name.UTF8String);

    return mt_device;
}

void fe_mt_device_destroy(fe_mt_device_t* device) {
    if (!device) {
        FE_LOG_WARN("Attempted to destroy NULL fe_mt_device_t.");
        return;
    }

    // Objective-C nesneleri ARC ile yönetildiği için manuel release gerekmez.
    // Ancak NULL'a set etmek, ARC'nin nesneleri serbest bırakmasına neden olur ve netlik sağlar.
    device->default_library = nil;
    device->command_queue = nil;
    device->metal_device = nil;

    fe_free(device, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    FE_LOG_INFO("fe_mt_device_t object freed.");
}
