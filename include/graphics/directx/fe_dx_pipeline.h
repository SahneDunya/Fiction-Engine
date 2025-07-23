#ifndef FE_DX_PIPELINE_H
#define FE_DX_PIPELINE_H

#include <d3d12.h>
#include <dxgi1_6.h> // DXGI 1.6 için
#include <wrl.h>     // Microsoft::WRL::ComPtr için

#include <stdbool.h>
#include <stdint.h>

#include "graphics/renderer/fe_renderer.h" // fe_renderer_error_t için
#include "graphics/directx/fe_dx_device.h"   // fe_dx_device_t için

using namespace Microsoft::WRL;

// --- DirectX Pipeline Hata Kodları ---
typedef enum fe_dx_pipeline_error {
    FE_DX_PIPELINE_SUCCESS = 0,
    FE_DX_PIPELINE_CREATION_FAILED,
    FE_DX_PIPELINE_SHADER_COMPILATION_FAILED, // DXIL/Bytecode yüklemede hata
    FE_DX_PIPELINE_ROOT_SIGNATURE_CREATION_FAILED,
    FE_DX_PIPELINE_INVALID_CONFIG,
    FE_DX_PIPELINE_OUT_OF_MEMORY,
    FE_DX_PIPELINE_UNKNOWN_ERROR
} fe_dx_pipeline_error_t;

// --- Shader Bytecode Bilgisi ---
// SPV/DXIL dosyasının yolu veya doğrudan bellekten bytecode
typedef struct fe_dx_shader_bytecode_info {
    const char* file_path;  // Shader DXIL/Bytecode dosyasının yolu (.cso)
    // Ya da doğrudan bytecode belleği ve boyutu
    const void* bytecode_ptr;
    SIZE_T bytecode_size;
} fe_dx_shader_bytecode_info_t;

// --- Vertex Input Layout Elemanı Tanımı ---
// Vertex verilerinin hafızada nasıl düzenlendiğini tanımlar (D3D12_INPUT_ELEMENT_DESC)
typedef struct fe_dx_input_element_desc {
    const char* semantic_name; // Shader'daki semantic adı (örneğin "POSITION", "COLOR")
    UINT semantic_index;       // Semantic index'i (örneğin "TEXCOORD0", "TEXCOORD1")
    DXGI_FORMAT format;        // Veri formatı (örneğin DXGI_FORMAT_R32G32B32_FLOAT)
    UINT input_slot;           // Vertex arabelleği slotu
    UINT aligned_byte_offset;  // Slot içindeki ofset (D3D12_APPEND_ALIGNED_ELEMENT veya byte cinsinden)
    D3D12_INPUT_CLASSIFICATION input_slot_class; // Vertex başına mı yoksa instance başına mı
    UINT instance_data_step_rate; // Instance başınaysa kaç instance'da bir ilerlesin
} fe_dx_input_element_desc_t;


// --- Pipeline Yapılandırması ---
// Bir Direct3D 12 grafik PSO'su oluşturmak için gerekli tüm ayarları kapsar.
// Bu yapılandırma, D3D12_GRAPHICS_PIPELINE_STATE_DESC yapısını doldurmak için kullanılır.
typedef struct fe_dx_pipeline_config {
    // Root Signature
    // Root Signature, GPU'nun shader'lara hangi kaynaklara (CBV, SRV, UAV, Sampler) erişebileceğini
    // ve push constant'lara benzer şekilde doğrudan verileri nasıl alacağını tanımlar.
    // Dışarıdan oluşturulmalı ve sağlanmalıdır.
    ID3D12RootSignature* root_signature;

    // Shader Aşamaları
    fe_dx_shader_bytecode_info_t vs_bytecode; // Vertex Shader Bytecode
    fe_dx_shader_bytecode_info_t ps_bytecode; // Pixel Shader Bytecode
    // Diğer shader aşamaları (GS, HS, DS) da eklenebilir.

    // Input Layout
    UINT input_element_count;
    fe_dx_input_element_desc_t* input_elements; // Vertex input düzeni

    // Input Assembler
    D3D12_PRIMITIVE_TOPOLOGY_TYPE primitive_topology_type; // Çizilecek primitiflerin topolojisi (örneğin, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)

    // Rasterizer State
    D3D12_FILL_MODE fill_mode;      // Çokgen doldurma modu (örneğin, D3D12_FILL_MODE_SOLID, D3D12_FILL_MODE_WIREFRAME)
    D3D12_CULL_MODE cull_mode;      // Yüzey ayıklama modu (arka, ön, hiçbiri)
    BOOL front_counter_clockwise;   // Ön yüzeyin yönü (saat yönünün tersi mi?)
    INT depth_bias;                 // Derinlik ofseti
    FLOAT depth_bias_clamp;         // Derinlik ofseti kenetleme
    FLOAT slope_scaled_depth_bias;  // Eğim ölçekli derinlik ofseti
    BOOL depth_clip_enable;         // Derinlik kırpma etkinleştirilsin mi?
    BOOL multisample_enable;        // Çoklu örnekleme etkinleştirilsin mi? (MSAA için)
    BOOL antialiased_line_enable;   // Kenar yumuşatma etkinleştirilsin mi?
    FLOAT forced_sample_count;      // Belirli bir örnek sayısını zorla (MSAA için)
    D3D12_CONSERVATIVE_RASTERIZATION_MODE conservative_raster; // Konservatif rasterization modu

    // Depth/Stencil State
    BOOL depth_enable;               // Derinlik testi etkinleştirilsin mi?
    D3D12_DEPTH_WRITE_MASK depth_write_mask; // Derinlik arabelleğine yazma maskesi
    D3D12_COMPARISON_FUNC depth_func; // Derinlik karşılaştırma fonksiyonu
    BOOL stencil_enable;             // Stencil testi etkinleştirilsin mi?
    UINT8 stencil_read_mask;         // Stencil okuma maskesi
    UINT8 stencil_write_mask;        // Stencil yazma maskesi
    D3D12_DEPTH_STENCILOP_DESC front_face_stencil; // Ön yüz stencil operasyonları
    D3D12_DEPTH_STENCILOP_DESC back_face_stencil;  // Arka yüz stencil operasyonları

    // Blend State (Renk Karıştırma)
    BOOL alpha_to_coverage_enable;   // Alfa-to-coverage etkinleştirilsin mi?
    BOOL independent_blend_enable;   // Bağımsız renk karıştırma etkinleştirilsin mi? (birden fazla render hedefi için)
    D3D12_RENDER_TARGET_BLEND_DESC render_target_blend[8]; // Her render hedefi için karıştırma ayarları (maks 8)

    // Sample Desc (MSAA için)
    UINT sample_count;               // Örnek sayısı (MSAA için)
    UINT sample_quality;             // Örnek kalitesi (MSAA için)

    // Render Target Formatları ve Derinlik Stencil Formatı
    UINT rtv_count;                  // Render hedefi sayısı
    DXGI_FORMAT rtv_formats[8];      // Render hedefi formatları (maks 8)
    DXGI_FORMAT dsv_format;          // Derinlik/Stencil formatı

    // Pipeline'ın kullanıldığı alt geçiş indeksi (Vulkan'daki gibi doğrudan bir kavram yok, ancak mantıksal olarak düşünülebilir)
    // Bu, D3D12'de bir anlam ifade etmez, sadece Vulkan ile tutarlılık için eklenmiştir.
    UINT subpass_index;

} fe_dx_pipeline_config_t;

// --- FE DirectX Pipeline Yapısı (Gizli Tutaç) ---
// Bu, dahili DirectX PSO ve Root Signature nesnelerini kapsüller.
typedef struct fe_dx_pipeline {
    ComPtr<ID3D12PipelineState> pso;
    ComPtr<ID3D12RootSignature> root_signature; // Pipeline tarafından referans alınır

    // Shader bytecode'ları burada saklanmaz, sadece PSO oluşturulurken kullanılır.
    // Ancak hata ayıklama veya yeniden oluşturma için referanslar tutulabilir.
} fe_dx_pipeline_t;

// --- Fonksiyonlar ---

/**
 * @brief Bir Direct3D 12 Root Signature oluşturur.
 * Root Signature, shader'ların hangi kaynaklara (descriptor'lar) erişebileceğini tanımlar.
 * Genellikle pipeline'dan ayrı olarak oluşturulur.
 *
 * @param device ID3D12Device arayüzü.
 * @param root_signature_desc Root Signature tanımı (D3D12_ROOT_SIGNATURE_DESC).
 * @return ComPtr<ID3D12RootSignature> Oluşturulan Root Signature nesnesi, başarısız olursa nullptr.
 */
ComPtr<ID3D12RootSignature> fe_dx_pipeline_create_root_signature(ID3D12Device* device, const D3D12_ROOT_SIGNATURE_DESC* root_signature_desc);


/**
 * @brief Yeni bir Direct3D 12 grafik Pipeline State Object (PSO) oluşturur.
 * Verilen yapılandırma bilgilerine göre bir ID3D12PipelineState nesnesi yaratır.
 *
 * @param device ID3D12Device arayüzü.
 * @param config PSO oluşturma yapılandırması.
 * @return fe_dx_pipeline_t* Oluşturulan PSO nesnesine işaretçi, başarısız olursa NULL.
 */
fe_dx_pipeline_t* fe_dx_pipeline_create(ID3D12Device* device, const fe_dx_pipeline_config_t* config);

/**
 * @brief Bir Direct3D 12 grafik Pipeline State Object (PSO) ve ilişkili kaynaklarını yok eder.
 *
 * @param pipeline Yok edilecek pipeline nesnesi.
 */
void fe_dx_pipeline_destroy(fe_dx_pipeline_t* pipeline);

// Yardımcı fonksiyonlar (varsayılan yapılandırmalar için)
/**
 * @brief Standart bir renderleyici PSO yapılandırması için varsayılan değerleri doldurur.
 * Bu, tipik 3D renderleme için iyi bir başlangıç noktası sağlar.
 *
 * @param config Doldurulacak yapılandırma yapısı.
 * @param width Viewport genişliği.
 * @param height Viewport yüksekliği.
 * @param back_buffer_format Swap chain back buffer formatı (RT formatı için).
 * @param depth_buffer_format Derinlik arabelleği formatı (DSV formatı için).
 */
void fe_dx_pipeline_default_config(fe_dx_pipeline_config_t* config,
                                   uint32_t width, uint32_t height,
                                   DXGI_FORMAT back_buffer_format,
                                   DXGI_FORMAT depth_buffer_format);

#endif // FE_DX_PIPELINE_H
