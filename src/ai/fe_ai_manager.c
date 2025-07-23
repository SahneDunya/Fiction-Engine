#include "ai/fe_ai_manager.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h"
#include "core/math/fe_math.h" // fe_vec3 için

#include <string.h> // memset için

// Varsayımsal fe_ai_agent.h içeriği (tam bir AI ajanı örneği)
// Genellikle her AI ajanı, kendi içinde davranış ağacı/durum makinesi
// ve algılama/yol bulma gibi alt sistemlerden gelen verileri tutar.
// Bu dosya ayrı bir modül olarak ele alınmalıdır, ancak manager'ın
// ihtiyaç duyduğu temel yapıyı burada kısaca tanımlayalım.
// NOT: fe_ai_agent.h'ı bu örnek için buraya dahil ediyoruz,
// ancak gerçekte ayrı bir dosya olmalı ve manager bunu sadece include etmeli.
#ifndef FE_AI_AGENT_H
#define FE_AI_AGENT_H

// --- AI Ajanı Durumları (Örnek) ---
typedef enum fe_ai_state {
    FE_AI_STATE_IDLE = 0,
    FE_AI_STATE_PATROL,
    FE_AI_STATE_CHASE,
    FE_AI_STATE_ATTACK,
    FE_AI_STATE_FLEE,
    FE_AI_STATE_FIND_PATH
} fe_ai_state_t;

// --- AI Ajanı Ana Yapısı ---
typedef struct fe_ai_agent {
    uint32_t entity_id;         // Bu AI'ın kontrol ettiği varlığın ID'si
    fe_vec3_t current_pos;      // Ajanın mevcut dünya konumu
    fe_vec3_t forward_dir;      // Ajanın ileri yön vektörü
    fe_ai_state_t current_state; // Ajanın mevcut durumu
    bool is_active;             // Ajanın güncellenip güncellenmeyeceği

    // Algılama sistemi ile etkileşim için bileşen
    fe_perceiver_component_t* perceiver_comp;
    // Yol bulma sistemi ile etkileşim için yol verisi
    fe_path_t current_path;     // Ajanın takip ettiği mevcut yol

    // Davranış ağacı veya durum makinesi için veriler (basitçe bir hedef pozisyon)
    fe_vec3_t target_position;
    // ... Daha karmaşık AI davranış verileri eklenebilir (örn. hedef ID, davranış ağacı kök düğümü)

    // Callback'ler veya doğrudan erişim için AI Manager referansı
    // fe_ai_manager_t* manager; // Geriye dönük referans (isteğe bağlı)
} fe_ai_agent_t;

// Ajanın yaşam döngüsü fonksiyonları (basit örnekler)
void fe_ai_agent_init(fe_ai_agent_t* agent, uint32_t entity_id, fe_vec3_t initial_pos, fe_vec3_t initial_forward_dir, fe_perceiver_component_t* perceiver_comp);
void fe_ai_agent_destroy(fe_ai_agent_t* agent);
void fe_ai_agent_update(fe_ai_agent_t* agent, fe_ai_manager_t* manager, uint32_t delta_time_ms, uint32_t current_game_time_ms);

#endif // FE_AI_AGENT_H

// fe_ai_agent.c implementasyonları (Bu kısım normalde ayrı bir fe_ai_agent.c dosyasında olur)
// Sadece AI Manager'ın çağıracağı temel fonksiyonlar
void fe_ai_agent_init(fe_ai_agent_t* agent, uint32_t entity_id, fe_vec3_t initial_pos, fe_vec3_t initial_forward_dir, fe_perceiver_component_t* perceiver_comp) {
    if (!agent) return;
    memset(agent, 0, sizeof(fe_ai_agent_t));
    agent->entity_id = entity_id;
    agent->current_pos = initial_pos;
    agent->forward_dir = initial_forward_dir;
    agent->current_state = FE_AI_STATE_IDLE;
    agent->is_active = true;
    agent->perceiver_comp = perceiver_comp;
    // Path'i başlat, ancak içi boş olsun
    fe_path_init_empty(&agent->current_path, entity_id); 
    FE_LOG_DEBUG("AI Agent %u initialized at (%.2f, %.2f, %.2f).", entity_id, initial_pos.x, initial_pos.y, initial_pos.z);
}

void fe_ai_agent_destroy(fe_ai_agent_t* agent) {
    if (!agent) return;
    fe_path_destroy(&agent->current_path); // Varsa mevcut yolu serbest bırak
    FE_LOG_DEBUG("AI Agent %u destroyed.", agent->entity_id);
    memset(agent, 0, sizeof(fe_ai_agent_t)); // Belleği sıfırla
}

// Bu, AI ajanı davranış mantığının ana döngüsüdür.
// Normalde bir Davranış Ağacı veya Durum Makinesi tarafından yönetilir.
void fe_ai_agent_update(fe_ai_agent_t* agent, fe_ai_manager_t* manager, uint32_t delta_time_ms, uint32_t current_game_time_ms) {
    if (!agent || !agent->is_active) return;

    // 1. Algılama sistemini güncelle (ajanın güncel pozisyon ve yönünü bildir)
    // Not: Algılama sistemi kendi iç intervaliyle güncelleyecektir,
    // burada sadece agent'ın güncel verilerini iletiyoruz.
    if (agent->perceiver_comp) {
        fe_perceiver_component_update(agent->perceiver_comp, agent->current_pos, agent->forward_dir);
    }

    // 2. Algılanan nesneleri al ve davranışa karar ver
    const fe_array_t* perceived_objects = NULL;
    if (agent->perceiver_comp) {
        perceived_objects = fe_perceiver_get_perceived_objects(agent->perceiver_comp);
    }

    bool enemy_detected = false;
    fe_vec3_t enemy_pos = FE_VEC3_ZERO;

    if (perceived_objects) {
        for (size_t i = 0; i < fe_array_get_size(perceived_objects); ++i) {
            fe_perceived_object_t* obj = (fe_perceived_object_t*)fe_array_get_at(perceived_objects, i);
            if (obj->is_hostile && obj->type == FE_PERCEPTION_TYPE_VISUAL) { // Görsel olarak düşman algılandı
                enemy_detected = true;
                enemy_pos = obj->position;
                FE_LOG_INFO("Agent %u: Hostile entity %u visually detected at (%.2f,%.2f,%.2f)!",
                            agent->entity_id, obj->entity_id, enemy_pos.x, enemy_pos.y, enemy_pos.z);
                break; // İlk düşmanı bulduğumuzda yeterli
            }
        }
    }

    // 3. Davranış Mantığı (Basit Durum Makinesi)
    switch (agent->current_state) {
        case FE_AI_STATE_IDLE:
            if (enemy_detected) {
                agent->current_state = FE_AI_STATE_CHASE;
                agent->target_position = enemy_pos;
                FE_LOG_INFO("Agent %u: Changing state to CHASE (enemy detected).", agent->entity_id);
            } else {
                // Boşta dururken belki devriye gezmeye başla
                // Bu kısımda rastgele bir hedef belirleyebilir veya devriye noktalarına gidebilir.
            }
            break;

        case FE_AI_STATE_CHASE:
            if (!enemy_detected) {
                FE_LOG_INFO("Agent %u: Lost sight of enemy. Returning to IDLE.", agent->entity_id);
                agent->current_state = FE_AI_STATE_IDLE;
                fe_path_destroy(&agent->current_path); // Eski yolu temizle
            } else {
                // Hedef pozisyonunu güncelle
                agent->target_position = enemy_pos;

                // Yol bulma ve takip etme
                if (fe_path_is_completed(&agent->current_path) || fe_vec3_dist(agent->current_path.end_pos, agent->target_position) > 1.0f) {
                    // Yol tamamlandıysa veya hedef değiştiyse yeni yol bul
                    FE_LOG_DEBUG("Agent %u: Path needs recalculation or completed. Finding new path to (%.2f,%.2f,%.2f).",
                                 agent->entity_id, agent->target_position.x, agent->target_position.y, agent->target_position.z);
                    fe_path_status_t path_status = fe_pathfinder_find_path(manager->pathfinder_system,
                                                                          agent->current_pos,
                                                                          agent->target_position,
                                                                          agent->entity_id,
                                                                          &agent->current_path);
                    if (path_status != FE_PATH_STATUS_SUCCESS) {
                        FE_LOG_WARN("Agent %u: Failed to find path to target. Status: %s", agent->entity_id, fe_path_status_to_string(path_status));
                        agent->current_state = FE_AI_STATE_IDLE; // Yol bulunamazsa boşa dön
                    }
                }

                if (!fe_path_is_completed(&agent->current_path)) {
                    fe_vec3_t next_point;
                    float movement_speed = 3.0f * (delta_time_ms / 1000.0f); // Saniyede 3 birim hız
                    float tolerance = 0.5f; // Noktaya yakınlık toleransı

                    if (fe_path_get_next_point(&agent->current_path, agent->current_pos, tolerance, &next_point)) {
                        // Hedef noktaya doğru hareket et
                        fe_vec3_t move_dir = fe_vec3_sub(next_point, agent->current_pos);
                        float dist_to_point = fe_vec3_len(move_dir);

                        if (dist_to_point > movement_speed) {
                            move_dir = fe_vec3_normalize(move_dir);
                            agent->current_pos = fe_vec3_add(agent->current_pos, fe_vec3_mul_scalar(move_dir, movement_speed));
                        } else {
                            agent->current_pos = next_point; // Tam hedefe git
                        }
                        // Hareket yönünü güncelle
                        if (fe_vec3_len_sq(move_dir) > FE_EPSILON) { // Sıfır vektörden kaçın
                             agent->forward_dir = fe_vec3_normalize(move_dir);
                        }
                    }
                } else {
                    FE_LOG_INFO("Agent %u: Reached target. Changing state to IDLE.", agent->entity_id);
                    agent->current_state = FE_AI_STATE_IDLE;
                }
            }
            break;

        case FE_AI_STATE_ATTACK:
            // Saldırı mantığı buraya gelir
            // Düşmana bak, saldırı animasyonu oyna, hasar ver
            if (!enemy_detected || fe_vec3_dist(agent->current_pos, enemy_pos) > 2.0f) { // Saldırı menzilinden çıktı
                FE_LOG_INFO("Agent %u: Enemy out of attack range or lost. Changing state to CHASE/IDLE.", agent->entity_id);
                agent->current_state = enemy_detected ? FE_AI_STATE_CHASE : FE_AI_STATE_IDLE;
            } else {
                FE_LOG_INFO("Agent %u: Attacking enemy %u at (%.2f, %.2f, %.2f)!",
                            agent->entity_id, (perceived_objects && fe_array_get_size(perceived_objects) > 0) ? ((fe_perceived_object_t*)fe_array_get_at(perceived_objects, 0))->entity_id : 0,
                            enemy_pos.x, enemy_pos.y, enemy_pos.z);
                // Örneğin: fe_game_deal_damage(agent->entity_id, enemy_entity_id, 10);
            }
            break;
        // Diğer durumlar eklenebilir
        default:
            break;
    }
}
// fe_ai_agent.c implementasyonları sonu

// --- AI Manager Fonksiyon Implementasyonları ---

bool fe_ai_manager_init(fe_ai_manager_t* manager,
                        size_t initial_agent_capacity,
                        fe_perception_system_t* perception_system_ptr,
                        fe_pathfinder_t* pathfinder_system_ptr) {
    if (!manager || !perception_system_ptr || !pathfinder_system_ptr) {
        FE_LOG_ERROR("fe_ai_manager_init: Manager, perception_system, or pathfinder_system pointer is NULL.");
        return false;
    }
    memset(manager, 0, sizeof(fe_ai_manager_t));

    if (!fe_array_init(&manager->ai_agents, sizeof(fe_ai_agent_t), initial_agent_capacity, FE_MEM_TYPE_AI_AGENT)) {
        FE_LOG_CRITICAL("fe_ai_manager_init: Failed to initialize AI agents array.");
        return false;
    }
    fe_array_set_capacity(&manager->ai_agents, initial_agent_capacity);

    manager->perception_system = perception_system_ptr;
    manager->pathfinder_system = pathfinder_system_ptr;

    FE_LOG_INFO("AI Manager initialized with %zu agent capacity.", initial_agent_capacity);
    return true;
}

void fe_ai_manager_destroy(fe_ai_manager_t* manager) {
    if (!manager) return;

    FE_LOG_INFO("Destroying AI Manager.");

    // Tüm kayıtlı AI ajanlarını serbest bırak
    size_t num_agents = fe_array_get_size(&manager->ai_agents);
    for (size_t i = 0; i < num_agents; ++i) {
        fe_ai_agent_t* agent = (fe_ai_agent_t*)fe_array_get_at(&manager->ai_agents, i);
        fe_ai_agent_destroy(agent); // Ajanın kendi kaynaklarını serbest bırak
    }

    fe_array_destroy(&manager->ai_agents);

    // Alt sistemlerin referanslarını sıfırla (sahipliğini almadığı için serbest bırakmaz)
    manager->perception_system = NULL;
    manager->pathfinder_system = NULL;

    memset(manager, 0, sizeof(fe_ai_manager_t));
}

fe_ai_agent_t* fe_ai_manager_register_agent(fe_ai_manager_t* manager,
                                            uint32_t entity_id,
                                            fe_vec3_t initial_pos,
                                            fe_vec3_t initial_forward_dir) {
    if (!manager) {
        FE_LOG_ERROR("fe_ai_manager_register_agent: Manager is NULL.");
        return NULL;
    }

    // Agent için bir perceiver bileşeni oluştur ve algılama sistemine ekle
    fe_perceiver_component_t new_perceiver_template = {
        .entity_id = entity_id,
        .position = initial_pos,
        .forward_dir = initial_forward_dir,
        .field_of_view_angle_rad = FE_DEG_TO_RAD(90.0f), // Varsayılan FOV
        .view_distance = 20.0f, // Varsayılan görüş mesafesi
        .hearing_distance = 15.0f, // Varsayılan işitme mesafesi
        .perception_update_interval_ms = 200 // Her 200ms'de bir algılama güncellemesi
    };
    fe_perceiver_component_t* perceiver_comp = fe_perception_system_add_perceiver(manager->perception_system, &new_perceiver_template);
    if (!perceiver_comp) {
        FE_LOG_ERROR("fe_ai_manager_register_agent: Failed to add perceiver component for entity %u.", entity_id);
        return NULL;
    }

    // Yeni AI ajanı yapısı oluştur
    fe_ai_agent_t new_agent;
    fe_ai_agent_init(&new_agent, entity_id, initial_pos, initial_forward_dir, perceiver_comp);
    // Diğer başlangıç durumları ayarlanabilir

    if (!fe_array_add_element(&manager->ai_agents, &new_agent)) {
        FE_LOG_ERROR("fe_ai_manager_register_agent: Failed to add AI agent for entity %u.", entity_id);
        // Eğer array'e ekleyemezsek, perceiver bileşenini de geri almalıyız,
        // ancak mevcut sistemde perceiver'ları ID ile kaldırma mekanizması yok.
        // Bu durum kritik bir hatadır.
        fe_ai_agent_destroy(&new_agent); // Ajanın içsel kaynaklarını serbest bırak
        return NULL;
    }
    FE_LOG_INFO("AI Agent %u registered successfully.", entity_id);
    return (fe_ai_agent_t*)fe_array_get_at(&manager->ai_agents, fe_array_get_size(&manager->ai_agents) - 1);
}

bool fe_ai_manager_unregister_agent(fe_ai_manager_t* manager, uint32_t agent_id) {
    if (!manager) {
        FE_LOG_ERROR("fe_ai_manager_unregister_agent: Manager is NULL.");
        return false;
    }

    size_t num_agents = fe_array_get_size(&manager->ai_agents);
    for (size_t i = 0; i < num_agents; ++i) {
        fe_ai_agent_t* agent = (fe_ai_agent_t*)fe_array_get_at(&manager->ai_agents, i);
        if (agent->entity_id == agent_id) {
            fe_ai_agent_destroy(agent); // Ajanın kaynaklarını serbest bırak

            // Diziden elemanı kaldır
            if (fe_array_remove_at(&manager->ai_agents, i)) {
                FE_LOG_INFO("AI Agent %u unregistered successfully.", agent_id);
                // TODO: Algılama sisteminden de perceiver bileşenini kaldırmalıyız.
                // Current perception system does not have unregister for perceiver.
                return true;
            } else {
                FE_LOG_ERROR("fe_ai_manager_unregister_agent: Failed to remove agent %u from array.", agent_id);
                return false;
            }
        }
    }
    FE_LOG_WARN("fe_ai_manager_unregister_agent: Agent with ID %u not found.", agent_id);
    return false;
}

void fe_ai_manager_update(fe_ai_manager_t* manager, uint32_t delta_time_ms, uint32_t current_game_time_ms) {
    if (!manager) return;

    manager->current_game_time_ms = current_game_time_ms;

    // AI algılama sistemini güncelle (bu, tüm perceiver'ları kendi iç zamanlamalarına göre güncelleyecek)
    // AI ajanlarının perceiver bileşenlerini güncellemeleri, onların pozisyon ve yönlerini perception_system'a bildirmesiyle gerçekleşir.
    // perception_system_update çağrısı, bu bildirilen verileri işler.
    fe_perception_system_update(manager->perception_system, delta_time_ms, current_game_time_ms);

    // Her bir AI ajanını güncelle
    size_t num_agents = fe_array_get_size(&manager->ai_agents);
    for (size_t i = 0; i < num_agents; ++i) {
        fe_ai_agent_t* agent = (fe_ai_agent_t*)fe_array_get_at(&manager->ai_agents, i);
        if (agent->is_active) {
            fe_ai_agent_update(agent, manager, delta_time_ms, current_game_time_ms);
        }
    }
}

fe_ai_agent_t* fe_ai_manager_get_agent(const fe_ai_manager_t* manager, uint32_t entity_id) {
    if (!manager) {
        FE_LOG_ERROR("fe_ai_manager_get_agent: Manager is NULL.");
        return NULL;
    }
    size_t num_agents = fe_array_get_size(&manager->ai_agents);
    for (size_t i = 0; i < num_agents; ++i) {
        fe_ai_agent_t* agent = (fe_ai_agent_t*)fe_array_get_at(&manager->ai_agents, i);
        if (agent->entity_id == entity_id) {
            return agent;
        }
    }
    FE_LOG_WARN("fe_ai_manager_get_agent: Agent with ID %u not found.", entity_id);
    return NULL;
}
