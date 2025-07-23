#include "editor/fe_material_editor.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h"
#include "core/utils/fe_string_builder.h" // String işlemleri için
#include "core/utils/fe_file_system.h"   // Dosya işlemleri için

// Harici kütüphane bağımlılıkları (JSON için cJSON veya benzeri)
// Bu örnekte basitleştirilmiş bir JSON okuma/yazma mantığı kullanılacaktır.
// Gerçek bir uygulamada cJSON, Jansson gibi daha sağlam bir kütüphane tercih edilmelidir.

#include <stdio.h>
#include <string.h>
#include <stdlib.h> // malloc, realloc, free

// Global materyal düzenleyici durumu (singleton)
static fe_material_editor_state_t g_material_editor_state;
static bool g_is_initialized = false;

// --- Dahili Yardımcı Fonksiyonlar ---

// Materyal parametresini serbest bırakır
static void fe_material_editor_free_parameter(fe_material_parameter_t* param) {
    if (param->type == FE_MATERIAL_PARAM_TYPE_TEXTURE2D) {
        // Texture referansı serbest bırakılmaz, çünkü burası sahibiyetini almaz.
        // fe_texture_manager veya benzeri bir yer tarafından yönetilir.
        param->value.texture_val = NULL;
    }
    fe_string_destroy(&param->name);
}

// Materyali serbest bırakır
static void fe_material_editor_free_material(fe_material_t* material) {
    if (!material) return;

    fe_string_destroy(&material->id);
    fe_string_destroy(&material->name);
    fe_string_destroy(&material->shader_path);
    fe_string_destroy(&material->description);

    if (material->parameters) {
        for (size_t i = 0; i < material->parameter_count; ++i) {
            fe_material_editor_free_parameter(&material->parameters[i]);
        }
        fe_free(material->parameters, FE_MEM_TYPE_EDITOR, __FILE__, __LINE__);
    }
    fe_free(material, FE_MEM_TYPE_EDITOR, __FILE__, __LINE__);
}

// Materyal parametresini JSON'a serileştirir (basit örnek)
// Gerçek bir senaryoda cJSON gibi bir kütüphane kullanılmalı
static void fe_material_editor_serialize_param_to_json(FILE* file, const fe_material_parameter_t* param) {
    fprintf(file, "      {\n");
    fprintf(file, "        \"name\": \"%s\",\n", param->name.data);
    fprintf(file, "        \"type\": \"%s\",\n", fe_material_param_type_to_string(param->type));
    fprintf(file, "        \"value\": ");

    switch (param->type) {
        case FE_MATERIAL_PARAM_TYPE_FLOAT:
            fprintf(file, "%.6f\n", param->value.float_val);
            break;
        case FE_MATERIAL_PARAM_TYPE_VEC2:
            fprintf(file, "[%.6f, %.6f]\n", param->value.vec2_val.x, param->value.vec2_val.y);
            break;
        case FE_MATERIAL_PARAM_TYPE_VEC3:
        case FE_MATERIAL_PARAM_TYPE_COLOR_RGB:
            fprintf(file, "[%.6f, %.6f, %.6f]\n", param->value.vec3_val.x, param->value.vec3_val.y, param->value.vec3_val.z);
            break;
        case FE_MATERIAL_PARAM_TYPE_VEC4:
        case FE_MATERIAL_PARAM_TYPE_COLOR_RGBA:
            fprintf(file, "[%.6f, %.6f, %.6f, %.6f]\n", param->value.vec4_val.x, param->value.vec4_val.y, param->value.vec4_val.z, param->value.vec4_val.w);
            break;
        case FE_MATERIAL_PARAM_TYPE_INT:
            fprintf(file, "%d\n", param->value.int_val);
            break;
        case FE_MATERIAL_PARAM_TYPE_BOOL:
            fprintf(file, "%s\n", param->value.bool_val ? "true" : "false");
            break;
        case FE_MATERIAL_PARAM_TYPE_TEXTURE2D:
            if (param->value.texture_val && param->value.texture_val->id.data) {
                fprintf(file, "\"%s\"\n", param->value.texture_val->id.data); // Sadece doku ID'sini/yolunu kaydet
            } else {
                fprintf(file, "\"null\"\n"); // Dokusuz
            }
            break;
        default:
            fprintf(file, "\"unsupported_type\"\n");
            break;
    }
    fprintf(file, "      }");
}

// Parametre tipini string'e çevirir (ve tersi için gerekli olabilir)
const char* fe_material_param_type_to_string(fe_material_param_type_t type) {
    switch (type) {
        case FE_MATERIAL_PARAM_TYPE_FLOAT: return "float";
        case FE_MATERIAL_PARAM_TYPE_VEC2: return "vec2";
        case FE_MATERIAL_PARAM_TYPE_VEC3: return "vec3";
        case FE_MATERIAL_PARAM_TYPE_VEC4: return "vec4";
        case FE_MATERIAL_PARAM_TYPE_COLOR_RGB: return "color_rgb";
        case FE_MATERIAL_PARAM_TYPE_COLOR_RGBA: return "color_rgba";
        case FE_MATERIAL_PARAM_TYPE_INT: return "int";
        case FE_MATERIAL_PARAM_TYPE_BOOL: return "bool";
        case FE_MATERIAL_PARAM_TYPE_TEXTURE2D: return "texture2d";
        default: return "unknown";
    }
}

// Materyal parametresini JSON'dan deseralize eder (basit örnek)
// Bu kısım, dosya okuma ve JSON parsing için daha karmaşık olacaktır.
// Genellikle cJSON gibi bir kütüphane kullanılır.
// Bu fonksiyon sadece bir placeholder'dır ve tam bir JSON parser içermez.
// Sadece örnek amaçlı bir iskelet sağlanmıştır.
static fe_material_editor_error_t fe_material_editor_deserialize_param_from_json(
    fe_material_parameter_t* param,
    const char* param_name, const char* param_type_str, const char* param_value_str)
{
    fe_string_init(&param->name, param_name);

    if (strcmp(param_type_str, "float") == 0) {
        param->type = FE_MATERIAL_PARAM_TYPE_FLOAT;
        param->value.float_val = (float)atof(param_value_str);
    } else if (strcmp(param_type_str, "vec2") == 0) {
        param->type = FE_MATERIAL_PARAM_TYPE_VEC2;
        // Basit JSON için manuel parse, cJSON burada çok daha iyi olurdu
        sscanf(param_value_str, "[%f, %f]", &param->value.vec2_val.x, &param->value.vec2_val.y);
    } // ... diğer tipler için benzer sscanf/atof/atoi çağrıları
    else if (strcmp(param_type_str, "texture2d") == 0) {
        param->type = FE_MATERIAL_PARAM_TYPE_TEXTURE2D;
        // Param_value_str doku ID/yolu. fe_texture_manager'dan yükle/al
        // param->value.texture_val = fe_texture_manager_get_texture(param_value_str); // Varsayımsal çağrı
        param->value.texture_val = NULL; // Şimdilik NULL
    }
    // ... diğer tipler

    return FE_MATERIAL_EDITOR_SUCCESS;
}


// Materyal listesini yeniden boyutlandır
static fe_material_editor_error_t fe_material_editor_resize_material_list(size_t new_capacity) {
    fe_material_t** new_list = fe_realloc(g_material_editor_state.loaded_materials,
                                          new_capacity * sizeof(fe_material_t*),
                                          FE_MEM_TYPE_EDITOR, __FILE__, __LINE__);
    if (!new_list) {
        FE_LOG_CRITICAL("Failed to reallocate memory for material list.");
        return FE_MATERIAL_EDITOR_OUT_OF_MEMORY;
    }
    g_material_editor_state.loaded_materials = new_list;
    g_material_editor_state.material_capacity = new_capacity;
    return FE_MATERIAL_EDITOR_SUCCESS;
}

// Materyal parametre listesini yeniden boyutlandır
static fe_material_editor_error_t fe_material_editor_resize_parameter_list(fe_material_t* material, size_t new_capacity) {
    fe_material_parameter_t* new_list = fe_realloc(material->parameters,
                                                  new_capacity * sizeof(fe_material_parameter_t),
                                                  FE_MEM_TYPE_EDITOR, __FILE__, __LINE__);
    if (!new_list) {
        FE_LOG_CRITICAL("Failed to reallocate memory for material parameters.");
        return FE_MATERIAL_EDITOR_OUT_OF_MEMORY;
    }
    material->parameters = new_list;
    material->parameter_capacity = new_capacity;
    return FE_MATERIAL_EDITOR_SUCCESS;
}

// --- Ana Fonksiyonlar Uygulaması ---

fe_material_editor_error_t fe_material_editor_init() {
    if (g_is_initialized) {
        FE_LOG_WARN("Material editor already initialized.");
        return FE_MATERIAL_EDITOR_SUCCESS;
    }

    g_material_editor_state.loaded_materials = NULL;
    g_material_editor_state.material_count = 0;
    g_material_editor_state.material_capacity = 0;
    g_material_editor_state.current_material_being_edited = NULL;

    g_is_initialized = true;
    FE_LOG_INFO("Material editor initialized.");
    return FE_MATERIAL_EDITOR_SUCCESS;
}

void fe_material_editor_shutdown() {
    if (!g_is_initialized) {
        FE_LOG_WARN("Material editor not initialized.");
        return;
    }

    // Yüklü tüm materyalleri serbest bırak
    for (size_t i = 0; i < g_material_editor_state.material_count; ++i) {
        fe_material_editor_free_material(g_material_editor_state.loaded_materials[i]);
    }
    if (g_material_editor_state.loaded_materials) {
        fe_free(g_material_editor_state.loaded_materials, FE_MEM_TYPE_EDITOR, __FILE__, __LINE__);
        g_material_editor_state.loaded_materials = NULL;
    }

    g_material_editor_state.material_count = 0;
    g_material_editor_state.material_capacity = 0;
    g_material_editor_state.current_material_being_edited = NULL;
    g_is_initialized = false;
    FE_LOG_INFO("Material editor shutdown complete.");
}

fe_material_t* fe_material_editor_create_new_material(const char* name, const char* shader_path) {
    if (!g_is_initialized) {
        FE_LOG_ERROR("Material editor not initialized.");
        return NULL;
    }
    if (!name || !shader_path) {
        FE_LOG_ERROR("Cannot create new material: name or shader_path is NULL.");
        return NULL;
    }

    fe_material_t* new_material = fe_malloc(sizeof(fe_material_t), FE_MEM_TYPE_EDITOR, __FILE__, __LINE__);
    if (!new_material) {
        FE_LOG_CRITICAL("Failed to allocate memory for new material.");
        return NULL;
    }
    memset(new_material, 0, sizeof(fe_material_t));

    // Basit bir ID oluşturma (UUID kütüphanesi kullanılabilir)
    // Şimdilik adı ve rastgele bir sayı ile
    fe_string_builder_t sb;
    fe_string_builder_init(&sb);
    fe_string_builder_append_c_str(&sb, name);
    fe_string_builder_append_c_str(&sb, "_");
    fe_string_builder_append_int(&sb, (int)fe_get_current_time_ms()); // Benzersiz olması için
    fe_string_init(&new_material->id, fe_string_builder_get_string(&sb));
    fe_string_builder_destroy(&sb);

    fe_string_init(&new_material->name, name);
    fe_string_init(&new_material->shader_path, shader_path);
    fe_string_init(&new_material->description, "New material created by editor.");
    new_material->is_dirty = true; // Yeni oluşturulduğu için kirli

    // Materyal listesine ekle
    if (g_material_editor_state.material_count >= g_material_editor_state.material_capacity) {
        size_t new_capacity = g_material_editor_state.material_capacity == 0 ? 4 : g_material_editor_state.material_capacity * 2;
        if (fe_material_editor_resize_material_list(new_capacity) != FE_MATERIAL_EDITOR_SUCCESS) {
            fe_material_editor_free_material(new_material);
            return NULL;
        }
    }
    g_material_editor_state.loaded_materials[g_material_editor_state.material_count++] = new_material;
    g_material_editor_state.current_material_being_edited = new_material;

    FE_LOG_INFO("New material '%s' created with ID '%s'.", name, new_material->id.data);
    return new_material;
}

fe_material_t* fe_material_editor_load_material(const char* file_path) {
    if (!g_is_initialized) {
        FE_LOG_ERROR("Material editor not initialized.");
        return NULL;
    }
    if (!file_path) {
        FE_LOG_ERROR("Cannot load material: file_path is NULL.");
        return NULL;
    }

    FE_LOG_INFO("Loading material from: %s", file_path);

    // TODO: Materyal dosyasını oku ve ayrıştır (örneğin JSON)
    // Bu kısım tam bir JSON kütüphanesi (cJSON) ile yapılmalıdır.
    // Şimdilik basit bir placeholder:

    // Örnek bir materyal oluşturup varsayılan değerleri atayalım.
    // Gerçekte, dosya içeriğinden veriler okunup yeni_materyal'e atanır.
    fe_material_t* loaded_material = fe_material_editor_create_new_material("LoadedMaterial", "shaders/default.glsl"); // Geçici
    if (!loaded_material) return NULL;

    fe_string_destroy(&loaded_material->id); // Geçici ID'yi kaldır
    fe_string_init(&loaded_material->id, file_path); // Dosya yolunu ID olarak kullan
    fe_string_destroy(&loaded_material->name);
    fe_string_init(&loaded_material->name, fe_file_system_get_filename_from_path(file_path));

    // Eğer materyal zaten yüklüyse, eskisini kaldır
    fe_material_t* existing_material = fe_material_editor_get_material_by_id(loaded_material->id.data);
    if (existing_material) {
        fe_material_editor_remove_material(loaded_material->id.data);
    }
    // Materyal listesine ekleme create_new_material içinde zaten yapıldı.

    loaded_material->is_dirty = false; // Yüklendiği için kirli değil
    g_material_editor_state.current_material_being_edited = loaded_material;

    FE_LOG_INFO("Material '%s' loaded successfully.", loaded_material->name.data);
    return loaded_material;
}

fe_material_editor_error_t fe_material_editor_save_material(fe_material_t* material, const char* file_path) {
    if (!g_is_initialized) {
        FE_LOG_ERROR("Material editor not initialized.");
        return FE_MATERIAL_EDITOR_NOT_INITIALIZED;
    }
    if (!material) {
        FE_LOG_ERROR("Cannot save NULL material.");
        return FE_MATERIAL_EDITOR_INVALID_ARGUMENT;
    }

    const char* path_to_save = file_path;
    fe_string_t default_path;
    if (!path_to_save) {
        // Varsayılan bir yol oluştur: "materials/<material_id>.fem"
        fe_string_builder_t sb;
        fe_string_builder_init(&sb);
        fe_string_builder_append_c_str(&sb, "assets/materials/"); // Varsayılan materyal klasörü
        fe_string_builder_append_c_str(&sb, material->id.data);
        fe_string_builder_append_c_str(&sb, ".fem"); // Fiction Engine Material
        fe_string_init(&default_path, fe_string_builder_get_string(&sb));
        fe_string_builder_destroy(&sb);
        path_to_save = default_path.data;
    }

    FE_LOG_INFO("Saving material '%s' to: %s", material->name.data, path_to_save);

    FILE* file = fopen(path_to_save, "w");
    if (!file) {
        FE_LOG_ERROR("Failed to open file for writing: %s", path_to_save);
        if (!file_path) fe_string_destroy(&default_path);
        return FE_MATERIAL_EDITOR_FILE_WRITE_ERROR;
    }

    // JSON formatında yaz (basit örnek)
    fprintf(file, "{\n");
    fprintf(file, "  \"id\": \"%s\",\n", material->id.data);
    fprintf(file, "  \"name\": \"%s\",\n", material->name.data);
    fprintf(file, "  \"shader_path\": \"%s\",\n", material->shader_path.data);
    fprintf(file, "  \"description\": \"%s\",\n", material->description.data);
    fprintf(file, "  \"parameters\": [\n");

    for (size_t i = 0; i < material->parameter_count; ++i) {
        fe_material_editor_serialize_param_to_json(file, &material->parameters[i]);
        if (i < material->parameter_count - 1) {
            fprintf(file, ",\n");
        } else {
            fprintf(file, "\n");
        }
    }

    fprintf(file, "  ]\n");
    fprintf(file, "}\n");

    fclose(file);
    material->is_dirty = false; // Kaydedildiği için kirli değil

    if (!file_path) fe_string_destroy(&default_path);
    FE_LOG_INFO("Material '%s' saved successfully.", material->name.data);
    return FE_MATERIAL_EDITOR_SUCCESS;
}

fe_material_editor_error_t fe_material_editor_remove_material(const char* material_id) {
    if (!g_is_initialized) {
        FE_LOG_ERROR("Material editor not initialized.");
        return FE_MATERIAL_EDITOR_NOT_INITIALIZED;
    }
    if (!material_id) {
        FE_LOG_ERROR("Cannot remove material: material_id is NULL.");
        return FE_MATERIAL_EDITOR_INVALID_ARGUMENT;
    }

    size_t found_index = (size_t)-1;
    for (size_t i = 0; i < g_material_editor_state.material_count; ++i) {
        if (fe_string_equals_c_str(&g_material_editor_state.loaded_materials[i]->id, material_id)) {
            found_index = i;
            break;
        }
    }

    if (found_index == (size_t)-1) {
        FE_LOG_WARN("Material with ID '%s' not found for removal.", material_id);
        return FE_MATERIAL_EDITOR_MATERIAL_NOT_FOUND;
    }

    fe_material_t* material_to_remove = g_material_editor_state.loaded_materials[found_index];

    // Eğer kaldırılan materyal şu an düzenlenen materyalse, NULL'la
    if (g_material_editor_state.current_material_being_edited == material_to_remove) {
        g_material_editor_state.current_material_being_edited = NULL;
    }

    fe_material_editor_free_material(material_to_remove);

    // Diziden çıkar
    for (size_t i = found_index; i < g_material_editor_state.material_count - 1; ++i) {
        g_material_editor_state.loaded_materials[i] = g_material_editor_state.loaded_materials[i + 1];
    }
    g_material_editor_state.material_count--;

    FE_LOG_INFO("Material with ID '%s' removed.", material_id);
    return FE_MATERIAL_EDITOR_SUCCESS;
}


fe_material_editor_error_t fe_material_editor_add_parameter(fe_material_t* material, const char* name, fe_material_param_type_t type, fe_material_param_value_t initial_value) {
    if (!material || !name) {
        return FE_MATERIAL_EDITOR_INVALID_ARGUMENT;
    }
    // Parametre zaten var mı kontrol et
    if (fe_material_editor_get_parameter(material, name) != NULL) {
        FE_LOG_WARN("Parameter '%s' already exists in material '%s'.", name, material->name.data);
        return FE_MATERIAL_EDITOR_UNKNOWN_ERROR; // Veya daha spesifik bir hata kodu
    }

    if (material->parameter_count >= material->parameter_capacity) {
        size_t new_capacity = material->parameter_capacity == 0 ? 4 : material->parameter_capacity * 2;
        if (fe_material_editor_resize_parameter_list(material, new_capacity) != FE_MATERIAL_EDITOR_SUCCESS) {
            return FE_MATERIAL_EDITOR_OUT_OF_MEMORY;
        }
    }

    fe_material_parameter_t* new_param = &material->parameters[material->parameter_count++];
    fe_string_init(&new_param->name, name);
    new_param->type = type;
    new_param->value = initial_value;
    material->is_dirty = true;

    FE_LOG_DEBUG("Added parameter '%s' to material '%s'.", name, material->name.data);
    return FE_MATERIAL_EDITOR_SUCCESS;
}

fe_material_editor_error_t fe_material_editor_update_parameter(fe_material_t* material, const char* name, fe_material_param_value_t new_value) {
    if (!material || !name) {
        return FE_MATERIAL_EDITOR_INVALID_ARGUMENT;
    }

    fe_material_parameter_t* param = fe_material_editor_get_parameter(material, name);
    if (!param) {
        FE_LOG_WARN("Parameter '%s' not found in material '%s' for update.", name, material->name.data);
        return FE_MATERIAL_EDITOR_UNKNOWN_ERROR; // Veya MATERIAL_PARAM_NOT_FOUND
    }

    // Type checking could be done here: if (param->type != expected_type) { error; }
    param->value = new_value;
    material->is_dirty = true;

    FE_LOG_DEBUG("Updated parameter '%s' in material '%s'.", name, material->name.data);
    return FE_MATERIAL_EDITOR_SUCCESS;
}

fe_material_editor_error_t fe_material_editor_remove_parameter(fe_material_t* material, const char* name) {
    if (!material || !name) {
        return FE_MATERIAL_EDITOR_INVALID_ARGUMENT;
    }

    size_t found_index = (size_t)-1;
    for (size_t i = 0; i < material->parameter_count; ++i) {
        if (fe_string_equals_c_str(&material->parameters[i].name, name)) {
            found_index = i;
            break;
        }
    }

    if (found_index == (size_t)-1) {
        FE_LOG_WARN("Parameter '%s' not found in material '%s' for removal.", name, material->name.data);
        return FE_MATERIAL_EDITOR_UNKNOWN_ERROR; // Veya MATERIAL_PARAM_NOT_FOUND
    }

    fe_material_editor_free_parameter(&material->parameters[found_index]);

    // Diziden çıkar
    for (size_t i = found_index; i < material->parameter_count - 1; ++i) {
        material->parameters[i] = material->parameters[i + 1];
    }
    material->parameter_count--;
    material->is_dirty = true;

    FE_LOG_DEBUG("Removed parameter '%s' from material '%s'.", name, material->name.data);
    return FE_MATERIAL_EDITOR_SUCCESS;
}

fe_material_parameter_t* fe_material_editor_get_parameter(fe_material_t* material, const char* name) {
    if (!material || !name) {
        return NULL;
    }
    for (size_t i = 0; i < material->parameter_count; ++i) {
        if (fe_string_equals_c_str(&material->parameters[i].name, name)) {
            return &material->parameters[i];
        }
    }
    return NULL;
}

fe_material_t* fe_material_editor_get_material_by_id(const char* material_id) {
    if (!g_is_initialized || !material_id) {
        return NULL;
    }
    for (size_t i = 0; i < g_material_editor_state.material_count; ++i) {
        if (fe_string_equals_c_str(&g_material_editor_state.loaded_materials[i]->id, material_id)) {
            return g_material_editor_state.loaded_materials[i];
        }
    }
    return NULL;
}

fe_material_t* fe_material_editor_get_current_material() {
    return g_material_editor_state.current_material_being_edited;
}

void fe_material_editor_set_current_material(fe_material_t* material) {
    g_material_editor_state.current_material_being_edited = material;
}
