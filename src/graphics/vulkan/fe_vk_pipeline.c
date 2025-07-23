#include "graphics/vulkan/fe_vk_pipeline.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h"

#include <string.h> // memset
#include <stdio.h>  // fopen, fread, fclose
#include <stdlib.h> // size_t

// --- Dahili Yardımcı Fonksiyonlar ---

// SPV dosyasını okur
static size_t fe_vk_pipeline_read_file(const char* filename, char** buffer) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        FE_LOG_ERROR("Failed to open shader file: %s", filename);
        return 0;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    *buffer = fe_malloc(file_size, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    if (!(*buffer)) {
        FE_LOG_ERROR("Failed to allocate memory for shader file: %s", filename);
        fclose(file);
        return 0;
    }

    size_t bytes_read = fread(*buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        FE_LOG_ERROR("Failed to read entire shader file: %s", filename);
        fe_free(*buffer, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
        *buffer = NULL;
        fclose(file);
        return 0;
    }

    fclose(file);
    return file_size;
}

// Shader modülü oluşturma
VkShaderModule fe_vk_pipeline_create_shader_module(VkDevice logical_device, const char* code_path) {
    char* code_buffer = NULL;
    size_t code_size = fe_vk_pipeline_read_file(code_path, &code_buffer);

    if (code_size == 0 || code_buffer == NULL) {
        FE_LOG_ERROR("Failed to read shader code from file: %s", code_path);
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code_size,
        .pCode = (const uint32_t*)code_buffer
    };

    VkShaderModule shader_module;
    VkResult result = vkCreateShaderModule(logical_device, &create_info, NULL, &shader_module);
    if (result != VK_SUCCESS) {
        FE_LOG_ERROR("Failed to create shader module from '%s'! VkResult: %d", code_path, result);
        shader_module = VK_NULL_HANDLE;
    } else {
        FE_LOG_DEBUG("Shader module created from: %s", code_path);
    }

    fe_free(code_buffer, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    return shader_module;
}

void fe_vk_pipeline_destroy_shader_module(VkDevice logical_device, VkShaderModule shader_module) {
    if (shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(logical_device, shader_module, NULL);
        // FE_LOG_DEBUG("Shader module destroyed."); // Çok sık çağrıldığı için log spamini önlemek adına yorum satırı yapıldı.
    }
}

// --- Ana Pipeline Fonksiyonları Uygulaması ---

void fe_vk_pipeline_default_config(fe_vk_pipeline_config_t* config, uint32_t width, uint32_t height) {
    memset(config, 0, sizeof(fe_vk_pipeline_config_t));

    // Vertex Input (Örnek: Position ve Color)
    // Bu, örnek bir varsayılan konfigürasyondur. Gerçekte, kullanılacak vertex formatına göre dinamik olarak oluşturulur.
    static fe_vk_vertex_input_binding_description_t default_binding = {
        .binding = 0,
        .stride = (3 * sizeof(float)) + (3 * sizeof(float)), // Pos (vec3) + Color (vec3)
        .input_rate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    config->binding_description_count = 1;
    config->binding_descriptions = &default_binding;

    static fe_vk_vertex_input_attribute_description_t default_attributes[2] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0 }, // Position
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = (3 * sizeof(float)) } // Color
    };
    config->attribute_description_count = 2;
    config->attribute_descriptions = default_attributes;

    // Input Assembly
    config->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config->primitive_restart_enable = VK_FALSE;

    // Viewport & Scissor
    config->viewport.x = 0.0f;
    config->viewport.y = 0.0f;
    config->viewport.width = (float)width;
    config->viewport.height = (float)height;
    config->viewport.minDepth = 0.0f;
    config->viewport.maxDepth = 1.0f;

    config->scissor.offset.x = 0;
    config->scissor.offset.y = 0;
    config->scissor.extent.width = width;
    config->scissor.extent.height = height;

    // Rasterization
    config->polygon_mode = VK_POLYGON_MODE_FILL;
    config->cull_mode = VK_CULL_MODE_BACK_BIT;
    config->front_face = VK_FRONT_FACE_CLOCKWISE; // Sağ el koordinat sistemi için genellikle saat yönü
    config->line_width = 1.0f;
    config->depth_bias_enable = VK_FALSE;

    // Multisampling
    config->rasterization_samples = VK_SAMPLE_COUNT_1_BIT; // MSAA kapalı varsayılan
    config->sample_shading_enable = VK_FALSE;
    config->min_sample_shading = 1.0f;

    // Depth/Stencil Test
    config->depth_test_enable = VK_TRUE;
    config->depth_write_enable = VK_TRUE;
    config->depth_compare_op = VK_COMPARE_OP_LESS;
    config->stencil_test_enable = VK_FALSE;

    // Color Blending (Tek bir renk eki için)
    config->blend_enable = VK_FALSE; // Varsayılan olarak karıştırma kapalı
    config->src_color_blend_factor = VK_BLEND_FACTOR_ONE;
    config->dst_color_blend_factor = VK_BLEND_FACTOR_ZERO;
    config->color_blend_op = VK_BLEND_OP_ADD;
    config->src_alpha_blend_factor = VK_BLEND_FACTOR_ONE;
    config->dst_alpha_blend_factor = VK_BLEND_FACTOR_ZERO;
    config->alpha_blend_op = VK_BLEND_OP_ADD;
    config->color_write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    // Dinamik Durumlar (viewport ve scissor genellikle dinamiktir)
    static VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    config->dynamic_state_count = (uint32_t)ARRAY_SIZE(dynamic_states);
    config->dynamic_states = dynamic_states;

    config->subpass_index = 0; // Genellikle ilk alt geçiş
}


fe_vk_pipeline_t* fe_vk_pipeline_create(VkDevice logical_device, const fe_vk_pipeline_config_t* config) {
    if (logical_device == VK_NULL_HANDLE || config == NULL) {
        FE_LOG_ERROR("fe_vk_pipeline_create: logical_device or config is NULL.");
        return NULL;
    }
    if (config->pipeline_layout == VK_NULL_HANDLE) {
        FE_LOG_ERROR("fe_vk_pipeline_create: Pipeline layout is NULL. A pipeline layout must be provided.");
        return NULL;
    }
    if (config->render_pass == NULL) {
        FE_LOG_ERROR("fe_vk_pipeline_create: Render pass is NULL. A valid render pass must be provided.");
        return NULL;
    }

    fe_vk_pipeline_t* pipeline = fe_malloc(sizeof(fe_vk_pipeline_t), FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    if (!pipeline) {
        FE_LOG_ERROR("Failed to allocate memory for fe_vk_pipeline_t.");
        return NULL;
    }
    memset(pipeline, 0, sizeof(fe_vk_pipeline_t));
    pipeline->pipeline_layout = config->pipeline_layout; // Layout'u kopyala

    // Shader Aşamaları Oluşturma
    VkPipelineShaderStageCreateInfo* shader_stages_create_info = fe_malloc(sizeof(VkPipelineShaderStageCreateInfo) * config->shader_stage_count, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    if (!shader_stages_create_info) {
        FE_LOG_ERROR("Failed to allocate memory for shader stage create infos.");
        fe_free(pipeline, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
        return NULL;
    }

    for (uint32_t i = 0; i < config->shader_stage_count; ++i) {
        VkShaderModule shader_module = fe_vk_pipeline_create_shader_module(logical_device, config->shader_stages[i].file_path);
        if (shader_module == VK_NULL_HANDLE) {
            FE_LOG_ERROR("Failed to create shader module for stage %d from file: %s", config->shader_stages[i].stage, config->shader_stages[i].file_path);
            // Oluşturulan shader modüllerini temizle
            for (uint32_t j = 0; j < i; ++j) {
                fe_vk_pipeline_destroy_shader_module(logical_device, shader_stages_create_info[j].module);
            }
            fe_free(shader_stages_create_info, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
            fe_free(pipeline, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
            return NULL;
        }

        shader_stages_create_info[i] = (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = config->shader_stages[i].stage,
            .module = shader_module,
            .pName = config->shader_stages[i].entry_point
        };

        if (config->shader_stages[i].stage == VK_SHADER_STAGE_VERTEX_BIT) {
            pipeline->vertex_shader_module = shader_module;
        } else if (config->shader_stages[i].stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
            pipeline->fragment_shader_module = shader_module;
        }
    }

    // Vertex Giriş Oluşturma
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = config->binding_description_count,
        .pVertexBindingDescriptions = config->binding_descriptions,
        .vertexAttributeDescriptionCount = config->attribute_description_count,
        .pVertexAttributeDescriptions = config->attribute_descriptions
    };

    // Giriş Birleştirici Oluşturma
    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = config->topology,
        .primitiveRestartEnable = config->primitive_restart_enable
    };

    // Viewport ve Scissor Oluşturma
    VkPipelineViewportStateCreateInfo viewport_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &config->viewport,
        .scissorCount = 1,
        .pScissors = &config->scissor
    };

    // Rasterization Oluşturma
    VkPipelineRasterizationStateCreateInfo rasterization_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE, // Derinliği [0,1] aralığına kenetler
        .rasterizerDiscardEnable = VK_FALSE, // Geometriyi çizim aşamasına geçirmeyi engeller
        .polygonMode = config->polygon_mode,
        .lineWidth = config->line_width,
        .cullMode = config->cull_mode,
        .frontFace = config->front_face,
        .depthBiasEnable = config->depth_bias_enable,
        .depthBiasConstantFactor = 0.0f, // Opsiyonel
        .depthBiasClamp = 0.0f,          // Opsiyonel
        .depthBiasSlopeFactor = 0.0f     // Opsiyonel
    };

    // Multisampling Oluşturma
    VkPipelineMultisampleStateCreateInfo multisampling_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = config->sample_shading_enable,
        .minSampleShading = config->min_sample_shading,
        .pSampleMask = NULL, // Opsiyonel
        .alphaToCoverageEnable = VK_FALSE, // Opsiyonel
        .alphaToOneEnable = VK_FALSE,    // Opsiyonel
        .rasterizationSamples = config->rasterization_samples
    };

    // Derinlik ve Stencil Oluşturma
    VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = config->depth_test_enable,
        .depthWriteEnable = config->depth_write_enable,
        .depthCompareOp = config->depth_compare_op,
        .depthBoundsTestEnable = VK_FALSE, // Opsiyonel
        .stencilTestEnable = config->stencil_test_enable,
        .front = {0}, // Opsiyonel: Stencil operasyonları
        .back = {0},  // Opsiyonel: Stencil operasyonları
        .minDepthBounds = 0.0f, // Opsiyonel
        .maxDepthBounds = 1.0f  // Opsiyonel
    };
    // Stencil operasyonları için .front ve .back doldurulmalı eğer stencil_test_enable true ise.

    // Renk Karıştırma Oluşturma (Her bir renk eki için ayrı ayar)
    // Şu an tek bir renk eki varsayıyoruz
    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .blendEnable = config->blend_enable,
        .srcColorBlendFactor = config->src_color_blend_factor,
        .dstColorBlendFactor = config->dst_color_blend_factor,
        .colorBlendOp = config->color_blend_op,
        .srcAlphaBlendFactor = config->src_alpha_blend_factor,
        .dstAlphaBlendFactor = config->dst_alpha_blend_factor,
        .alphaBlendOp = config->alpha_blend_op,
        .colorWriteMask = config->color_write_mask
    };

    VkPipelineColorBlendStateCreateInfo color_blending_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE, // Mantık operasyonları etkinleştirilsin mi? (blend ile çakışır)
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1, // Şu an için tek bir renk eki
        .pAttachments = &color_blend_attachment,
        .blendConstants[0] = 0.0f, // Opsiyonel
        .blendConstants[1] = 0.0f, // Opsiyonel
        .blendConstants[2] = 0.0f, // Opsiyonel
        .blendConstants[3] = 0.0f  // Opsiyonel
    };

    // Dinamik Durumlar Oluşturma
    VkPipelineDynamicStateCreateInfo dynamic_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = config->dynamic_state_count,
        .pDynamicStates = config->dynamic_states
    };

    // Grafik Pipeline'ı Oluşturma
    VkGraphicsPipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = config->shader_stage_count,
        .pStages = shader_stages_create_info,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly_info,
        .pViewportState = &viewport_state_info,
        .pRasterizationState = &rasterization_info,
        .pMultisampleState = &multisampling_info,
        .pDepthStencilState = &depth_stencil_info,
        .pColorBlendState = &color_blending_info,
        .pDynamicState = &dynamic_state_info, // Dinamik durumlar varsa ekle
        .layout = pipeline->pipeline_layout,
        .renderPass = pipeline->render_pass->api_handle, // fe_render_pass_t'nin içindeki API handle'ı kullan
        .subpass = config->subpass_index,
        .basePipelineHandle = VK_NULL_HANDLE, // Opsiyonel: Daha hızlı pipeline türetme için
        .basePipelineIndex = -1             // Opsiyonel
    };

    VkResult result = vkCreateGraphicsPipelines(logical_device, VK_NULL_HANDLE, 1, &pipeline_create_info, NULL, &pipeline->graphics_pipeline);
    if (result != VK_SUCCESS) {
        FE_LOG_ERROR("Failed to create graphics pipeline! VkResult: %d", result);
        // Hata durumunda oluşturulan shader modüllerini temizle
        for (uint32_t i = 0; i < config->shader_stage_count; ++i) {
            fe_vk_pipeline_destroy_shader_module(logical_device, shader_stages_create_info[i].module);
        }
        fe_free(shader_stages_create_info, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
        fe_free(pipeline, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
        return NULL;
    }

    FE_LOG_INFO("Vulkan graphics pipeline created successfully.");
    fe_free(shader_stages_create_info, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__); // Geçici shader aşamaları bilgisi
    return pipeline;
}

void fe_vk_pipeline_destroy(VkDevice logical_device, fe_vk_pipeline_t* pipeline) {
    if (!pipeline) {
        FE_LOG_WARN("Attempted to destroy NULL fe_vk_pipeline_t.");
        return;
    }
    if (logical_device == VK_NULL_HANDLE) {
        FE_LOG_ERROR("fe_vk_pipeline_destroy: logical_device is NULL.");
        return;
    }

    // Pipeline'ı yok etmeden önce ilişkili shader modüllerini yok et
    fe_vk_pipeline_destroy_shader_module(logical_device, pipeline->vertex_shader_module);
    fe_vk_pipeline_destroy_shader_module(logical_device, pipeline->fragment_shader_module);
    // Diğer shader modülleri varsa onlar da burada yok edilmelidir.

    if (pipeline->graphics_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(logical_device, pipeline->graphics_pipeline, NULL);
        FE_LOG_INFO("Vulkan graphics pipeline destroyed.");
    }
    // Pipeline layout burada yok edilmez, çünkü fe_vk_pipeline'ın kendisi oluşturmaz,
    // dışarıdan sağlanır ve yönetimi dışarıya aittir.

    fe_free(pipeline, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    FE_LOG_INFO("fe_vk_pipeline_t object freed.");
}
