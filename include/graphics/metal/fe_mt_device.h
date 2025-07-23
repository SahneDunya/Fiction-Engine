#ifndef FE_MT_DEVICE_H
#define FE_MT_DEVICE_H

#import <Metal/Metal.h> // Metal framework başlığı
#import <QuartzCore/CAMetalLayer.h> // CAMetalLayer için

#include <stdbool.h>
#include <stdint.h>

#include "graphics/fe_renderer.h" // fe_renderer_error_t için

// --- Metal Cihaz Hata Kodları ---
typedef enum fe_mt_device_error {
    FE_MT_DEVICE_SUCCESS = 0,
    FE_MT_DEVICE_NOT_INITIALIZED,
    FE_MT_DEVICE_NO_SUITABLE_DEVICE,        // Uygun Metal cihazı bulunamadı
    FE_MT_DEVICE_COMMAND_QUEUE_CREATION_FAILED, // Command Queue oluşturma hatası
    FE_MT_DEVICE_LIBRARY_CREATION_FAILED,   // Shader kütüphanesi oluşturma hatası
    FE_MT_DEVICE_UNKNOWN_ERROR
} fe_mt_device_error_t;

// --- FE Metal Cihaz Yapısı (Gizli Tutaç) ---
// Bu, dahili Metal cihaz nesnelerini kapsüller.
// Objective-C nesneleri için ARC (Automatic Reference Counting) kullanılır,
// bu yüzden manuel retain/release gerekmez, ancak NULL'a set etmek ve fe_free kullanmak
// C tabanlı bellek yönetimiyle tutarlılık sağlar.
typedef struct fe_mt_device {
    __strong id<MTLDevice> metal_device;        // Ana Metal mantıksal cihaz (fiziksel GPU'yu temsil eder)
    __strong id<MTLCommandQueue> command_queue; // Komut göndermek için kuyruk
    __strong id<MTLLibrary> default_library;    // Varsayılan shader kütüphanesi

    // Render pass'ler ve diğer kaynaklar için genel bir tampon önceliği ayarı
    MTLResourceOptions default_buffer_resource_options;

    // Diğer gerekli Metal nesneleri veya bilgiler burada tutulabilir.
} fe_mt_device_t;

// --- Fonksiyonlar ---

/**
 * @brief En uygun Metal cihazını seçer ve ilişkili kaynakları oluşturur.
 * Bu fonksiyon, bir MTLDevice'ı seçer, bir MTLCommandQueue ve varsayılan bir MTLLibrary oluşturur.
 *
 * @param enable_gpu_validation GPU doğrulamasının etkinleştirilip etkinleştirilmeyeceği.
 * @return fe_mt_device_t* Oluşturulan Metal cihazına işaretçi, başarısız olursa NULL.
 */
fe_mt_device_t* fe_mt_device_create(bool enable_gpu_validation);

/**
 * @brief Metal cihazını yok eder ve tüm ilişkili kaynakları serbest bırakır.
 *
 * @param device Yok edilecek Metal cihaz nesnesi.
 */
void fe_mt_device_destroy(fe_mt_device_t* device);

// --- Yardımcı Fonksiyonlar (Dahili veya Debug Amaçlı) ---

/**
 * @brief En iyi MTLDevice'ı seçer (genellikle harici GPU tercih edilir).
 * @return id<MTLDevice> Seçilen MTLDevice nesnesi, bulunamazsa nil.
 */
id<MTLDevice> fe_mt_device_select_best_device();

#endif // FE_MT_DEVICE_H
