#ifndef FE_VK_PIPELINE_H
#define FE_VK_PIPELINE_H

#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stdint.h>

#include "graphics/renderer/fe_renderer.h" // fe_renderer_error_t için
#include "graphics/vulkan/fe_vk_device.h"   // fe_vk_device_t için
#include "graphics/render_pass/fe_render_pass.h" // fe_render_pass_t için

// --- Vulkan Pipeline Hata Kodları ---
typedef enum fe_vk_pipeline_error {
    FE_VK_PIPELINE_SUCCESS = 0,
    FE_VK_PIPELINE_CREATION_FAILED,
    FE_VK_PIPELINE_SHADER_MODULE_CREATION_FAILED,
    FE_VK_PIPELINE_LAYOUT_CREATION_FAILED,
    FE_VK_PIPELINE_RENDER_PASS_MISMATCH,
    FE_VK_PIPELINE_INVALID_CONFIG,
    FE_VK_PIPELINE_OUT_OF_MEMORY,
    FE_VK_PIPELINE_UNKNOWN_ERROR
} fe_vk_pipeline_error_t;

// --- Vertex Input Binding (Vertex Giriş Bağlama) Tanımı ---
// Vertex veri arabelleğinin nasıl okunacağını tanımlar (örneğin, aralıklı mı, bitişik mi).
typedef struct fe_vk_vertex_input_binding_description {
    uint32_t binding;      // Bağlama noktası
    uint32_t stride;       // Bir sonraki vertex arasındaki byte mesafesi
    VkVertexInputRate input_rate; // Vertex başına mı yoksa instance başına mı ilerlesin
} fe_vk_vertex_input_binding_description_t;

// --- Vertex Input Attribute (Vertex Giriş Niteliği) Tanımı ---
// Shader'daki bir giriş niteliğinin (örneğin, pozisyon, renk) veri formatını ve konumunu tanımlar.
typedef struct fe_vk_vertex_input_attribute_description {
    uint32_t location;     // Shader'daki 'location' niteliği
    uint32_t binding;      // Bağlandığı binding (fe_vk_vertex_input_binding_description)
    VkFormat format;       // Veri formatı (örneğin, VK_FORMAT_R32G32B32_SFLOAT)
    uint32_t offset;       // Binding içindeki ofset (byte cinsinden)
} fe_vk_vertex_input_attribute_description_t;

// --- Shader Modülü Bilgisi ---
// SPV dosyasının yolu ve shader aşaması (vertex, fragment vb.).
typedef struct fe_vk_shader_module_info {
    const char* file_path;  // Shader SPV dosyasının yolu
    VkShaderStageFlagBits stage; // Shader aşaması (VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT vb.)
    const char* entry_point; // Shader'daki giriş noktası fonksiyon adı (genellikle "main")
} fe_vk_shader_module_info_t;

// --- Pipeline Yapılandırması ---
// Bir Vulkan grafik pipeline'ı oluşturmak için gerekli tüm ayarları kapsar.
typedef struct fe_vk_pipeline_config {
    // Pipeline Layout
    VkPipelineLayout pipeline_layout; // Pipeline Layout'u harici olarak oluşturulur ve sağlanır
                                      // (descriptor set layout'larını ve push constant aralıklarını tanımlar)

    // Render Pass
    fe_render_pass_t* render_pass; // Bu pipeline'ın uyumlu olduğu render pass

    // Shader Aşamaları
    uint32_t shader_stage_count;
    fe_vk_shader_module_info_t* shader_stages; // Shader modülleri ve aşama bilgileri

    // Vertex Giriş Tanımları
    uint32_t binding_description_count;
    fe_vk_vertex_input_binding_description_t* binding_descriptions;
    uint32_t attribute_description_count;
    fe_vk_vertex_input_attribute_description_t* attribute_descriptions;

    // Giriş Birleştirici (Input Assembly)
    VkPrimitiveTopology topology;    // Çizilecek primitif türü (örneğin, üçgen listesi)
    bool primitive_restart_enable;   // Primitif yeniden başlatma etkinleştirilsin mi?

    // Viewport ve Scissor
    VkViewport viewport;             // Görünüm alanı tanımı
    VkRect2D scissor;                // Kırpma dikdörtgeni tanımı

    // Rasterization
    VkPolygonMode polygon_mode;      // Çokgen doldurma modu (örneğin, doldurma, tel kafes)
    VkCullModeFlags cull_mode;       // Yüzey ayıklama modu (arka, ön, hiçbiri)
    VkFrontFace front_face;          // Ön yüzeyin yönü (saat yönü, saat yönünün tersi)
    float line_width;                // Çizgi genişliği (eğer çizgi çiziliyorsa)
    bool depth_bias_enable;          // Derinlik ofseti etkinleştirilsin mi?

    // Multisampling (Çoklu Örnekleme)
    VkSampleCountFlagBits rasterization_samples; // Örnek sayısı (MSAA için)
    bool sample_shading_enable;      // Örnekleme gölgelendirmesi etkinleştirilsin mi?
    float min_sample_shading;        // Minimum örnekleme gölgelendirme oranı

    // Derinlik ve Stencil Testi
    bool depth_test_enable;          // Derinlik testi etkinleştirilsin mi?
    bool depth_write_enable;         // Derinlik arabelleğine yazılsın mı?
    VkCompareOp depth_compare_op;    // Derinlik karşılaştırma operatörü
    bool stencil_test_enable;        // Stencil testi etkinleştirilsin mi?
    // Stencil operasyonları (front, back) burada detaylandırılabilir

    // Renk Karıştırma (Color Blending)
    bool blend_enable;               // Renk karıştırma etkinleştirilsin mi?
    VkBlendFactor src_color_blend_factor; // Kaynak renk karıştırma faktörü
    VkBlendFactor dst_color_blend_factor; // Hedef renk karıştırma faktörü
    VkBlendOp color_blend_op;       // Renk karıştırma operatörü
    VkBlendFactor src_alpha_blend_factor; // Kaynak alfa karıştırma faktörü
    VkBlendFactor dst_alpha_blend_factor; // Hedef alfa karıştırma faktörü
    VkBlendOp alpha_blend_op;       // Alfa karıştırma operatörü
    VkColorComponentFlags color_write_mask; // Hangi renk bileşenleri yazılsın

    // Dinamik Durumlar (Opsiyonel)
    // Bu, bazı pipeline durumlarının çizim zamanında değiştirilmesine izin verir.
    // Varsayılan olarak tüm durumlar sabittir.
    uint32_t dynamic_state_count;
    VkDynamicState* dynamic_states;  // VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR vb.
    // Dinamik durumlar etkinleştirilirse, viewport ve scissor bilgileri VkPipelineViewportStateCreateInfo'ya NULL olarak geçer.

    uint32_t subpass_index; // Bu pipeline'ın kullanılacağı render pass alt geçişinin indeksi (genellikle 0)
} fe_vk_pipeline_config_t;

// --- FE Vulkan Pipeline Yapısı (Gizli Tutaç) ---
// Bu, dahili Vulkan pipeline nesnelerini kapsüller.
typedef struct fe_vk_pipeline {
    VkPipeline graphics_pipeline;
    VkPipelineLayout pipeline_layout; // Pipeline Layout (fe_vk_pipeline'ın kendisi oluşturmaz, harici sağlanır)
    VkShaderModule vertex_shader_module;
    VkShaderModule fragment_shader_module;
    // Diğer shader modülleri (geometry, compute, tessellation) eklenebilir.
} fe_vk_pipeline_t;

// --- Fonksiyonlar ---

/**
 * @brief Yeni bir Vulkan grafik pipeline'ı oluşturur.
 * Verilen yapılandırma bilgilerine göre bir VkPipeline nesnesi yaratır.
 *
 * @param logical_device Mantıksal cihaz (fe_vk_device_t'den alınır).
 * @param config Pipeline oluşturma yapılandırması.
 * @return fe_vk_pipeline_t* Oluşturulan pipeline nesnesine işaretçi, başarısız olursa NULL.
 */
fe_vk_pipeline_t* fe_vk_pipeline_create(VkDevice logical_device, const fe_vk_pipeline_config_t* config);

/**
 * @brief Bir Vulkan grafik pipeline'ı ve ilişkili kaynaklarını yok eder.
 *
 * @param logical_device Mantıksal cihaz.
 * @param pipeline Yok edilecek pipeline nesnesi.
 */
void fe_vk_pipeline_destroy(VkDevice logical_device, fe_vk_pipeline_t* pipeline);

/**
 * @brief Bir shader SPV dosyasından bir VkShaderModule oluşturur.
 * Bu fonksiyon dahili olarak kullanılır ancak harici testler için de açılabilir.
 *
 * @param logical_device Mantıksal cihaz.
 * @param code_path Shader SPV dosyasının yolu.
 * @return VkShaderModule Oluşturulan shader modülü, başarısız olursa VK_NULL_HANDLE.
 */
VkShaderModule fe_vk_pipeline_create_shader_module(VkDevice logical_device, const char* code_path);

/**
 * @brief Bir VkShaderModule'ü yok eder.
 * @param logical_device Mantıksal cihaz.
 * @param shader_module Yok edilecek shader modülü.
 */
void fe_vk_pipeline_destroy_shader_module(VkDevice logical_device, VkShaderModule shader_module);

// Yardımcı fonksiyonlar (varsayılan yapılandırmalar için)
/**
 * @brief Standart bir renderleyici boru hattı yapılandırması için varsayılan değerleri doldurur.
 * Bu, tipik 3D renderleme için iyi bir başlangıç noktası sağlar.
 *
 * @param config Doldurulacak yapılandırma yapısı.
 * @param width Viewport genişliği.
 * @param height Viewport yüksekliği.
 */
void fe_vk_pipeline_default_config(fe_vk_pipeline_config_t* config, uint32_t width, uint32_t height);

#endif // FE_VK_PIPELINE_H
