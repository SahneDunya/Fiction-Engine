#ifndef FE_MT_PIPELINE_H
#define FE_MT_PIPELINE_H

#import <Metal/Metal.h> // Metal framework başlığı
#import <QuartzCore/CAMetalLayer.h> // CAMetalLayer için

#include <stdbool.h>
#include <stdint.h>

#include "graphics/renderer/fe_renderer.h" // fe_renderer_error_t için
#include "graphics/metal/fe_mt_device.h"   // fe_mt_device_t için

// --- Metal Pipeline Hata Kodları ---
typedef enum fe_mt_pipeline_error {
    FE_MT_PIPELINE_SUCCESS = 0,
    FE_MT_PIPELINE_CREATION_FAILED,
    FE_MT_PIPELINE_SHADER_FUNCTION_NOT_FOUND, // Shader fonksiyonu bulunamadı
    FE_MT_PIPELINE_INVALID_CONFIG,
    FE_MT_PIPELINE_OUT_OF_MEMORY,
    FE_MT_PIPELINE_UNKNOWN_ERROR
} fe_mt_pipeline_error_t;

// --- Vertex Input Layout Elemanı Tanımı ---
// Metal'de bu doğrudan bir yapı olarak tanımlanmaz, MTLVertexDescriptor ile kurulur.
// Bu yapı, kullanıcıdan MTLVertexDescriptor oluşturmak için bilgi toplar.
typedef struct fe_mt_vertex_attribute_desc {
    const char* name;          // (Opsiyonel) Debugging veya bilgi amaçlı
    MTLVertexFormat format;    // Veri formatı (örn. MTLVertexFormatFloat3)
    NSUInteger offset;         // Vertex arabelleği içindeki ofset
    NSUInteger buffer_index;   // Vertex arabelleği dizini
} fe_mt_vertex_attribute_desc_t;

typedef struct fe_mt_vertex_buffer_layout_desc {
    NSUInteger stride;         // Her vertex'in boyutu (byte cinsinden)
    MTLVertexStepFunction step_function; // Vertex başına mı, instance başına mı
    NSUInteger step_rate;      // Adım hızı (instance başına ise)
    NSUInteger buffer_index;   // Vertex arabelleği dizini (layout'un ait olduğu)
} fe_mt_vertex_buffer_layout_desc_t;


// --- Pipeline Yapılandırması ---
// Bir Metal Render Pipeline State oluşturmak için gerekli tüm ayarları kapsar.
typedef struct fe_mt_pipeline_config {
    // Shader Fonksiyon Adları
    const char* vertex_shader_name; // Vertex shader fonksiyonunun adı
    const char* fragment_shader_name; // Fragment shader fonksiyonunun adı

    // Vertex Input Layout (MTLVertexDescriptor için bilgiler)
    NSUInteger vertex_attribute_count;
    fe_mt_vertex_attribute_desc_t* vertex_attributes; // Vertex nitelikleri

    NSUInteger vertex_buffer_layout_count;
    fe_mt_vertex_buffer_layout_desc_t* vertex_buffer_layouts; // Vertex arabelleği düzenleri

    // Render Target Ayarları
    NSUInteger color_attachment_count;
    MTLPixelFormat color_attachment_formats[8]; // Renk eki formatları (maks 8)
    // Renk karıştırma ayarları (MTLRenderPipelineColorAttachmentDescriptor)
    bool blend_enable[8]; // Her renk eki için karıştırma etkinleştirilsin mi?
    MTLBlendFactor source_rgb_blend_factor[8];
    MTLBlendFactor destination_rgb_blend_factor[8];
    MTLBlendOperation rgb_blend_operation[8];
    MTLBlendFactor source_alpha_blend_factor[8];
    MTLBlendFactor destination_alpha_blend_factor[8];
    MTLBlendOperation alpha_blend_operation[8];
    MTLColorWriteMask color_write_mask[8];

    MTLPixelFormat depth_attachment_format; // Derinlik eki formatı
    MTLPixelFormat stencil_attachment_format; // Stencil eki formatı

    // Sample Desc (MSAA için)
    NSUInteger sample_count; // Örnek sayısı (MSAA için)

    // Primitive Topolojisi (Metal'de pipeline'ın bir parçası değildir, Command Encoder'a ayarlanır)
    // Ancak bilgiyi burada tutmak faydalı olabilir, Metal'de bu kısım MTLRenderCommandEncoder'a atanır.
    MTLPrimitiveType primitive_type; // Çizilecek primitiflerin topolojisi (örneğin, MTLPrimitiveTypeTriangle)

    // Pipeline'ın kullanıldığı alt geçiş indeksi (Vulkan'daki gibi doğrudan bir kavram yok, ancak mantıksal olarak düşünülebilir)
    // Metal'de anlamı yoktur, sadece Vulkan ile tutarlılık için eklenmiştir.
    UINT subpass_index;

} fe_mt_pipeline_config_t;

// --- FE Metal Pipeline Yapısı (Gizli Tutaç) ---
// Bu, dahili Metal Pipeline State ve ilişkili nesneleri kapsüller.
typedef struct fe_mt_pipeline {
    __strong id<MTLRenderPipelineState> pso; // Render Pipeline State Object
    __strong id<MTLLibrary> library;         // Shader kütüphanesine referans (sahip değiliz)
    __strong MTLVertexDescriptor* vertex_descriptor; // Vertex düzeni tanımlayıcısı

    // Pipeline oluşturmak için kullanılan config'in bir kopyası tutulabilir
    // veya sadece gerekli runtime bilgiler tutulabilir.
} fe_mt_pipeline_t;

// --- Fonksiyonlar ---

/**
 * @brief Yeni bir Metal Render Pipeline State Object (PSO) oluşturur.
 * Verilen yapılandırma bilgilerine göre bir MTLRenderPipelineState nesnesi yaratır.
 *
 * @param device MTLDevice arayüzü.
 * @param library Shader fonksiyonlarını içeren MTLLibrary.
 * @param config PSO oluşturma yapılandırması.
 * @return fe_mt_pipeline_t* Oluşturulan PSO nesnesine işaretçi, başarısız olursa NULL.
 */
fe_mt_pipeline_t* fe_mt_pipeline_create(id<MTLDevice> device, id<MTLLibrary> library, const fe_mt_pipeline_config_t* config);

/**
 * @brief Bir Metal Render Pipeline State Object (PSO) ve ilişkili kaynaklarını yok eder.
 *
 * @param pipeline Yok edilecek pipeline nesnesi.
 */
void fe_mt_pipeline_destroy(fe_mt_pipeline_t* pipeline);

// Yardımcı fonksiyonlar (varsayılan yapılandırmalar için)
/**
 * @brief Standart bir renderleyici PSO yapılandırması için varsayılan değerleri doldurur.
 * Bu, tipik 3D renderleme için iyi bir başlangıç noktası sağlar.
 *
 * @param config Doldurulacak yapılandırma yapısı.
 * @param back_buffer_format Swap chain back buffer formatı (RT formatı için).
 * @param depth_buffer_format Derinlik arabelleği formatı (DSV formatı için).
 * @param sample_count MSAA örnek sayısı.
 */
void fe_mt_pipeline_default_config(fe_mt_pipeline_config_t* config,
                                   MTLPixelFormat back_buffer_format,
                                   MTLPixelFormat depth_buffer_format,
                                   NSUInteger sample_count);

#endif // FE_MT_PIPELINE_H
