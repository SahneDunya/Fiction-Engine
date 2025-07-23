#include "graphics/metal/fe_mt_pipeline.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h"

#include <string.h> // memset
#include <vector>   // Geçici bellek için

// Objective-C hata kontrolü için basit bir makro.
// Metal fonksiyonları NSError** döndürebilir veya nil/NULL dönebilir.
#define METAL_ERROR_CHECK(obj, msg, error_ptr) \
    if (obj == nil) { \
        if {
            FE_LOG_CRITICAL("%s Error: %s", msg, .localizedDescription.UTF8String); \
        } else {
            FE_LOG_CRITICAL("%s", msg);
        }
        return NULL;
    }

// --- Dahili Yardımcı Fonksiyonlar ---

// MTLVertexDescriptor oluşturur
static MTLVertexDescriptor* fe_mt_pipeline_create_vertex_descriptor(const fe_mt_pipeline_config_t* config) {
    MTLVertexDescriptor* vertex_descriptor = [MTLVertexDescriptor vertexDescriptor];

    // Nitelikler (Attributes)
    for (NSUInteger i = 0; i < config->vertex_attribute_count; ++i) {
        fe_mt_vertex_attribute_desc_t attr_desc = config->vertex_attributes[i];
        vertex_descriptor.attributes[i].format = attr_desc.format;
        vertex_descriptor.attributes[i].offset = attr_desc.offset;
        vertex_descriptor.attributes[i].bufferIndex = attr_desc.buffer_index;
        // Opsiyonel: label ekleyebiliriz for debugging: vertex_descriptor.attributes[i].label = [NSString stringWithUTF8String:attr_desc.name];
    }

    // Düzenler (Layouts)
    for (NSUInteger i = 0; i < config->vertex_buffer_layout_count; ++i) {
        fe_mt_vertex_buffer_layout_desc_t layout_desc = config->vertex_buffer_layouts[i];
        vertex_descriptor.layouts[layout_desc.buffer_index].stride = layout_desc.stride;
        vertex_descriptor.layouts[layout_desc.buffer_index].stepFunction = layout_desc.step_function;
        vertex_descriptor.layouts[layout_desc.buffer_index].stepRate = layout_desc.step_rate;
    }

    return vertex_descriptor;
}


// --- Ana Pipeline Fonksiyonları Uygulaması ---

void fe_mt_pipeline_default_config(fe_mt_pipeline_config_t* config,
                                   MTLPixelFormat back_buffer_format,
                                   MTLPixelFormat depth_buffer_format,
                                   NSUInteger sample_count) {
    memset(config, 0, sizeof(fe_mt_pipeline_config_t));

    // Render Target Ayarları (Varsayılan olarak tek renk eki)
    config->color_attachment_count = 1;
    config->color_attachment_formats[0] = back_buffer_format;

    // Varsayılan olarak karıştırma kapalı
    for (int i = 0; i < 8; ++i) {
        config->blend_enable[i] = NO;
        config->source_rgb_blend_factor[i] = MTLBlendFactorOne;
        config->destination_rgb_blend_factor[i] = MTLBlendFactorZero;
        config->rgb_blend_operation[i] = MTLBlendOperationAdd;
        config->source_alpha_blend_factor[i] = MTLBlendFactorOne;
        config->destination_alpha_blend_factor[i] = MTLBlendFactorZero;
        config->alpha_blend_operation[i] = MTLBlendOperationAdd;
        config->color_write_mask[i] = MTLColorWriteMaskAll;
    }

    config->depth_attachment_format = depth_buffer_format;
    config->stencil_attachment_format = MTLPixelFormatInvalid; // Stencil varsayılan olarak kapalı

    // Sample Desc (MSAA için)
    config->sample_count = sample_count;

    // Primitive Topolojisi (Metal'de pipeline'ın bir parçası değildir, Command Encoder'a ayarlanır)
    config->primitive_type = MTLPrimitiveTypeTriangle; // Varsayılan

    config->subpass_index = 0; // Metal'de anlamı yoktur, uyumluluk için.
}

fe_mt_pipeline_t* fe_mt_pipeline_create(id<MTLDevice> device, id<MTLLibrary> library, const fe_mt_pipeline_config_t* config) {
    if (!device || !library || !config) {
        FE_LOG_ERROR("fe_mt_pipeline_create: device, library, or config is NULL.");
        return NULL;
    }
    if (!config->vertex_shader_name || !config->fragment_shader_name) {
        FE_LOG_ERROR("fe_mt_pipeline_create: Vertex or Fragment shader name is NULL.");
        return NULL;
    }

    fe_mt_pipeline_t* pipeline = fe_malloc(sizeof(fe_mt_pipeline_t), FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    if (!pipeline) {
        FE_LOG_CRITICAL("Failed to allocate memory for fe_mt_pipeline_t.");
        return NULL;
    }
    memset(pipeline, 0, sizeof(fe_mt_pipeline_t));
    pipeline->library = library; // Kütüphaneye referans tut

    // 1. Render Pipeline Descriptor Oluştur
    MTLRenderPipelineDescriptor* pipeline_descriptor = [[MTLRenderPipelineDescriptor alloc] init];

    // Shader Fonksiyonları
    // library, fe_mt_device'da yüklenen varsayılan kütüphane olabilir.
    id<MTLFunction> vertex_function = [pipeline->library newFunctionWithName:[NSString stringWithUTF8String:config->vertex_shader_name]];
    if (vertex_function == nil) {
        FE_LOG_CRITICAL("Failed to find Metal vertex shader function: %s", config->vertex_shader_name);
        fe_mt_pipeline_destroy(pipeline);
        return NULL;
    }
    pipeline_descriptor.vertexFunction = vertex_function;

    id<MTLFunction> fragment_function = [pipeline->library newFunctionWithName:[NSString stringWithUTF8String:config->fragment_shader_name]];
    if (fragment_function == nil) {
        FE_LOG_CRITICAL("Failed to find Metal fragment shader function: %s", config->fragment_shader_name);
        fe_mt_pipeline_destroy(pipeline);
        return NULL;
    }
    pipeline_descriptor.fragmentFunction = fragment_function;

    // Render Target Ayarları (Color Attachments)
    pipeline_descriptor.rasterizationEnabled = YES; // Rasterization'ı varsayılan olarak etkinleştir
    pipeline_descriptor.sampleCount = config->sample_count;

    for (NSUInteger i = 0; i < config->color_attachment_count; ++i) {
        MTLRenderPipelineColorAttachmentDescriptor* color_attachment = pipeline_descriptor.colorAttachments[i];
        color_attachment.pixelFormat = config->color_attachment_formats[i];

        // Karıştırma ayarları
        color_attachment.blendingEnabled = config->blend_enable[i];
        color_attachment.sourceRGBBlendFactor = config->source_rgb_blend_factor[i];
        color_attachment.destinationRGBBlendFactor = config->destination_rgb_blend_factor[i];
        color_attachment.rgbBlendOperation = config->rgb_blend_operation[i];
        color_attachment.sourceAlphaBlendFactor = config->source_alpha_blend_factor[i];
        color_attachment.destinationAlphaBlendFactor = config->destination_alpha_blend_factor[i];
        color_attachment.alphaBlendOperation = config->alpha_blend_operation[i];
        color_attachment.writeMask = config->color_write_mask[i];
    }

    // Derinlik ve Stencil Formatları
    pipeline_descriptor.depthAttachmentPixelFormat = config->depth_attachment_format;
    pipeline_descriptor.stencilAttachmentPixelFormat = config->stencil_attachment_format;

    // Vertex Giriş Düzeni
    pipeline->vertex_descriptor = fe_mt_pipeline_create_vertex_descriptor(config);
    if (pipeline->vertex_descriptor == nil) {
        FE_LOG_CRITICAL("Failed to create Metal vertex descriptor!");
        fe_mt_pipeline_destroy(pipeline);
        return NULL;
    }
    pipeline_descriptor.vertexDescriptor = pipeline->vertex_descriptor;

    // 2. Render Pipeline State Oluştur
    NSError* error = nil;
    pipeline->pso = [device newRenderPipelineStateWithDescriptor:pipeline_descriptor error:&error];
    METAL_ERROR_CHECK(pipeline->pso, "Failed to create Metal render pipeline state!", &error);

    FE_LOG_INFO("Metal render pipeline state object created successfully.");
    return pipeline;
}

void fe_mt_pipeline_destroy(fe_mt_pipeline_t* pipeline) {
    if (!pipeline) {
        FE_LOG_WARN("Attempted to destroy NULL fe_mt_pipeline_t.");
        return;
    }

    // ARC ile yönetildiği için manuel release gerekmez, nil'e set etmek yeterlidir.
    pipeline->vertex_descriptor = nil;
    pipeline->library = nil; // Referansı serbest bırak
    pipeline->pso = nil;

    fe_free(pipeline, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    FE_LOG_INFO("fe_mt_pipeline_t object freed.");
}
