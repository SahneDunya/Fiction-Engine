#ifndef FE_SHADER_COMPILER_H
#define FE_SHADER_COMPILER_H

#include "core/utils/fe_types.h" // fe_string için
#include "graphics/renderer/fe_renderer.h" // fe_renderer_backend_type_t için

// Shader Derleyici Hata Kodları
typedef enum fe_shader_compiler_error {
    FE_SHADER_COMPILER_SUCCESS = 0,
    FE_SHADER_COMPILER_NOT_INITIALIZED,
    FE_SHADER_COMPILER_INVALID_ARGUMENT,
    FE_SHADER_COMPILER_FILE_NOT_FOUND,
    FE_SHADER_COMPILER_READ_ERROR,
    FE_SHADER_COMPILER_COMPILATION_FAILED, // Derleme hatası
    FE_SHADER_COMPILER_REFLECTION_FAILED,   // Yansıma hatası
    FE_SHADER_COMPILER_OUT_OF_MEMORY,
    FE_SHADER_COMPILER_UNKNOWN_ERROR
} fe_shader_compiler_error_t;

// Desteklenen Shader Dilleri
typedef enum fe_shader_source_language {
    FE_SHADER_LANG_HLSL, // High-Level Shading Language (DirectX için)
    FE_SHADER_LANG_GLSL, // OpenGL Shading Language (Vulkan, OpenGL için)
    FE_SHADER_LANG_MSL   // Metal Shading Language (Metal için)
} fe_shader_source_language_t;

// Shader Aşamaları
typedef enum fe_shader_stage {
    FE_SHADER_STAGE_VERTEX,
    FE_SHADER_STAGE_FRAGMENT,
    FE_SHADER_STAGE_COMPUTE,
    FE_SHADER_STAGE_GEOMETRY,
    FE_SHADER_STAGE_TESS_CONTROL,
    FE_SHADER_STAGE_TESS_EVALUATION,
    FE_SHADER_STAGE_RAYGEN,
    FE_SHADER_STAGE_INTERSECTION,
    FE_SHADER_STAGE_ANY_HIT,
    FE_SHADER_STAGE_CLOSEST_HIT,
    FE_SHADER_STAGE_MISS,
    FE_SHADER_STAGE_CALLABLE,
    FE_SHADER_STAGE_MESH,
    FE_SHADER_STAGE_TASK,
    FE_SHADER_STAGE_COUNT
} fe_shader_stage_t;

// Shader Derleme Hedefleri
typedef enum fe_shader_target_api {
    FE_SHADER_TARGET_VULKAN,    // SPIR-V'ye derleme
    FE_SHADER_TARGET_DIRECTX11, // DXBC'ye derleme
    FE_SHADER_TARGET_DIRECTX12, // DXIL'e derleme
    FE_SHADER_TARGET_METAL,     // MSL kaynak kodu veya .metallib'e derleme
    FE_SHADER_TARGET_COUNT
} fe_shader_target_api_t;

// Derlenmiş Shader Verileri (Binary Blob)
// Farklı API'ler için farklı formatlarda olabilir.
typedef struct fe_compiled_shader_data {
    uint8_t* data;   // Derlenmiş shader baytları (SPIR-V, DXIL, DXBC vb.)
    size_t size;     // Verinin boyutu
    // Bu veri `fe_shader_compiler_compile_shader` tarafından tahsis edilir ve kullanıcı tarafından serbest bırakılmalıdır.
} fe_compiled_shader_data_t;

// Shader Reflection Verileri (örneğin, sabit arabellekler, dokular, örnekleyiciler)
// Shader'dan çıkarılan meta veriler.
typedef struct fe_shader_resource_binding {
    fe_string name;      // Kaynağın adı (örn. "g_ViewProjectionBuffer")
    uint32_t set;        // Kaynak set indeksi (Vulkan için)
    uint32_t binding;    // Bağlama noktası indeksi
    uint32_t count;      // Dizi boyutu (eğer dizi ise)
    // Diğer shader reflection bilgileri buraya eklenebilir (örn. boyutu, tipi)
} fe_shader_resource_binding_t;

typedef struct fe_shader_reflection_data {
    fe_shader_resource_binding_t* uniform_buffers;
    size_t uniform_buffer_count;
    fe_shader_resource_binding_t* textures;
    size_t texture_count;
    fe_shader_resource_binding_t* samplers;
    size_t sampler_count;
    // ... diğer kaynaklar (storage buffers, images, input attachments vb.)
} fe_shader_reflection_data_t;


// --- Fonksiyon Deklarasyonları ---

/**
 * @brief Shader derleyici sistemini başlatır.
 * Harici derleyici kütüphanelerini (Shaderc, DXC) yükler ve hazırlar.
 *
 * @return fe_shader_compiler_error_t Başarı durumunu döner.
 */
fe_shader_compiler_error_t fe_shader_compiler_init();

/**
 * @brief Shader derleyici sistemini kapatır ve tüm kaynakları serbest bırakır.
 * Yüklenen derleyici kütüphanelerini kaldırır.
 */
void fe_shader_compiler_shutdown();

/**
 * @brief Shader kaynak kodunu derler.
 * Verilen shader dosyasını okur, belirtilen hedef API'ye göre derler
 * ve derlenmiş ikili veriyi döndürür.
 *
 * @param file_path Shader kaynak dosyasının yolu.
 * @param source_language Kaynak kodun dili (HLSL, GLSL, MSL).
 * @param entry_point Shader'ın giriş noktası fonksiyonunun adı (örn. "main", "VSMain").
 * @param shader_stage Shader aşaması (Vertex, Fragment, Compute vb.).
 * @param target_api Derlemenin hedef alınacağı grafik API'si (Vulkan, DirectX12, Metal vb.).
 * @param debug_info Debug bilgileri eklensin mi?
 * @param optimize Optimizasyon yapılsın mı?
 * @param output_compiled_data Derlenmiş veriyi içerecek çıktı yapısı. Kullanıcı bu veriyi serbest bırakmalıdır.
 * @param output_reflection_data Shader reflection verilerini içerecek çıktı yapısı (isteğe bağlı, NULL olabilir).
 * @return fe_shader_compiler_error_t Başarı durumunu döner.
 */
fe_shader_compiler_error_t fe_shader_compiler_compile_shader(
    const char* file_path,
    fe_shader_source_language_t source_language,
    const char* entry_point,
    fe_shader_stage_t shader_stage,
    fe_shader_target_api_t target_api,
    bool debug_info,
    bool optimize,
    fe_compiled_shader_data_t* output_compiled_data,
    fe_shader_reflection_data_t* output_reflection_data
);

/**
 * @brief fe_compiled_shader_data_t tarafından tahsis edilen belleği serbest bırakır.
 *
 * @param data Serbest bırakılacak derlenmiş shader verisi.
 */
void fe_shader_compiler_free_compiled_shader_data(fe_compiled_shader_data_t* data);

/**
 * @brief fe_shader_reflection_data_t tarafından tahsis edilen belleği serbest bırakır.
 *
 * @param reflection_data Serbest bırakılacak yansıma verisi.
 */
void fe_shader_compiler_free_reflection_data(fe_shader_reflection_data_t* reflection_data);

// Yardımcı fonksiyon: Shader aşamasını string'e çevirir
const char* fe_shader_stage_to_string(fe_shader_stage_t stage);
// Yardımcı fonksiyon: Kaynak dilini string'e çevirir
const char* fe_shader_language_to_string(fe_shader_source_language_t lang);
// Yardımcı fonksiyon: Hedef API'yi string'e çevirir
const char* fe_shader_target_api_to_string(fe_shader_target_api_t target);


#endif // FE_SHADER_COMPILER_H
