#include "graphics/shader/fe_shader_compiler.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h"
#include "core/utils/fe_file_system.h" // Dosya okuma için

#include <stdio.h>
#include <string.h>
#include <stdlib.h> // for free

// --- Harici Derleyici Entegrasyonları için Başlıklar (Örnek) ---
// Gerçek bir projede bu kütüphaneler doğru şekilde kurulmalı ve bağlanmalıdır.

 #ifdef FE_PLATFORM_WINDOWS
 #include <dxcapi.h> // Microsoft'un DXC için başlığı
 #endif

 #include <shaderc/shaderc.h> // Google'ın Shaderc için başlığı (GLSL/HLSL -> SPIR-V)
 #include <spirv-reflect/spirv_reflect.h> // SPIR-V Reflection için başlık

// --- Metal Shading Language (MSL) için Objective-C Başlıkları ---
// Eğer .m/.mm dosyası ise import edilebilir, aksi takdirde PIMPL (Pointer to IMPL) veya Objective-C bridge kullanılır.
// Bu .c dosyası içinde, Metal derlemesi için Objective-C runtime'a bir köprü kurulmalıdır.
// Şimdilik Metal derleme kısmını bir placeholder olarak bırakıyoruz, genellikle bunu Objective-C++ dosyasında yapmak daha kolaydır.


// Global shader derleyici durumu (singleton)
static bool g_is_initialized = false;
// Harici derleyici kütüphanelerinin tutaçları burada saklanabilir (örn. dxc library, shaderc compiler_t)

// --- Dahili Yardımcı Fonksiyonlar ---

// Shader aşamasını string'e çevirir
const char* fe_shader_stage_to_string(fe_shader_stage_t stage) {
    switch (stage) {
        case FE_SHADER_STAGE_VERTEX: return "Vertex";
        case FE_SHADER_STAGE_FRAGMENT: return "Fragment";
        case FE_SHADER_STAGE_COMPUTE: return "Compute";
        case FE_SHADER_STAGE_GEOMETRY: return "Geometry";
        case FE_SHADER_STAGE_TESS_CONTROL: return "Tessellation Control";
        case FE_SHADER_STAGE_TESS_EVALUATION: return "Tessellation Evaluation";
        case FE_SHADER_STAGE_RAYGEN: return "Ray Generation";
        case FE_SHADER_STAGE_INTERSECTION: return "Intersection";
        case FE_SHADER_STAGE_ANY_HIT: return "Any Hit";
        case FE_SHADER_STAGE_CLOSEST_HIT: return "Closest Hit";
        case FE_SHADER_STAGE_MISS: return "Miss";
        case FE_SHADER_STAGE_CALLABLE: return "Callable";
        case FE_SHADER_STAGE_MESH: return "Mesh";
        case FE_SHADER_STAGE_TASK: return "Task";
        default: return "Unknown";
    }
}

// Kaynak dilini string'e çevirir
const char* fe_shader_language_to_string(fe_shader_source_language_t lang) {
    switch (lang) {
        case FE_SHADER_LANG_HLSL: return "HLSL";
        case FE_SHADER_LANG_GLSL: return "GLSL";
        case FE_SHADER_LANG_MSL: return "MSL";
        default: return "Unknown";
    }
}

// Hedef API'yi string'e çevirir
const char* fe_shader_target_api_to_string(fe_shader_target_api_t target) {
    switch (target) {
        case FE_SHADER_TARGET_VULKAN: return "Vulkan (SPIR-V)";
        case FE_SHADER_TARGET_DIRECTX11: return "DirectX 11 (DXBC)";
        case FE_SHADER_TARGET_DIRECTX12: return "DirectX 12 (DXIL)";
        case FE_SHADER_TARGET_METAL: return "Metal (MSL/Metallib)";
        default: return "Unknown";
    }
}

// --- Derleme ve Yansıma için Placeholder Fonksiyonlar ---
// Gerçek uygulamada buraya Shaderc, DXC, SPIRV-Reflect entegrasyonu gelir.

// Shaderc (GLSL/HLSL -> SPIR-V) için derleme örneği (yer tutucu)
static fe_shader_compiler_error_t fe_shader_compiler_compile_shaderc(
    const char* source_code, size_t source_size,
    fe_shader_source_language_t source_language, const char* entry_point,
    fe_shader_stage_t shader_stage, bool debug_info, bool optimize,
    fe_compiled_shader_data_t* output_compiled_data,
    fe_shader_reflection_data_t* output_reflection_data)
{
    FE_LOG_DEBUG("Placeholder: Compiling with Shaderc (GLSL/HLSL to SPIR-V)...");
    // Gerçek entegrasyon:
     shaderc_compiler_t compiler = shaderc_compiler_initialize();
     shaderc_compile_options_t options = shaderc_compile_options_initialize();
    // // Options: optimization_level, target_env, etc.
     shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler, source_code, source_size,
         fe_shader_stage_to_shaderc_kind(shader_stage), "shader_file.glsl", entry_point, options);
    //
     if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) {
         FE_LOG_ERROR("Shaderc compilation failed: %s", shaderc_result_get_error_message(result));
         shaderc_result_release(result);
         shaderc_compiler_release(compiler);
         return FE_SHADER_COMPILER_COMPILATION_FAILED;
     }
    //
     output_compiled_data->size = shaderc_result_get_length(result);
     output_compiled_data->data = fe_malloc(output_compiled_data->size, FE_MEM_TYPE_GRAPHICS_SHADER, __FILE__, __LINE__);
     memcpy(output_compiled_data->data, shaderc_result_get_bytes(result), output_compiled_data->size);
    //
    // // SPIR-V Reflection (using spirv-reflect)
     if (output_reflection_data) {
    //    // Implement SPIR-V reflection using spirv_reflect library here
    //    // This would parse the SPIR-V binary and populate output_reflection_data
     }
    
     shaderc_result_release(result);
     shaderc_compiler_release(compiler);

    // Geçici olarak mock veri döndürelim
    output_compiled_data->size = 100;
    output_compiled_data->data = fe_malloc(output_compiled_data->size, FE_MEM_TYPE_GRAPHICS_SHADER, __FILE__, __LINE__);
    memset(output_compiled_data->data, 0xCD, output_compiled_data->size); // Mock data

    if (output_reflection_data) {
        // Mock reflection data
        output_reflection_data->uniform_buffer_count = 1;
        output_reflection_data->uniform_buffers = fe_malloc(sizeof(fe_shader_resource_binding_t), FE_MEM_TYPE_GRAPHICS_SHADER, __FILE__, __LINE__);
        fe_string_init(&output_reflection_data->uniform_buffers[0].name, "CameraBuffer");
        output_reflection_data->uniform_buffers[0].set = 0;
        output_reflection_data->uniform_buffers[0].binding = 0;
        output_reflection_data->uniform_buffers[0].count = 1;
        // Diğerlerini de doldurun
    }

    FE_LOG_DEBUG("Shaderc placeholder compilation successful.");
    return FE_SHADER_COMPILER_SUCCESS;
}

// DXC (HLSL -> DXIL) için derleme örneği (yer tutucu)
static fe_shader_compiler_error_t fe_shader_compiler_compile_dxc(
    const char* source_code, size_t source_size,
    const char* entry_point, fe_shader_stage_t shader_stage,
    fe_shader_target_api_t target_api, bool debug_info, bool optimize,
    fe_compiled_shader_data_t* output_compiled_data,
    fe_shader_reflection_data_t* output_reflection_data)
{
    FE_LOG_DEBUG("Placeholder: Compiling with DXC (HLSL to DXIL/DXBC)...");
    // Gerçek entegrasyon (Windows'a özgü):
    // #ifdef FE_PLATFORM_WINDOWS
    // IDxcCompiler3* pCompiler;
    // DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));
    // // ... create source blob, compile, handle errors, get result ...
    // // For reflection, use IDxcContainerReflection or older D3DCompile API
    // #endif

    // Geçici olarak mock veri döndürelim
    output_compiled_data->size = 128;
    output_compiled_data->data = fe_malloc(output_compiled_data->size, FE_MEM_TYPE_GRAPHICS_SHADER, __FILE__, __LINE__);
    memset(output_compiled_data->data, 0xDE, output_compiled_data->size); // Mock data

    if (output_reflection_data) {
        // Mock reflection data
        output_reflection_data->texture_count = 1;
        output_reflection_data->textures = fe_malloc(sizeof(fe_shader_resource_binding_t), FE_MEM_TYPE_GRAPHICS_SHADER, __FILE__, __LINE__);
        fe_string_init(&output_reflection_data->textures[0].name, "g_DiffuseTexture");
        output_reflection_data->textures[0].set = 0; // DirectX'te set kavramı yoktur, binding sadece slot no.
        output_reflection_data->textures[0].binding = 0;
        output_reflection_data->textures[0].count = 1;
    }
    FE_LOG_DEBUG("DXC placeholder compilation successful.");
    return FE_SHADER_COMPILER_SUCCESS;
}

// MSL (Metal Shading Language) için derleme örneği (yer tutucu)
// Metal'de shader'lar genellikle çalışma zamanında `MTLLibrary` nesnesi oluşturularak veya
// önceden derlenmiş `.metallib` dosyaları yüklenerek derlenir.
// C kodundan doğrudan Objective-C Metal API'sine erişmek için bir Objective-C++ ara katmanı gereklidir.
static fe_shader_compiler_error_t fe_shader_compiler_compile_msl(
    const char* source_code, size_t source_size,
    const char* entry_point, fe_shader_stage_t shader_stage,
    bool debug_info, bool optimize,
    fe_compiled_shader_data_t* output_compiled_data,
    fe_shader_reflection_data_t* output_reflection_data)
{
    FE_LOG_DEBUG("Placeholder: Compiling MSL (Metal Shading Language)...");
    // Gerçek entegrasyon (Objective-C++ gerektirir):
     #ifdef FE_PLATFORM_APPLE
     #import <Metal/Metal.h>
     id<MTLDevice> device = MTLCreateSystemDefaultDevice();
     NSError* error = nil;
    // // Compile from source
      id<MTLLibrary> library = [device newLibraryWithSource:[NSString stringWithUTF8String:source_code]
                                                   options:nil
                                                     error:&error];
     if (!library) {
        FE_LOG_ERROR("MSL compilation failed: %s", error.localizedDescription.UTF8String);
        return FE_SHADER_COMPILER_COMPILATION_FAILED;
     }
    // // Metal reflection is usually done via MTLFunction objects.
    // // Or by parsing the Metal Shading Language source itself (less common)
     #endif

    // Geçici olarak mock veri döndürelim (MSL için genellikle doğrudan ikili çıktıya ihtiyacımız olmaz,
    // MTLLibrary nesnesi tutulur. Ama burada bir ikili çıktı gibi davranıyoruz.)
    output_compiled_data->size = 64;
    output_compiled_data->data = fe_malloc(output_compiled_data->size, FE_MEM_TYPE_GRAPHICS_SHADER, __FILE__, __LINE__);
    memset(output_compiled_data->data, 0xAB, output_compiled_data->size); // Mock data

    if (output_reflection_data) {
        // Mock reflection data (Metal'de reflection biraz farklıdır)
        output_reflection_data->sampler_count = 1;
        output_reflection_data->samplers = fe_malloc(sizeof(fe_shader_resource_binding_t), FE_MEM_TYPE_GRAPHICS_SHADER, __FILE__, __LINE__);
        fe_string_init(&output_reflection_data->samplers[0].name, "mySampler");
        output_reflection_data->samplers[0].set = 0; // Metal'de set kavramı yok
        output_reflection_data->samplers[0].binding = 0; // Genellikle index
        output_reflection_data->samplers[0].count = 1;
    }
    FE_LOG_DEBUG("MSL placeholder compilation successful.");
    return FE_SHADER_COMPILER_SUCCESS;
}


// --- Ana Fonksiyonlar Uygulaması ---

fe_shader_compiler_error_t fe_shader_compiler_init() {
    if (g_is_initialized) {
        FE_LOG_WARN("Shader compiler already initialized.");
        return FE_SHADER_COMPILER_SUCCESS;
    }

    // Harici derleyici kütüphanelerini (Shaderc, DXC) başlatma burada yapılır.
    // Örn: shaderc_compiler_initialize(), DxcCreateInstance() çağrıları.
    // Bu, dynamic library loading ile de yapılabilir (LoadLibrary / dlopen).

    g_is_initialized = true;
    FE_LOG_INFO("Shader compiler initialized.");
    return FE_SHADER_COMPILER_SUCCESS;
}

void fe_shader_compiler_shutdown() {
    if (!g_is_initialized) {
        FE_LOG_WARN("Shader compiler not initialized.");
        return;
    }

    // Harici derleyici kütüphanelerini kapatma burada yapılır.
    // Örn: shaderc_compiler_release(), DXC nesnelerinin serbest bırakılması.

    g_is_initialized = false;
    FE_LOG_INFO("Shader compiler shutdown complete.");
}

fe_shader_compiler_error_t fe_shader_compiler_compile_shader(
    const char* file_path,
    fe_shader_source_language_t source_language,
    const char* entry_point,
    fe_shader_stage_t shader_stage,
    fe_shader_target_api_t target_api,
    bool debug_info,
    bool optimize,
    fe_compiled_shader_data_t* output_compiled_data,
    fe_shader_reflection_data_t* output_reflection_data)
{
    if (!g_is_initialized) {
        FE_LOG_ERROR("Shader compiler not initialized.");
        return FE_SHADER_COMPILER_NOT_INITIALIZED;
    }
    if (!file_path || !entry_point || !output_compiled_data) {
        FE_LOG_ERROR("Invalid arguments for shader compilation.");
        return FE_SHADER_COMPILER_INVALID_ARGUMENT;
    }

    FE_LOG_INFO("Compiling shader '%s' (%s - %s) for target %s...",
                file_path,
                fe_shader_language_to_string(source_language),
                fe_shader_stage_to_string(shader_stage),
                fe_shader_target_api_to_string(target_api));

    // Shader kaynak kodunu oku
    char* source_code = NULL;
    size_t source_size = 0;
    fe_fs_read_text_file(file_path, &source_code, &source_size); // fe_file_system.h'den varsayılan fonksiyon
    if (!source_code) {
        FE_LOG_ERROR("Failed to read shader file: %s", file_path);
        return FE_SHADER_COMPILER_FILE_READ_ERROR;
    }

    fe_shader_compiler_error_t result = FE_SHADER_COMPILER_UNKNOWN_ERROR;

    // Hedef API'ye göre derleyici çağrısı
    switch (target_api) {
        case FE_SHADER_TARGET_VULKAN:
            // Vulkan için genellikle GLSL veya HLSL'den SPIR-V'ye derleme yapılır (Shaderc kullanılır)
            result = fe_shader_compiler_compile_shaderc(
                source_code, source_size, source_language, entry_point, shader_stage,
                debug_info, optimize, output_compiled_data, output_reflection_data
            );
            break;
        case FE_SHADER_TARGET_DIRECTX11: // DXBC (eski)
        case FE_SHADER_TARGET_DIRECTX12: // DXIL (yeni)
            // DirectX için HLSL'den DXIL/DXBC'ye derleme yapılır (DXC kullanılır)
            result = fe_shader_compiler_compile_dxc(
                source_code, source_size, entry_point, shader_stage,
                target_api, debug_info, optimize, output_compiled_data, output_reflection_data
            );
            break;
        case FE_SHADER_TARGET_METAL:
            // Metal için MSL'den Metallib'e veya doğrudan çalışma zamanı derlemesi yapılır.
            // Bu genellikle Objective-C/C++ ara katmanı ile ele alınır.
            // Burada sadece MSL kaynak kodu olduğunu varsayıyoruz.
            result = fe_shader_compiler_compile_msl(
                source_code, source_size, entry_point, shader_stage,
                debug_info, optimize, output_compiled_data, output_reflection_data
            );
            break;
        default:
            FE_LOG_ERROR("Unsupported shader target API: %s", fe_shader_target_api_to_string(target_api));
            result = FE_SHADER_COMPILER_INVALID_ARGUMENT;
            break;
    }

    fe_free(source_code, FE_MEM_TYPE_GENERAL, __FILE__, __LINE__); // Okunan kaynak kodu serbest bırak

    if (result != FE_SHADER_COMPILER_SUCCESS) {
        FE_LOG_ERROR("Shader compilation failed for '%s'.", file_path);
    } else {
        FE_LOG_INFO("Shader '%s' compiled successfully for %s.", file_path, fe_shader_target_api_to_string(target_api));
    }

    return result;
}

void fe_shader_compiler_free_compiled_shader_data(fe_compiled_shader_data_t* data) {
    if (data && data->data) {
        fe_free(data->data, FE_MEM_TYPE_GRAPHICS_SHADER, __FILE__, __LINE__);
        data->data = NULL;
        data->size = 0;
    }
}

void fe_shader_compiler_free_reflection_data(fe_shader_reflection_data_t* reflection_data) {
    if (reflection_data) {
        if (reflection_data->uniform_buffers) {
            for (size_t i = 0; i < reflection_data->uniform_buffer_count; ++i) {
                fe_string_destroy(&reflection_data->uniform_buffers[i].name);
            }
            fe_free(reflection_data->uniform_buffers, FE_MEM_TYPE_GRAPHICS_SHADER, __FILE__, __LINE__);
            reflection_data->uniform_buffers = NULL;
            reflection_data->uniform_buffer_count = 0;
        }
        if (reflection_data->textures) {
            for (size_t i = 0; i < reflection_data->texture_count; ++i) {
                fe_string_destroy(&reflection_data->textures[i].name);
            }
            fe_free(reflection_data->textures, FE_MEM_TYPE_GRAPHICS_SHADER, __FILE__, __LINE__);
            reflection_data->textures = NULL;
            reflection_data->texture_count = 0;
        }
        if (reflection_data->samplers) {
            for (size_t i = 0; i < reflection_data->sampler_count; ++i) {
                fe_string_destroy(&reflection_data->samplers[i].name);
            }
            fe_free(reflection_data->samplers, FE_MEM_TYPE_GRAPHICS_SHADER, __FILE__, __LINE__);
            reflection_data->samplers = NULL;
            reflection_data->sampler_count = 0;
        }
        // Diğer kaynakları da temizle
    }
}
