#include "ai/fe_bt_node.h"
#include "core/utils/fe_logger.h"
#include <string.h> // For strncpy, memset

// --- Dahili Yardımcı Fonksiyonlar ---

// Sequence düğümü için tick fonksiyonu
static fe_bt_state_t fe_bt_sequence_tick(fe_bt_node_t* node, void* context) {
    if (!node || node->type != FE_BT_NODE_TYPE_SEQUENCE) {
        FE_LOG_ERROR("fe_bt_sequence_tick: Invalid node or type.");
        return FE_BT_STATE_FAILURE;
    }

    size_t* current_child_idx = (size_t*)node->internal_state;
    if (!current_child_idx) { // İlk kez çalışıyorsa veya sıfırlanmışsa
        node->internal_state = FE_MALLOC(sizeof(size_t), FE_MEM_TYPE_BT_INTERNAL);
        if (!node->internal_state) {
            FE_LOG_ERROR("fe_bt_sequence_tick: Failed to allocate internal state.");
            return FE_BT_STATE_FAILURE;
        }
        current_child_idx = (size_t*)node->internal_state;
        *current_child_idx = 0;
    }

    // Çocukları sırayla işle
    while (*current_child_idx < fe_array_get_size(&node->children)) {
        fe_bt_node_t* child = *(fe_bt_node_t**)fe_array_get_at(&node->children, *current_child_idx);
        
        // Eğer çocuk zaten Running durumundaysa, resetlemeye gerek yok
        if (child->current_state != FE_BT_STATE_RUNNING) {
            fe_bt_node_reset(child); // Çocuğu başlat veya sıfırla
        }

        fe_bt_state_t child_state = fe_bt_node_tick(child, context);

        if (child_state == FE_BT_STATE_RUNNING) {
            node->current_state = FE_BT_STATE_RUNNING;
            FE_LOG_TRACE("BT Node '%s' (Sequence) is RUNNING (child '%s' running).", node->name, child->name);
            return FE_BT_STATE_RUNNING; // Bu çocuk hala çalışıyor, Sequence de çalışıyor
        } else if (child_state == FE_BT_STATE_FAILURE) {
            node->current_state = FE_BT_STATE_FAILURE;
            *current_child_idx = 0; // Sıfırla
            FE_LOG_TRACE("BT Node '%s' (Sequence) is FAILURE (child '%s' failed).", node->name, child->name);
            return FE_BT_STATE_FAILURE; // Bir çocuk başarısız oldu, Sequence başarısız
        } else { // child_state == FE_BT_STATE_SUCCESS
            (*current_child_idx)++; // Sonraki çocuğa geç
        }
    }

    // Tüm çocuklar başarıyla tamamlandıysa
    *current_child_idx = 0; // Sıfırla
    node->current_state = FE_BT_STATE_SUCCESS;
    FE_LOG_TRACE("BT Node '%s' (Sequence) is SUCCESS (all children succeeded).", node->name);
    return FE_BT_STATE_SUCCESS;
}

// Selector düğümü için tick fonksiyonu
static fe_bt_state_t fe_bt_selector_tick(fe_bt_node_t* node, void* context) {
    if (!node || node->type != FE_BT_NODE_TYPE_SELECTOR) {
        FE_LOG_ERROR("fe_bt_selector_tick: Invalid node or type.");
        return FE_BT_STATE_FAILURE;
    }

    size_t* current_child_idx = (size_t*)node->internal_state;
    if (!current_child_idx) {
        node->internal_state = FE_MALLOC(sizeof(size_t), FE_MEM_TYPE_BT_INTERNAL);
        if (!node->internal_state) {
            FE_LOG_ERROR("fe_bt_selector_tick: Failed to allocate internal state.");
            return FE_BT_STATE_FAILURE;
        }
        current_child_idx = (size_t*)node->internal_state;
        *current_child_idx = 0;
    }

    // Çocukları sırayla işle
    while (*current_child_idx < fe_array_get_size(&node->children)) {
        fe_bt_node_t* child = *(fe_bt_node_t**)fe_array_get_at(&node->children, *current_child_idx);

        if (child->current_state != FE_BT_STATE_RUNNING) {
            fe_bt_node_reset(child); // Çocuğu başlat veya sıfırla
        }

        fe_bt_state_t child_state = fe_bt_node_tick(child, context);

        if (child_state == FE_BT_STATE_RUNNING) {
            node->current_state = FE_BT_STATE_RUNNING;
            FE_LOG_TRACE("BT Node '%s' (Selector) is RUNNING (child '%s' running).", node->name, child->name);
            return FE_BT_STATE_RUNNING; // Bu çocuk hala çalışıyor, Selector de çalışıyor
        } else if (child_state == FE_BT_STATE_SUCCESS) {
            node->current_state = FE_BT_STATE_SUCCESS;
            *current_child_idx = 0; // Sıfırla
            FE_LOG_TRACE("BT Node '%s' (Selector) is SUCCESS (child '%s' succeeded).", node->name, child->name);
            return FE_BT_STATE_SUCCESS; // Bir çocuk başarılı oldu, Selector başarılı
        } else { // child_state == FE_BT_STATE_FAILURE
            (*current_child_idx)++; // Sonraki çocuğa geç
        }
    }

    // Tüm çocuklar başarısız olduysa
    *current_child_idx = 0; // Sıfırla
    node->current_state = FE_BT_STATE_FAILURE;
    FE_LOG_TRACE("BT Node '%s' (Selector) is FAILURE (all children failed).", node->name);
    return FE_BT_STATE_FAILURE;
}

// Leaf düğümü için varsayılan tick fonksiyonu (eğer özel bir fonksiyon sağlanmazsa)
// Bu sadece bir placeholder'dır, Leaf düğümlerinin kendi özelleşmiş tick fonksiyonları olmalıdır.
static fe_bt_state_t fe_bt_leaf_default_tick(fe_bt_node_t* node, void* context) {
    FE_LOG_WARN("BT Node '%s' (Leaf) has no custom tick function. Returning FAILURE.", node->name);
    return FE_BT_STATE_FAILURE;
}

// Kompozit düğümler için internal_state'i serbest bırakma
static void fe_bt_composite_destroy_internal_state(fe_bt_node_t* node) {
    if (node && node->internal_state) {
        FE_FREE(node->internal_state, FE_MEM_TYPE_BT_INTERNAL);
        node->internal_state = NULL;
    }
}

// --- Davranış Ağacı Soyut Düğüm Fonksiyonları ---

bool fe_bt_node_base_init(fe_bt_node_t* node, fe_bt_node_type_t type, const char* name,
                           PFN_fe_bt_node_tick tick_func, PFN_fe_bt_node_init_func init_func,
                           PFN_fe_bt_node_destroy_func destroy_func, size_t initial_child_capacity) {
    if (!node || !name || !tick_func) {
        FE_LOG_ERROR("fe_bt_node_base_init: Invalid arguments (node, name, or tick_func is NULL).");
        return false;
    }

    memset(node, 0, sizeof(fe_bt_node_t)); // Yapıyı sıfırla

    node->type = type;
    strncpy(node->name, name, sizeof(node->name) - 1);
    node->name[sizeof(node->name) - 1] = '\0'; // Null sonlandırıcı garanti altına al

    node->current_state = FE_BT_STATE_FAILURE; // Başlangıçta başarısız
    node->is_initialized = false;

    node->tick_func = tick_func;
    node->init_func = init_func;
    node->destroy_func = destroy_func;

    // Kompozit düğümler için çocuk dizisini başlat
    if (type == FE_BT_NODE_TYPE_SEQUENCE || type == FE_BT_NODE_TYPE_SELECTOR) {
        if (!fe_array_init(&node->children, sizeof(fe_bt_node_t*), initial_child_capacity, FE_MEM_TYPE_BT_NODE)) {
            FE_LOG_CRITICAL("fe_bt_node_base_init: Failed to initialize children array for composite node '%s'.", name);
            return false;
        }
        fe_array_set_capacity(&node->children, initial_child_capacity);
    } else {
        // Leaf düğümlerin çocuk dizisi olmamalı
        memset(&node->children, 0, sizeof(fe_array_t)); // Güvenlik için sıfırla
    }

    FE_LOG_DEBUG("BT Node '%s' (Type: %d) base initialized.", node->name, node->type);
    return true;
}

void fe_bt_node_destroy(fe_bt_node_t* node) {
    if (!node) return;

    FE_LOG_DEBUG("Destroying BT Node '%s' (Type: %d).", node->name, node->type);

    // Eğer bir destroy fonksiyonu varsa çağır
    if (node->destroy_func) {
        node->destroy_func(node);
    }

    // Kompozit düğümler için çocukları da yok et
    if (node->type == FE_BT_NODE_TYPE_SEQUENCE || node->type == FE_BT_NODE_TYPE_SELECTOR) {
        for (size_t i = 0; i < fe_array_get_size(&node->children); ++i) {
            fe_bt_node_t* child = *(fe_bt_node_t**)fe_array_get_at(&node->children, i);
            if (child) {
                fe_bt_node_destroy(child); // Özyinelemeli olarak çocukları yok et
                // child objesi FE_MALLOC ile tahsis edildiği için burada FREE'lenmesi gerekebilir.
                // fe_bt_node_create_* fonksiyonları child'ı zaten FE_MALLOC yapıyor.
                FE_FREE(child, FE_MEM_TYPE_BT_NODE); 
            }
        }
        fe_array_destroy(&node->children);
        fe_bt_composite_destroy_internal_state(node); // Internal state'i temizle
    } else if (node->internal_state) {
         // Leaf düğümlerin de internal_state'i olabilir, örneğin bir sayıcı
         FE_FREE(node->internal_state, FE_MEM_TYPE_BT_INTERNAL);
         node->internal_state = NULL;
    }
    
    // Kendisini sıfırla
    memset(node, 0, sizeof(fe_bt_node_t));
}

fe_bt_state_t fe_bt_node_tick(fe_bt_node_t* node, void* context) {
    if (!node || !node->tick_func) {
        FE_LOG_ERROR("fe_bt_node_tick: Invalid node or missing tick_func.");
        return FE_BT_STATE_FAILURE;
    }

    // Eğer düğüm daha önce başlatılmadıysa, başlatma fonksiyonunu çağır
    if (!node->is_initialized) {
        if (node->init_func) {
            node->init_func(node);
        }
        node->is_initialized = true;
        node->current_state = FE_BT_STATE_RUNNING; // Yeni başlatılan düğüm genelde çalışıyor olarak başlar
        FE_LOG_TRACE("BT Node '%s' initialized.", node->name);
    }

    // Düğümün özel tick fonksiyonunu çağır
    node->current_state = node->tick_func(node, context);

    // Eğer düğüm tamamlandıysa (SUCCESS veya FAILURE), bir sonraki tick için sıfırla
    if (node->current_state != FE_BT_STATE_RUNNING) {
        node->is_initialized = false; // Bir sonraki çalıştırmada tekrar init çağrılması için
        if (node->type == FE_BT_NODE_TYPE_SEQUENCE || node->type == FE_BT_NODE_TYPE_SELECTOR) {
            if (node->internal_state) {
                // Kompozit düğümlerin dahili durumunu sıfırla
                *((size_t*)node->internal_state) = 0;
            }
        }
        FE_LOG_TRACE("BT Node '%s' finished with state: %d.", node->name, node->current_state);
    }

    return node->current_state;
}

bool fe_bt_node_add_child(fe_bt_node_t* parent, fe_bt_node_t* child) {
    if (!parent || !child) {
        FE_LOG_ERROR("fe_bt_node_add_child: Parent or child is NULL.");
        return false;
    }
    if (parent->type != FE_BT_NODE_TYPE_SEQUENCE && parent->type != FE_BT_NODE_TYPE_SELECTOR) {
        FE_LOG_ERROR("fe_bt_node_add_child: Node '%s' (Type: %d) is not a composite node.", parent->name, parent->type);
        return false;
    }
    if (!fe_array_add_element(&parent->children, &child)) { // Çocuk işaretçisini ekle
        FE_LOG_ERROR("fe_bt_node_add_child: Failed to add child '%s' to parent '%s'.", child->name, parent->name);
        return false;
    }
    FE_LOG_DEBUG("BT Node '%s' added as child to '%s'.", child->name, parent->name);
    return true;
}

void fe_bt_node_reset(fe_bt_node_t* node) {
    if (!node) return;

    FE_LOG_TRACE("Resetting BT Node '%s'.", node->name);
    node->current_state = FE_BT_STATE_FAILURE; // Resetlendiğinde varsayılan durum
    node->is_initialized = false;

    // Eğer bir kompozit düğümse, dahili durumunu ve çocuklarının durumunu sıfırla
    if (node->type == FE_BT_NODE_TYPE_SEQUENCE || node->type == FE_BT_NODE_TYPE_SELECTOR) {
        if (node->internal_state) {
            *((size_t*)node->internal_state) = 0; // Mevcut çocuk indeksini sıfırla
        }
        for (size_t i = 0; i < fe_array_get_size(&node->children); ++i) {
            fe_bt_node_t* child = *(fe_bt_node_t**)fe_array_get_at(&node->children, i);
            if (child) {
                fe_bt_node_reset(child); // Özyinelemeli olarak çocukları sıfırla
            }
        }
    } else { // Leaf düğümlerin de internal state'i olabilir
        // Eğer internal_state kendisi bir pointer ise ve sıfırlanması gerekiyorsa burada yapılmalı.
        // Genelde leaf düğümlerin state'i init/destroy ile yönetilir.
        // Buradaki basit internal_state yönetimi sadece composite düğümler için tasarlandı.
    }
}


void fe_bt_node_set_user_data(fe_bt_node_t* node, void* user_data) {
    if (node) {
        node->user_data = user_data;
    }
}

void* fe_bt_node_get_user_data(const fe_bt_node_t* node) {
    if (!node) {
        return NULL;
    }
    return node->user_data;
}


// --- Düğüm Tipleri için Yapıcı Fonksiyonlar ---

fe_bt_node_t* fe_bt_node_create_sequence(const char* name) {
    fe_bt_node_t* node = (fe_bt_node_t*)FE_MALLOC(sizeof(fe_bt_node_t), FE_MEM_TYPE_BT_NODE);
    if (!node) {
        FE_LOG_CRITICAL("Failed to allocate memory for Sequence node.");
        return NULL;
    }
    // Sequence düğümleri için başlangıç çocuk kapasitesi belirleyebiliriz.
    // Varsayılan olarak 4 veya 8 iyi bir başlangıç olabilir.
    if (!fe_bt_node_base_init(node, FE_BT_NODE_TYPE_SEQUENCE, name, fe_bt_sequence_tick, NULL, fe_bt_composite_destroy_internal_state, 8)) {
        FE_FREE(node, FE_MEM_TYPE_BT_NODE); // Başarısız olursa serbest bırak
        return NULL;
    }
    FE_LOG_DEBUG("Created Sequence node: '%s'.", name);
    return node;
}

fe_bt_node_t* fe_bt_node_create_selector(const char* name) {
    fe_bt_node_t* node = (fe_bt_node_t*)FE_MALLOC(sizeof(fe_bt_node_t), FE_MEM_TYPE_BT_NODE);
    if (!node) {
        FE_LOG_CRITICAL("Failed to allocate memory for Selector node.");
        return NULL;
    }
    if (!fe_bt_node_base_init(node, FE_BT_NODE_TYPE_SELECTOR, name, fe_bt_selector_tick, NULL, fe_bt_composite_destroy_internal_state, 8)) {
        FE_FREE(node, FE_MEM_TYPE_BT_NODE);
        return NULL;
    }
    FE_LOG_DEBUG("Created Selector node: '%s'.", name);
    return node;
}

fe_bt_node_t* fe_bt_node_create_leaf(const char* name, PFN_fe_bt_node_tick tick_func,
                                     PFN_fe_bt_node_init_func init_func, PFN_fe_bt_node_destroy_func destroy_func) {
    if (!tick_func) {
        FE_LOG_ERROR("fe_bt_node_create_leaf: Leaf node must have a tick function.");
        return NULL;
    }
    fe_bt_node_t* node = (fe_bt_node_t*)FE_MALLOC(sizeof(fe_bt_node_t), FE_MEM_TYPE_BT_NODE);
    if (!node) {
        FE_LOG_CRITICAL("Failed to allocate memory for Leaf node.");
        return NULL;
    }
    if (!fe_bt_node_base_init(node, FE_BT_NODE_TYPE_LEAF, name, tick_func, init_func, destroy_func, 0)) {
        FE_FREE(node, FE_MEM_TYPE_BT_NODE);
        return NULL;
    }
    FE_LOG_DEBUG("Created Leaf node: '%s'.", name);
    return node;
}
