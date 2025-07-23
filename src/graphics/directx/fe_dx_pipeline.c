#include "graphics/directx/fe_dx_pipeline.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h"

#include <string.h> // memset
#include <stdio.h>  // fopen, fread, fclose
#include <vector>   // Geçici bellek için

// COM hata kodlarını string'e çevirmek için basit bir makro/fonksiyon
#define HR_CHECK_NULL(hr, msg) \
    if (FAILED(hr)) { \
        FE_LOG_CRITICAL("%s HRESULT: 0x%lx", msg, (unsigned long)hr); \
        return NULL; \
    }

#define HR_CHECK_COMPTR(hr, msg) \
    if (FAILED(hr)) { \
        FE_LOG_CRITICAL("%s HRESULT: 0x%lx", msg, (unsigned long)hr); \
        return nullptr; \
    }

// --- Dahili Yardımcı Fonksiyonlar ---

// Bytecode dosyasını okur
static std::vector<char> fe_dx_pipeline_read_bytecode_file(const char* filename) {
    std::vector<char> bytecode;
    FILE* file = fopen(filename, "rb");
    if (!file) {
        FE_LOG_ERROR("Failed to open shader bytecode file: %s", filename);
        return bytecode;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    bytecode.resize(file_size);
    size_t bytes_read = fread(bytecode.data(), 1, file_size, file);
    if (bytes_read != file_size) {
        FE_LOG_ERROR("Failed to read entire shader bytecode file: %s", filename);
        bytecode.clear(); // Hata durumunda vektörü temizle
    }

    fclose(file);
    return bytecode;
}

// --- Ana Pipeline Fonksiyonları Uygulaması ---

// Root Signature oluşturma
ComPtr<ID3D12RootSignature> fe_dx_pipeline_create_root_signature(ID3D12Device* device, const D3D12_ROOT_SIGNATURE_DESC* root_signature_desc) {
    if (!device || !root_signature_desc) {
        FE_LOG_ERROR("fe_dx_pipeline_create_root_signature: device or root_signature_desc is NULL.");
        return nullptr;
    }

    ComPtr<ID3DBlob> signature_blob;
    ComPtr<ID3DBlob> error_blob;
    HRESULT hr = D3D12SerializeRootSignature(root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature_blob, &error_blob);
    if (FAILED(hr)) {
        if (error_blob) {
            FE_LOG_CRITICAL("Failed to serialize root signature: %s", (char*)error_blob->GetBufferPointer());
        } else {
            FE_LOG_CRITICAL("Failed to serialize root signature! HRESULT: 0x%lx", (unsigned long)hr);
        }
        return nullptr;
    }

    ComPtr<ID3D12RootSignature> root_signature;
    hr = device->CreateRootSignature(0, signature_blob->GetBufferPointer(), signature_blob->GetBufferSize(), IID_PPV_ARGS(&root_signature));
    HR_CHECK_COMPTR(hr, "Failed to create root signature!");

    FE_LOG_INFO("Direct3D 12 Root Signature created successfully.");
    return root_signature;
}


void fe_dx_pipeline_default_config(fe_dx_pipeline_config_t* config,
                                   uint32_t width, uint32_t height,
                                   DXGI_FORMAT back_buffer_format,
                                   DXGI_FORMAT depth_buffer_format) {
    memset(config, 0, sizeof(fe_dx_pipeline_config_t));

    // Input Assembler
    config->primitive_topology_type = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    // Rasterizer State
    config->fill_mode = D3D12_FILL_MODE_SOLID;
    config->cull_mode = D3D12_CULL_MODE_BACK;
    config->front_counter_clockwise = FALSE; // Direct3D'de varsayılan saat yönündedir (false ile)
    config->depth_bias = D3D12_DEFAULT_DEPTH_BIAS;
    config->depth_bias_clamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    config->slope_scaled_depth_bias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    config->depth_clip_enable = TRUE;
    config->multisample_enable = FALSE; // MSAA kapalı varsayılan
    config->antialiased_line_enable = FALSE;
    config->forced_sample_count = 0; // Eğer multisample_enable false ise 0 olmalı
    config->conservative_raster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // Depth/Stencil State
    config->depth_enable = TRUE;
    config->depth_write_mask = D3D12_DEPTH_WRITE_MASK_ALL;
    config->depth_func = D3D12_COMPARISON_FUNC_LESS;
    config->stencil_enable = FALSE;
    config->stencil_read_mask = D3D12_DEFAULT_STENCIL_READ_MASK;
    config->stencil_write_mask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    const D3D12_DEPTH_STENCILOP_DESC default_stencil_op = {
        D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS
    };
    config->front_face_stencil = default_stencil_op;
    config->back_face_stencil = default_stencil_op;

    // Blend State (Tek bir Render Target için varsayılan)
    config->alpha_to_coverage_enable = FALSE;
    config->independent_blend_enable = FALSE;

    // Varsayılan olarak karıştırma kapalı, tüm renk bileşenleri yazılabilir
    D3D12_RENDER_TARGET_BLEND_DESC default_rt_blend = {
        FALSE,FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL,
    };
    config->render_target_blend[0] = default_rt_blend; // İlk RT için

    // Sample Desc (MSAA için)
    config->sample_count = 1;
    config->sample_quality = 0;

    // Render Target Formatları ve Derinlik Stencil Formatı
    config->rtv_count = 1;
    config->rtv_formats[0] = back_buffer_format;
    config->dsv_format = depth_buffer_format;

    config->subpass_index = 0; // D3D12'de anlamı yok, uyumluluk için.
}

fe_dx_pipeline_t* fe_dx_pipeline_create(ID3D12Device* device, const fe_dx_pipeline_config_t* config) {
    if (!device || !config || !config->root_signature) {
        FE_LOG_ERROR("fe_dx_pipeline_create: device, config or root_signature is NULL.");
        return NULL;
    }

    fe_dx_pipeline_t* pipeline = fe_malloc(sizeof(fe_dx_pipeline_t), FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    if (!pipeline) {
        FE_LOG_CRITICAL("Failed to allocate memory for fe_dx_pipeline_t.");
        return NULL;
    }
    // ComPtr'lar otomatik olarak nullptr'a initialize olur.
    memset(pipeline, 0, sizeof(fe_dx_pipeline_t));
    pipeline->root_signature = config->root_signature; // Root Signature'ı referans al

    // Shader Bytecode'larını Oku
    std::vector<char> vs_bytecode_data;
    if (config->vs_bytecode.file_path) {
        vs_bytecode_data = fe_dx_pipeline_read_bytecode_file(config->vs_bytecode.file_path);
        if (vs_bytecode_data.empty()) {
            FE_LOG_ERROR("Failed to load vertex shader bytecode from file: %s", config->vs_bytecode.file_path);
            fe_free(pipeline, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
            return NULL;
        }
    } else if (config->vs_bytecode.bytecode_ptr && config->vs_bytecode.bytecode_size > 0) {
        vs_bytecode_data.assign((char*)config->vs_bytecode.bytecode_ptr, (char*)config->vs_bytecode.bytecode_ptr + config->vs_bytecode.bytecode_size);
    } else {
        FE_LOG_ERROR("Vertex shader bytecode info is invalid (no file path or direct bytecode).");
        fe_free(pipeline, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
        return NULL;
    }

    std::vector<char> ps_bytecode_data;
    if (config->ps_bytecode.file_path) {
        ps_bytecode_data = fe_dx_pipeline_read_bytecode_file(config->ps_bytecode.file_path);
        if (ps_bytecode_data.empty()) {
            FE_LOG_ERROR("Failed to load pixel shader bytecode from file: %s", config->ps_bytecode.file_path);
            fe_free(pipeline, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
            return NULL;
        }
    } else if (config->ps_bytecode.bytecode_ptr && config->ps_bytecode.bytecode_size > 0) {
        ps_bytecode_data.assign((char*)config->ps_bytecode.bytecode_ptr, (char*)config->ps_bytecode.bytecode_ptr + config->ps_bytecode.bytecode_size);
    } else {
        FE_LOG_ERROR("Pixel shader bytecode info is invalid (no file path or direct bytecode).");
        fe_free(pipeline, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
        return NULL;
    }

    D3D12_SHADER_BYTECODE vs_bytecode = { vs_bytecode_data.data(), vs_bytecode_data.size() };
    D3D12_SHADER_BYTECODE ps_bytecode = { ps_bytecode_data.data(), ps_bytecode_data.size() };

    // Input Element Description'ları D3D12 formatına dönüştür
    std::vector<D3D12_INPUT_ELEMENT_DESC> d3d_input_elements(config->input_element_count);
    for (UINT i = 0; i < config->input_element_count; ++i) {
        d3d_input_elements[i].SemanticName = config->input_elements[i].semantic_name;
        d3d_input_elements[i].SemanticIndex = config->input_elements[i].semantic_index;
        d3d_input_elements[i].Format = config->input_elements[i].format;
        d3d_input_elements[i].InputSlot = config->input_elements[i].input_slot;
        d3d_input_elements[i].AlignedByteOffset = config->input_elements[i].aligned_byte_offset;
        d3d_input_elements[i].InputSlotClass = config->input_elements[i].input_slot_class;
        d3d_input_elements[i].InstanceDataStepRate = config->input_elements[i].instance_data_step_rate;
    }

    // Grafik Pipeline State Object (PSO) Tanımı
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.pRootSignature = pipeline->root_signature.Get();

    // Shader Aşamaları
    pso_desc.VS = vs_bytecode;
    pso_desc.PS = ps_bytecode;
    // Diğer shader'lar: GS, HS, DS (opsiyonel)

    // Input Layout
    pso_desc.InputLayout = { d3d_input_elements.data(), config->input_element_count };

    // Input Assembler
    pso_desc.PrimitiveTopologyType = config->primitive_topology_type;

    // Rasterizer State
    pso_desc.RasterizerState.FillMode = config->fill_mode;
    pso_desc.RasterizerState.CullMode = config->cull_mode;
    pso_desc.RasterizerState.FrontCounterClockwise = config->front_counter_clockwise;
    pso_desc.RasterizerState.DepthBias = config->depth_bias;
    pso_desc.RasterizerState.DepthBiasClamp = config->depth_bias_clamp;
    pso_desc.RasterizerState.SlopeScaledDepthBias = config->slope_scaled_depth_bias;
    pso_desc.RasterizerState.DepthClipEnable = config->depth_clip_enable;
    pso_desc.RasterizerState.MultisampleEnable = config->multisample_enable;
    pso_desc.RasterizerState.AntialiasedLineEnable = config->antialiased_line_enable;
    pso_desc.RasterizerState.ForcedSampleCount = config->forced_sample_count;
    pso_desc.RasterizerState.ConservativeRaster = config->conservative_raster;

    // Depth/Stencil State
    pso_desc.DepthStencilState.DepthEnable = config->depth_enable;
    pso_desc.DepthStencilState.DepthWriteMask = config->depth_write_mask;
    pso_desc.DepthStencilState.DepthFunc = config->depth_func;
    pso_desc.DepthStencilState.StencilEnable = config->stencil_enable;
    pso_desc.DepthStencilState.StencilReadMask = config->stencil_read_mask;
    pso_desc.DepthStencilState.StencilWriteMask = config->stencil_write_mask;
    pso_desc.DepthStencilState.FrontFace = config->front_face_stencil;
    pso_desc.DepthStencilState.BackFace = config->back_face_stencil;

    // Blend State
    pso_desc.BlendState.AlphaToCoverageEnable = config->alpha_to_coverage_enable;
    pso_desc.BlendState.IndependentBlendEnable = config->independent_blend_enable;
    for (UINT i = 0; i < 8; ++i) {
        pso_desc.BlendState.RenderTarget[i] = config->render_target_blend[i];
    }

    // Sample Desc
    pso_desc.SampleDesc.Count = config->sample_count;
    pso_desc.SampleDesc.Quality = config->sample_quality;

    // Render Target Formatları ve DSV Formatı
    pso_desc.NumRenderTargets = config->rtv_count;
    for (UINT i = 0; i < config->rtv_count; ++i) {
        pso_desc.RTVFormats[i] = config->rtv_formats[i];
    }
    pso_desc.DSVFormat = config->dsv_format;

    // Diğer ayarlar
    pso_desc.SampleMask = UINT_MAX; // Tüm örnekleri etkile

    // Pipeline'ı Oluştur
    HRESULT hr = device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline->pso));
    HR_CHECK_NULL(hr, "Failed to create graphics pipeline state object!");

    FE_LOG_INFO("Direct3D 12 graphics pipeline state object created successfully.");
    return pipeline;
}

void fe_dx_pipeline_destroy(fe_dx_pipeline_t* pipeline) {
    if (!pipeline) {
        FE_LOG_WARN("Attempted to destroy NULL fe_dx_pipeline_t.");
        return;
    }

    // ComPtr'lar kapsam dışına çıktığında otomatik olarak Release() çağırır.
    pipeline->pso.Reset();
    // Root signature burada yok edilmez, çünkü dışarıdan sağlanır ve yönetimi dışarıya aittir.
    pipeline->root_signature.Reset(); // Sadece referansı sıfırla

    fe_free(pipeline, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    FE_LOG_INFO("fe_dx_pipeline_t object freed.");
}
