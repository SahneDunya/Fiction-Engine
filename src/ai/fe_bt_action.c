#include "ai/fe_bt_action.h"
#include "core/utils/fe_logger.h"
#include <string.h> // For memcpy
#include <math.h>   // For sqrtf, fabsf (for distance calculations)

// Bu modül için AI bağlam yapısı (örnek)
// Genellikle bu, AI ajanının kendisi veya onunla ilgili bilgileri içerir.
// fe_bt_node_tick'e geçilen 'context' bu türde olacaktır.
typedef struct game_ai_context {
    float agent_x;
    float agent_y;
    // Diğer ajanların, hedeflerin konumları vb.
    float target_x;
    float target_y;
    uint32_t current_time_ms; // Oyunun mevcut zamanı (milisaniye)
    // ...
} game_ai_context_t;

// --- Dahili Eylem Tick Fonksiyonları ---

// Wait Eylemi Tick Fonksiyonu
// node->internal_state: kalan bekleme süresi (milisaniye)
static fe_bt_state_t fe_bt_action_wait_tick(fe_bt_node_t* node, void* context) {
    if (!node || !context) {
        FE_LOG_ERROR("Wait Action: Invalid node or context.");
        return FE_BT_STATE_FAILURE;
    }
    game_ai_context_t* ctx = (game_ai_context_t*)context;

    // Internal state, bekleme eylemine özgü dinamik veriyi tutar.
    // Init fonksiyonu yerine tick'in ilk çağrısında başlatma yapıyoruz.
    if (!node->internal_state) {
        // internal_state'i (kalan bekleme süresini) FE_MALLOC ile ayır ve başlangıç süresini ata
        uint32_t* remaining_time = (uint32_t*)FE_MALLOC(sizeof(uint32_t), FE_MEM_TYPE_BT_ACTION_STATE);
        if (!remaining_time) {
            FE_LOG_ERROR("Wait Action '%s': Failed to allocate internal state.", node->name);
            return FE_BT_STATE_FAILURE;
        }
        // node->user_data'dan beklenen süreyi al
        uint32_t* wait_duration_ptr = (uint32_t*)node->user_data;
        if (!wait_duration_ptr) {
             FE_LOG_ERROR("Wait Action '%s': Wait duration not set in user_data.", node->name);
             FE_FREE(remaining_time, FE_MEM_TYPE_BT_ACTION_STATE);
             return FE_BT_STATE_FAILURE;
        }
        *remaining_time = *wait_duration_ptr;
        node->internal_state = remaining_time;
        FE_LOG_DEBUG("Wait Action '%s': Starting wait for %u ms.", node->name, *remaining_time);
    }

    uint32_t* remaining_time = (uint32_t*)node->internal_state;

    // Simülasyon kolaylığı için sabit bir "geçen zaman" varsayalım.
    // Gerçek bir oyunda deltaTime veya ctx->time_since_last_tick kullanılmalı.
    uint32_t elapsed_time_ms = 100; // Her tick'te 100ms geçtiğini varsayalım
    
    // Basit bir kısıtlama: Pozitif zaman, 0 değil.
    if (*remaining_time == 0) {
        FE_LOG_DEBUG("Wait Action '%s': Already completed wait. SUCCESS.", node->name);
        return FE_BT_STATE_SUCCESS; // Already finished
    }

    if (*remaining_time > elapsed_time_ms) {
        *remaining_time -= elapsed_time_ms;
        FE_LOG_DEBUG("Wait Action '%s': Waiting... %u ms left. RUNNING.", node->name, *remaining_time);
        return FE_BT_STATE_RUNNING;
    } else {
        *remaining_time = 0; // Süre bitti
        FE_LOG_DEBUG("Wait Action '%s': Wait completed. SUCCESS.", node->name);
        return FE_BT_STATE_SUCCESS;
    }
}

// Wait Eylemi Destroy Fonksiyonu
static void fe_bt_action_wait_destroy(fe_bt_node_t* node) {
    if (node->internal_state) {
        FE_FREE(node->internal_state, FE_MEM_TYPE_BT_ACTION_STATE);
        node->internal_state = NULL;
    }
    // Eğer user_data da dinamik olarak tahsis edildiyse, onu da serbest bırakın.
    if (node->user_data) {
        FE_FREE(node->user_data, FE_MEM_TYPE_BT_ACTION_DATA);
        node->user_data = NULL;
    }
    FE_LOG_DEBUG("Wait Action '%s': Destroyed.", node->name);
}

// Move To Eylemi Tick Fonksiyonu
// node->user_data: fe_bt_action_move_data_t
static fe_bt_state_t fe_bt_action_move_to_tick(fe_bt_node_t* node, void* context) {
    if (!node || !node->user_data || !context) {
        FE_LOG_ERROR("Move To Action: Invalid node, user_data, or context.");
        return FE_BT_STATE_FAILURE;
    }

    game_ai_context_t* ctx = (game_ai_context_t*)context;
    fe_bt_action_move_data_t* move_data = (fe_bt_action_move_data_t*)node->user_data;

    float dx = move_data->target_x - ctx->agent_x;
    float dy = move_data->target_y - ctx->agent_y;
    float distance = sqrtf(dx * dx + dy * dy);

    if (distance <= move_data->tolerance) {
        // Hedefe ulaşıldı
        FE_LOG_DEBUG("Move To Action '%s': Reached target (%.2f, %.2f). SUCCESS.", node->name, move_data->target_x, move_data->target_y);
        return FE_BT_STATE_SUCCESS;
    }

    // Hedefe doğru hareket et
    float move_amount = move_data->speed; // Basitlik için her tick'te sabit hız
    if (distance < move_amount) {
        move_amount = distance; // Hedefi aşma
    }

    float ratio = move_amount / distance;
    ctx->agent_x += dx * ratio;
    ctx->agent_y += dy * ratio;

    FE_LOG_DEBUG("Move To Action '%s': Moving to (%.2f,%.2f). Current (%.2f,%.2f). Dist: %.2f. RUNNING.",
                 node->name, move_data->target_x, move_data->target_y, ctx->agent_x, ctx->agent_y, distance);
    return FE_BT_STATE_RUNNING;
}

// Move To Eylemi Destroy Fonksiyonu
static void fe_bt_action_move_to_destroy(fe_bt_node_t* node) {
    if (node->user_data) {
        FE_FREE(node->user_data, FE_MEM_TYPE_BT_ACTION_DATA);
        node->user_data = NULL;
    }
    FE_LOG_DEBUG("Move To Action '%s': Destroyed.", node->name);
}


// Attack Eylemi Tick Fonksiyonu
// node->user_data: fe_bt_action_attack_data_t
// node->internal_state: Dahili durum (örn. animasyon süresi, cooldown)
static fe_bt_state_t fe_bt_action_attack_tick(fe_bt_node_t* node, void* context) {
    if (!node || !node->user_data || !context) {
        FE_LOG_ERROR("Attack Action: Invalid node, user_data, or context.");
        return FE_BT_STATE_FAILURE;
    }

    game_ai_context_t* ctx = (game_ai_context_t*)context;
    fe_bt_action_attack_data_t* attack_data = (fe_bt_action_attack_data_t*)node->user_data;

    // Hedefin menzil içinde olup olmadığını kontrol et (basitçe hedef X, Y'nin context'te olduğunu varsayıyoruz)
    float dx = ctx->target_x - ctx->agent_x;
    float dy = ctx->target_y - ctx->agent_y;
    float distance_to_target_sq = dx * dx + dy * dy;

    if (distance_to_target_sq > attack_data->attack_range * attack_data->attack_range) {
        FE_LOG_WARN("Attack Action '%s': Target out of range. FAILURE.", node->name);
        return FE_BT_STATE_FAILURE;
    }
    
    // Basit bir saldırı eylemi: Sadece bir defa başarılı olur.
    // Daha karmaşık eylemler için internal_state kullanılabilir (örn. saldırı animasyonu süresi).
    FE_LOG_DEBUG("Attack Action '%s': Attacking target with ability ID %u. SUCCESS.", node->name, attack_data->ability_id);

    // Gerçek bir oyunda burada hasar verme, animasyon oynatma, cooldown başlatma vb. olur.
    // Örnek olarak, context'teki mermiyi azaltabiliriz:
    // ctx->has_ammo = false; // Mermi bittiğini varsayalım

    return FE_BT_STATE_SUCCESS;
}

// Attack Eylemi Destroy Fonksiyonu
static void fe_bt_action_attack_destroy(fe_bt_node_t* node) {
    if (node->user_data) {
        FE_FREE(node->user_data, FE_MEM_TYPE_BT_ACTION_DATA);
        node->user_data = NULL;
    }
    // internal_state varsa onu da serbest bırakın
    if (node->internal_state) {
        FE_FREE(node->internal_state, FE_MEM_TYPE_BT_ACTION_STATE);
        node->internal_state = NULL;
    }
    FE_LOG_DEBUG("Attack Action '%s': Destroyed.", node->name);
}


// --- Eylem Düğümü Yapıcı Fonksiyonları ---

fe_bt_node_t* fe_bt_action_create_wait(const char* name, uint32_t wait_duration_ms) {
    // wait_duration_ms değerini dinamik olarak ayırıp user_data olarak ata
    uint32_t* duration_data = (uint32_t*)FE_MALLOC(sizeof(uint32_t), FE_MEM_TYPE_BT_ACTION_DATA);
    if (!duration_data) {
        FE_LOG_CRITICAL("Failed to allocate duration data for Wait action.");
        return NULL;
    }
    *duration_data = wait_duration_ms;

    fe_bt_node_t* node = fe_bt_node_create_leaf(name,
                                                fe_bt_action_wait_tick,
                                                NULL, // init_func yok, tick içinde hallediliyor
                                                fe_bt_action_wait_destroy);
    if (!node) {
        FE_FREE(duration_data, FE_MEM_TYPE_BT_ACTION_DATA);
        return NULL;
    }
    fe_bt_node_set_user_data(node, duration_data); // Kullanıcı verisi olarak bekleme süresi işaretçisi
    FE_LOG_DEBUG("Created Wait action: '%s' (Duration: %u ms).", name, wait_duration_ms);
    return node;
}

fe_bt_node_t* fe_bt_action_create_move_to(const char* name, const fe_bt_action_move_data_t* data) {
    if (!data) {
        FE_LOG_ERROR("Move To Action: Provided data is NULL.");
        return NULL;
    }
    fe_bt_action_move_data_t* move_data = (fe_bt_action_move_data_t*)FE_MALLOC(sizeof(fe_bt_action_move_data_t), FE_MEM_TYPE_BT_ACTION_DATA);
    if (!move_data) {
        FE_LOG_CRITICAL("Failed to allocate move data for Move To action.");
        return NULL;
    }
    memcpy(move_data, data, sizeof(fe_bt_action_move_data_t));

    fe_bt_node_t* node = fe_bt_node_create_leaf(name,
                                                fe_bt_action_move_to_tick,
                                                NULL, // init_func yok
                                                fe_bt_action_move_to_destroy);
    if (!node) {
        FE_FREE(move_data, FE_MEM_TYPE_BT_ACTION_DATA);
        return NULL;
    }
    fe_bt_node_set_user_data(node, move_data);
    FE_LOG_DEBUG("Created Move To action: '%s' (Target: %.2f, %.2f, Speed: %.2f).",
                 name, data->target_x, data->target_y, data->speed);
    return node;
}

fe_bt_node_t* fe_bt_action_create_attack(const char* name, const fe_bt_action_attack_data_t* data) {
    if (!data) {
        FE_LOG_ERROR("Attack Action: Provided data is NULL.");
        return NULL;
    }
    fe_bt_action_attack_data_t* attack_data = (fe_bt_action_attack_data_t*)FE_MALLOC(sizeof(fe_bt_action_attack_data_t), FE_MEM_TYPE_BT_ACTION_DATA);
    if (!attack_data) {
        FE_LOG_CRITICAL("Failed to allocate attack data for Attack action.");
        return NULL;
    }
    memcpy(attack_data, data, sizeof(fe_bt_action_attack_data_t));

    fe_bt_node_t* node = fe_bt_node_create_leaf(name,
                                                fe_bt_action_attack_tick,
                                                NULL, // init_func yok
                                                fe_bt_action_attack_destroy);
    if (!node) {
        FE_FREE(attack_data, FE_MEM_TYPE_BT_ACTION_DATA);
        return NULL;
    }
    fe_bt_node_set_user_data(node, attack_data);
    FE_LOG_DEBUG("Created Attack action: '%s' (Ability ID: %u, Range: %.2f).",
                 name, data->ability_id, data->attack_range);
    return node;
}
