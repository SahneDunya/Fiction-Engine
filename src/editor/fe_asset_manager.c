#include "core/assets/fe_asset_manager.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h"
#include "platform/fe_platform_file.h" // Genel dosya I/O fonksiyonları için (fe_platform_file_read_binary, fe_platform_file_get_size)

#include <string.h> // strcmp, strcpy için
#include <stdio.h>  // snprintf için

// DİKKAT: Bu örnekte, varlık yükleyicileri doğrudan bu dosyada mock olarak tanımlanmıştır.
// Gerçek bir motor, bu yükleyicileri ayrı modüllerde (plg_texture_loader, plg_model_loader vb.)
// tutar ve fe_asset_manager_register_loader gibi bir fonksiyonla kaydederdi.

// --- Mock Varlık Yükleyicileri ---
// Bunlar, gerçek doku/model/ses verisi yükleme ve boşaltma mantığını içerir.
// Karmaşıklığı azaltmak için burada basitçe bellek tahsisi ve boşaltması yapılır.

// Mock Doku Yapısı
typedef struct fe_texture {
    uint32_t id;
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    void* pixel_data; // Gerçek piksel verisi
} fe_texture_t;

bool fe_mock_texture_loader(const char* file_path, void** asset_out, size_t* data_size_out) {
    FE_LOG_DEBUG("Mock loading texture: %s", file_path);
    // Gerçekte: PNG/JPG yükleme kütüphanesi (stb_image gibi) kullanılır.
    
    // Basit bir dummy doku oluştur
    fe_texture_t* tex = FE_MALLOC(sizeof(fe_texture_t), FE_MEM_TYPE_ASSET_TEXTURE);
    if (!tex) {
        return false;
    }
    tex->id = 1; // Dummy ID
    tex->width = 128;
    tex->height = 128;
    tex->channels = 4;
    tex->pixel_data = FE_MALLOC(tex->width * tex->height * tex->channels, FE_MEM_TYPE_ASSET_TEXTURE_DATA);
    if (!tex->pixel_data) {
        FE_FREE(tex, FE_MEM_TYPE_ASSET_TEXTURE);
        return false;
    }
    memset(tex->pixel_data, 0xFF, tex->width * tex->height * tex->channels); // Beyaz doku

    *asset_out = tex;
    *data_size_out = sizeof(fe_texture_t) + (tex->width * tex->height * tex->channels);
    FE_LOG_INFO("Mock texture loaded: %s (size: %zu bytes)", file_path, *data_size_out);
    return true;
}

void fe_mock_texture_unloader(void* asset_data, size_t data_size) {
    FE_LOG_DEBUG("Mock unloading texture.");
    fe_texture_t* tex = (fe_texture_t*)asset_data;
    if (tex) {
        FE_FREE(tex->pixel_data, FE_MEM_TYPE_ASSET_TEXTURE_DATA);
        FE_FREE(tex, FE_MEM_TYPE_ASSET_TEXTURE);
    }
}

// Mock Model Yapısı
typedef struct fe_model {
    uint32_t id;
    uint32_t num_vertices;
    uint32_t num_indices;
    void* vertex_data;
    void* index_data;
} fe_model_t;

bool fe_mock_model_loader(const char* file_path, void** asset_out, size_t* data_size_out) {
    FE_LOG_DEBUG("Mock loading model: %s", file_path);
    // Gerçekte: OBJ/FBX/GLTF yükleme kütüphanesi kullanılır.

    fe_model_t* model = FE_MALLOC(sizeof(fe_model_t), FE_MEM_TYPE_ASSET_MODEL);
    if (!model) return false;
    model->id = 2; // Dummy ID
    model->num_vertices = 100;
    model->num_indices = 150;
    model->vertex_data = FE_MALLOC(model->num_vertices * sizeof(float) * 8, FE_MEM_TYPE_ASSET_MODEL_DATA); // Pos, Normal, UV vb.
    model->index_data = FE_MALLOC(model->num_indices * sizeof(uint32_t), FE_MEM_TYPE_ASSET_MODEL_DATA);
    if (!model->vertex_data || !model->index_data) {
        FE_FREE(model->vertex_data, FE_MEM_TYPE_ASSET_MODEL_DATA);
        FE_FREE(model->index_data, FE_MEM_TYPE_ASSET_MODEL_DATA);
        FE_FREE(model, FE_MEM_TYPE_ASSET_MODEL);
        return false;
    }
    memset(model->vertex_data, 0, model->num_vertices * sizeof(float) * 8);
    memset(model->index_data, 0, model->num_indices * sizeof(uint32_t));

    *asset_out = model;
    *data_size_out = sizeof(fe_model_t) + (model->num_vertices * sizeof(float) * 8) + (model->num_indices * sizeof(uint32_t));
    FE_LOG_INFO("Mock model loaded: %s (size: %zu bytes)", file_path, *data_size_out);
    return true;
}

void fe_mock_model_unloader(void* asset_data, size_t data_size) {
    FE_LOG_DEBUG("Mock unloading model.");
    fe_model_t* model = (fe_model_t*)asset_data;
    if (model) {
        FE_FREE(model->vertex_data, FE_MEM_TYPE_ASSET_MODEL_DATA);
        FE_FREE(model->index_data, FE_MEM_TYPE_ASSET_MODEL_DATA);
        FE_FREE(model, FE_MEM_TYPE_ASSET_MODEL);
    }
}

// Global yükleyici ve boşaltıcı fonksiyon işaretçileri dizileri
// Gerçekte bu bir hash tablosu veya daha sofistike bir kayıt sistemi olurdu.
static fe_asset_loader_func   s_asset_loaders[FE_ASSET_TYPE_COUNT];
static fe_asset_unloader_func s_asset_unloaders[FE_ASSET_TYPE_COUNT];

// --- Fonksiyon Implementasyonları ---

bool fe_asset_manager_init(fe_asset_manager_t* manager) {
    if (!manager) {
        FE_LOG_FATAL("fe_asset_manager_init: Manager pointer is NULL.");
        return false;
    }

    memset(manager, 0, sizeof(fe_asset_manager_t));
    manager->next_asset_id = 1; // ID'leri 1'den başlat

    // Hash haritasını başlat
    if (!FE_HASH_MAP_INIT(&manager->assets_cache, 128)) { // Başlangıç kapasitesi 128
        FE_LOG_FATAL("Failed to initialize asset cache hash map.");
        return false;
    }

    // Yükleyicileri kaydet (Mock implementasyonlar)
    s_asset_loaders[FE_ASSET_TYPE_TEXTURE] = fe_mock_texture_loader;
    s_asset_unloaders[FE_ASSET_TYPE_TEXTURE] = fe_mock_texture_unloader;

    s_asset_loaders[FE_ASSET_TYPE_MODEL] = fe_mock_model_loader;
    s_asset_unloaders[FE_ASSET_TYPE_MODEL] = fe_mock_model_unloader;

    FE_LOG_INFO("FE Asset Manager initialized.");
    return true;
}

void fe_asset_manager_shutdown(fe_asset_manager_t* manager) {
    if (!manager) {
        return;
    }

    FE_LOG_INFO("Shutting down FE Asset Manager. Unloading all assets...");

    // Tüm varlıkları tek tek boşalt
    FE_HASH_MAP_FOREACH(&manager->assets_cache, char*, path_key, fe_asset_t*, asset_val) {
        if (asset_val) {
            FE_LOG_DEBUG("Forcibly unloading asset: %s (ID: %llu, Type: %d)", asset_val->path, asset_val->id, asset_val->type);
            if (s_asset_unloaders[asset_val->type]) {
                s_asset_unloaders[asset_val->type](asset_val->data_ptr, asset_val->data_size);
            }
            FE_FREE(asset_val, FE_MEM_TYPE_ASSET_STRUCT); // Varlık yapısının kendisini serbest bırak
        }
    }
    
    // Hash haritasını temizle ve kapat
    FE_HASH_MAP_SHUTDOWN(&manager->assets_cache);

    FE_LOG_INFO("FE Asset Manager shut down. All assets unloaded.");
}

fe_asset_t* fe_asset_manager_load_asset(fe_asset_manager_t* manager, const char* file_path, fe_asset_type_t asset_type) {
    if (!manager || !file_path || strlen(file_path) == 0 || asset_type == FE_ASSET_TYPE_UNKNOWN || asset_type >= FE_ASSET_TYPE_COUNT) {
        FE_LOG_ERROR("fe_asset_manager_load_asset: Invalid parameters.");
        return NULL;
    }

    // 1. Önbellekte varlığı kontrol et
    fe_asset_t** cached_asset_ptr = FE_HASH_MAP_GET(&manager->assets_cache, file_path);
    if (cached_asset_ptr && *cached_asset_ptr) {
        fe_asset_t* cached_asset = *cached_asset_ptr;
        if (cached_asset->type == asset_type) { // Tip uyumsuzluğu kontrolü
            cached_asset->ref_count++;
            FE_LOG_DEBUG("Asset already loaded: %s (Ref Count: %u)", file_path, cached_asset->ref_count);
            return cached_asset;
        } else {
            FE_LOG_WARN("Asset '%s' found in cache but with mismatched type. Requested: %d, Cached: %d. Forcing reload.",
                        file_path, asset_type, cached_asset->type);
            // Tip uyumsuzluğu varsa, mevcut varlığı boşaltıp yeniden yükleyebiliriz
            fe_asset_manager_release_asset(manager, cached_asset); // Referans sayısını azalt, gerekirse boşalt
            FE_HASH_MAP_REMOVE(&manager->assets_cache, file_path); // Cache'den de kaldır ki yeniden yüklenebilsin.
        }
    }

    // 2. Yükleyici fonksiyonu bul
    fe_asset_loader_func loader = s_asset_loaders[asset_type];
    if (!loader) {
        FE_LOG_ERROR("No loader registered for asset type: %d for file: %s", asset_type, file_path);
        return NULL;
    }

    // 3. Varlığı yükle
    void* loaded_data_ptr = NULL;
    size_t loaded_data_size = 0;
    if (!loader(file_path, &loaded_data_ptr, &loaded_data_size)) {
        FE_LOG_ERROR("Failed to load asset data for: %s (Type: %d)", file_path, asset_type);
        return NULL;
    }

    // 4. fe_asset_t yapısını oluştur ve doldur
    fe_asset_t* new_asset = FE_MALLOC(sizeof(fe_asset_t), FE_MEM_TYPE_ASSET_STRUCT);
    if (!new_asset) {
        FE_LOG_ERROR("Failed to allocate fe_asset_t for: %s", file_path);
        if (s_asset_unloaders[asset_type]) { // Yüklenen veriyi boşalt
             s_asset_unloaders[asset_type](loaded_data_ptr, loaded_data_size);
        }
        return NULL;
    }

    memset(new_asset, 0, sizeof(fe_asset_t));
    new_asset->id = manager->next_asset_id++;
    strncpy(new_asset->path, file_path, sizeof(new_asset->path) - 1);
    new_asset->path[sizeof(new_asset->path) - 1] = '\0';
    new_asset->type = asset_type;
    new_asset->ref_count = 1; // İlk referans
    new_asset->data_size = loaded_data_size;
    new_asset->data_ptr = loaded_data_ptr;

    // 5. Önbelleğe ekle
    FE_HASH_MAP_INSERT(&manager->assets_cache, new_asset->path, new_asset);

    FE_LOG_INFO("Asset loaded and cached: %s (ID: %llu, Type: %d, Ref Count: %u)",
                file_path, new_asset->id, new_asset->type, new_asset->ref_count);
    return new_asset;
}

void fe_asset_manager_release_asset(fe_asset_manager_t* manager, fe_asset_t* asset) {
    if (!manager || !asset) {
        return;
    }

    asset->ref_count--;
    FE_LOG_DEBUG("Asset %s ref count decreased to %u.", asset->path, asset->ref_count);

    if (asset->ref_count == 0) {
        FE_LOG_INFO("Asset %s (ID: %llu) ref count is zero, unloading.", asset->path, asset->id);

        // Varlığı boşalt
        if (s_asset_unloaders[asset->type]) {
            s_asset_unloaders[asset->type](asset->data_ptr, asset->data_size);
        } else {
            FE_LOG_WARN("No unloader registered for asset type %d.", asset->type);
        }

        // Hash haritasından kaldır
        FE_HASH_MAP_REMOVE(&manager->assets_cache, asset->path);

        // Varlık yapısının kendisini serbest bırak
        FE_FREE(asset, FE_MEM_TYPE_ASSET_STRUCT);
    }
}

void fe_asset_manager_unload_all_assets_of_type(fe_asset_manager_t* manager, fe_asset_type_t asset_type) {
    if (!manager) {
        return;
    }

    FE_LOG_INFO("Forcibly unloading all assets of type %d...", asset_type);

    // Temp list oluştur çünkü hash map üzerinde iterasyon yaparken öğeleri sileceğiz.
    FE_DYNAMIC_ARRAY_DEFINE(fe_asset_t*) assets_to_remove;
    FE_DYNAMIC_ARRAY_INIT(&assets_to_remove, 16); // Başlangıç kapasitesi

    FE_HASH_MAP_FOREACH(&manager->assets_cache, char*, path_key, fe_asset_t*, asset_val) {
        if (asset_val && (asset_type == FE_ASSET_TYPE_UNKNOWN || asset_val->type == asset_type)) {
            FE_DYNAMIC_ARRAY_ADD(&assets_to_remove, asset_val);
        }
    }

    for (size_t i = 0; i < assets_to_remove.size; ++i) {
        fe_asset_t* asset_to_unload = FE_DYNAMIC_ARRAY_GET(&assets_to_remove, i);
        if (asset_to_unload) {
            FE_LOG_DEBUG("Forcibly unloading asset: %s (ID: %llu, Ref Count: %u)",
                         asset_to_unload->path, asset_to_unload->id, asset_to_unload->ref_count);
            
            if (s_asset_unloaders[asset_to_unload->type]) {
                s_asset_unloaders[asset_to_unload->type](asset_to_unload->data_ptr, asset_to_unload->data_size);
            }
            // Hash map'ten kaldır ve kendi struct'ını serbest bırak
            FE_HASH_MAP_REMOVE(&manager->assets_cache, asset_to_unload->path);
            FE_FREE(asset_to_unload, FE_MEM_TYPE_ASSET_STRUCT);
        }
    }
    
    FE_DYNAMIC_ARRAY_SHUTDOWN(&assets_to_remove);

    FE_LOG_INFO("Finished forcibly unloading assets of type %d.", asset_type);
}
