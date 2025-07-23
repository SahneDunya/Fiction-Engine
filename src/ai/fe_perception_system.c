#include "ai/fe_perception_system.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h"
#include "core/math/fe_math.h" // fe_vec3_dist, fe_vec3_dot, fe_vec3_normalize vb. için
#include <string.h> // memset, memcpy için
#include <math.h>   // acosf için

// --- Dahili Yardımcı Fonksiyonlar (Implementasyonda kullanılacak) ---

// İki 3D vektör arasındaki açıyı radyan cinsinden hesaplar.
float fe_vec3_angle_between(fe_vec3_t v1, fe_vec3_t v2) {
    float dot_product = fe_vec3_dot(fe_vec3_normalize(v1), fe_vec3_normalize(v2));
    // Floating point hatalarını önlemek için değeri [-1, 1] aralığında sıkıştır
    dot_product = FE_CLAMP(dot_product, -1.0f, 1.0f);
    return acosf(dot_product);
}

// Görüş hattı kontrolü (Varsayımsal implementasyon)
// Gerçek bir motor, burada çarpışma/raycast sistemini kullanacaktır.
bool fe_perception_system_check_line_of_sight(const fe_perception_system_t* system, fe_vec3_t p1, fe_vec3_t p2, uint32_t exclude_entity_id) {
    // TODO: Gerçek çarpışma/raycast sistemi ile entegre et.
    // Bu, oyunun render/physics motorundan bir raycast fonksiyonunu çağıracaktır.
    // Örneğin: return fe_collision_system_raycast(system->collision_system, p1, p2, &hit_info, exclude_entity_id);
    (void)system; // Kullanılmadığını belirtmek için
    (void)exclude_entity_id; // Kullanılmadığını belirtmek için

    // Basitlik için, her zaman görüş hattının açık olduğunu varsayıyoruz.
    // Gerçekte, arada bir engel varsa true dönecek (görüş engellendi).
    return false; // Şu an için her zaman açık
}

// Bir perceiver için algılama işlemlerini gerçekleştirir
static void fe_perception_system_process_perceiver(fe_perception_system_t* system, fe_perceiver_component_t* perceiver) {
    // Eski algılanan nesneleri temizle
    fe_array_clear(&perceiver->perceived_objects);

    size_t num_perceivables = fe_array_get_size(&system->perceivables);
    for (size_t i = 0; i < num_perceivables; ++i) {
        fe_perceivable_component_t* perceivable = (fe_perceivable_component_t*)fe_array_get_at(&system->perceivables, i);

        // Kendi kendine algılama yapma
        if (perceiver->entity_id == perceivable->entity_id) {
            continue;
        }
        // Algılanabilir aktif değilse atla
        if (!perceivable->is_active) {
            continue;
        }

        fe_perceived_object_t perceived_obj;
        memset(&perceived_obj, 0, sizeof(fe_perceived_object_t));
        perceived_obj.entity_id = perceivable->entity_id;
        perceived_obj.position = perceivable->position;
        perceived_obj.last_known_position = perceivable->position; // İlk bilinen pozisyon
        perceived_obj.timestamp_ms = system->current_game_time_ms;

        float distance = fe_vec3_dist(perceiver->position, perceivable->position);
        perceived_obj.distance = distance;

        bool perceived_this_frame = false;

        // --- Görsel Algılama (Visual Perception) ---
        // 1. Mesafe kontrolü
        if (distance <= perceiver->view_distance + perceivable->visual_radius) {
            // 2. Görüş Alanı (FOV) kontrolü
            fe_vec3_t dir_to_target = fe_vec3_normalize(fe_vec3_sub(perceivable->position, perceiver->position));
            float angle_to_target = fe_vec3_angle_between(perceiver->forward_dir, dir_to_target);

            if (angle_to_target <= perceiver->field_of_view_angle_rad * 0.5f) { // Yarım FOV açısı
                // 3. Görüş Hattı (Line of Sight) kontrolü (engelleri dikkate alır)
                if (perceivable->is_visible && !fe_perception_system_check_line_of_sight(system, perceiver->position, perceivable->position, perceiver->entity_id)) {
                    perceived_obj.type = FE_PERCEPTION_TYPE_VISUAL;
                    // Görsel gücü mesafeye göre azalsın (basit bir örnek)
                    perceived_obj.strength = 1.0f - (distance / perceiver->view_distance);
                    perceived_obj.is_hostile = true; // Örnek olarak düşman varsayalım
                    perceived_this_frame = true;
                    FE_LOG_DEBUG("Agent %u visually perceived entity %u (dist: %.2f, strength: %.2f).",
                                 perceiver->entity_id, perceivable->entity_id, distance, perceived_obj.strength);
                }
            }
        }

        // --- İşitsel Algılama (Auditory Perception) ---
        // Eğer görsel algılanmadıysa veya sadece işitsel olarak algılanıyorsa
        // Algılanabilen nesnenin ses gücü ile algılayıcının işitme mesafesini karşılaştır
        if (!perceived_this_frame && distance <= perceiver->hearing_distance && perceivable->auditory_strength > 0.0f) {
            // Ses şiddetine ve mesafeye göre algılama gücü
            float effective_auditory_strength = perceivable->auditory_strength * (1.0f - (distance / perceiver->hearing_distance));
            if (effective_auditory_strength > 0.1f) { // Minimum ses eşiği
                perceived_obj.type = FE_PERCEPTION_TYPE_AUDITORY;
                perceived_obj.strength = effective_auditory_strength;
                perceived_obj.is_hostile = false; // Ses sadece düşmandan gelmez
                perceived_this_frame = true;
                FE_LOG_DEBUG("Agent %u auditorily perceived entity %u (dist: %.2f, strength: %.2f).",
                             perceiver->entity_id, perceivable->entity_id, distance, perceived_obj.strength);
            }
        }

        // --- Yakınlık Algılaması (Proximity Perception) ---
        // Doğrudan yakınlık kontrolü (collision detection sistemi ile de birleşebilir)
        if (!perceived_this_frame && distance <= 1.5f) { // Çok yakınsa algıla (küçük bir yarıçap)
            perceived_obj.type = FE_PERCEPTION_TYPE_PROXIMITY;
            perceived_obj.strength = 1.0f; // Tam güçte algıla
            perceived_obj.is_hostile = true; // Yakınsak tehlikeli varsayalım
            perceived_this_frame = true;
            FE_LOG_DEBUG("Agent %u perceived entity %u by proximity (dist: %.2f).",
                         perceiver->entity_id, perceivable->entity_id, distance);
        }

        // Eğer herhangi bir şekilde algılandıysa listeye ekle
        if (perceived_this_frame) {
            if (!fe_array_add_element(&perceiver->perceived_objects, &perceived_obj)) {
                FE_LOG_ERROR("Failed to add perceived object to agent %u's list.", perceiver->entity_id);
            }
        }
    }
}

// --- Algılama Sistemi Fonksiyon Implementasyonları ---

bool fe_perception_system_init(fe_perception_system_t* system, 
                               size_t initial_perceiver_capacity, 
                               size_t initial_perceivable_capacity) {
    if (!system) {
        FE_LOG_ERROR("fe_perception_system_init: System pointer is NULL.");
        return false;
    }
    memset(system, 0, sizeof(fe_perception_system_t));

    if (!fe_array_init(&system->perceivers, sizeof(fe_perceiver_component_t), initial_perceiver_capacity, FE_MEM_TYPE_PERCEPTION_PERCEIVERS)) {
        FE_LOG_CRITICAL("fe_perception_system_init: Failed to initialize perceivers array.");
        return false;
    }
    fe_array_set_capacity(&system->perceivers, initial_perceiver_capacity);

    if (!fe_array_init(&system->perceivables, sizeof(fe_perceivable_component_t), initial_perceivable_capacity, FE_MEM_TYPE_PERCEPTION_PERCEIVABLES)) {
        FE_LOG_CRITICAL("fe_perception_system_init: Failed to initialize perceivables array.");
        fe_array_destroy(&system->perceivers);
        return false;
    }
    fe_array_set_capacity(&system->perceivables, initial_perceivable_capacity);

    FE_LOG_INFO("Perception System initialized with perceiver capacity %zu, perceivable capacity %zu.",
                initial_perceiver_capacity, initial_perceivable_capacity);
    return true;
}

void fe_perception_system_destroy(fe_perception_system_t* system) {
    if (!system) return;

    FE_LOG_INFO("Destroying Perception System.");

    // Tüm algılayıcı bileşenlerinin dahili perceived_objects dizilerini serbest bırak
    size_t num_perceivers = fe_array_get_size(&system->perceivers);
    for (size_t i = 0; i < num_perceivers; ++i) {
        fe_perceiver_component_t* perceiver = (fe_perceiver_component_t*)fe_array_get_at(&system->perceivers, i);
        if (fe_array_is_initialized(&perceiver->perceived_objects)) {
            fe_array_destroy(&perceiver->perceived_objects);
        }
    }

    fe_array_destroy(&system->perceivables);
    fe_array_destroy(&system->perceivers);

    memset(system, 0, sizeof(fe_perception_system_t));
}

fe_perceiver_component_t* fe_perception_system_add_perceiver(fe_perception_system_t* system, const fe_perceiver_component_t* perceiver_template) {
    if (!system || !perceiver_template) {
        FE_LOG_ERROR("fe_perception_system_add_perceiver: System or perceiver_template is NULL.");
        return NULL;
    }

    fe_perceiver_component_t new_perceiver = *perceiver_template; // Veriyi kopyala

    // Algılanan nesneler dizisini başlat
    if (!fe_array_init(&new_perceiver.perceived_objects, sizeof(fe_perceived_object_t), 16, FE_MEM_TYPE_PERCEPTION_PERCEIVED_OBJECTS)) {
        FE_LOG_CRITICAL("fe_perception_system_add_perceiver: Failed to initialize perceived_objects array for entity %u.", perceiver_template->entity_id);
        return NULL;
    }
    new_perceiver.last_update_time_ms = 0; // Başlangıçta hiç güncellenmediğini işaretle

    if (!fe_array_add_element(&system->perceivers, &new_perceiver)) {
        FE_LOG_ERROR("fe_perception_system_add_perceiver: Failed to add perceiver for entity %u.", perceiver_template->entity_id);
        fe_array_destroy(&new_perceiver.perceived_objects); // Başarısız olursa dahili diziyi serbest bırak
        return NULL;
    }
    FE_LOG_DEBUG("Perceiver added for entity ID %u.", new_perceiver.entity_id);
    return (fe_perceiver_component_t*)fe_array_get_at(&system->perceivers, fe_array_get_size(&system->perceivers) - 1);
}

fe_perceivable_component_t* fe_perception_system_add_perceivable(fe_perception_system_t* system, const fe_perceivable_component_t* perceivable_template) {
    if (!system || !perceivable_template) {
        FE_LOG_ERROR("fe_perception_system_add_perceivable: System or perceivable_template is NULL.");
        return NULL;
    }

    if (!fe_array_add_element(&system->perceivables, perceivable_template)) { // Doğrudan kopyala
        FE_LOG_ERROR("fe_perception_system_add_perceivable: Failed to add perceivable for entity %u.", perceivable_template->entity_id);
        return NULL;
    }
    FE_LOG_DEBUG("Perceivable added for entity ID %u.", perceivable_template->entity_id);
    return (fe_perceivable_component_t*)fe_array_get_at(&system->perceivables, fe_array_get_size(&system->perceivables) - 1);
}

void fe_perceiver_component_update(fe_perceiver_component_t* perceiver_component, fe_vec3_t new_pos, fe_vec3_t new_forward_dir) {
    if (!perceiver_component) return;
    perceiver_component->position = new_pos;
    perceiver_component->forward_dir = fe_vec3_normalize(new_forward_dir); // Yönün normalize olduğundan emin ol
}

void fe_perceivable_component_update(fe_perceivable_component_t* perceivable_component, fe_vec3_t new_pos, bool is_visible) {
    if (!perceivable_component) return;
    perceivable_component->position = new_pos;
    perceivable_component->is_visible = is_visible;
}

void fe_perception_system_update(fe_perception_system_t* system, uint32_t delta_time_ms, uint32_t current_game_time_ms) {
    if (!system) return;

    system->current_game_time_ms = current_game_time_ms;

    size_t num_perceivers = fe_array_get_size(&system->perceivers);
    for (size_t i = 0; i < num_perceivers; ++i) {
        fe_perceiver_component_t* perceiver = (fe_perceiver_component_t*)fe_array_get_at(&system->perceivers, i);

        // Algılama güncelleme intervalini kontrol et
        if (system->current_game_time_ms - perceiver->last_update_time_ms >= perceiver->perception_update_interval_ms) {
            fe_perception_system_process_perceiver(system, perceiver);
            perceiver->last_update_time_ms = system->current_game_time_ms;
        }
    }
    (void)delta_time_ms; // Şu an kullanılmıyor ama gelecekte animasyonlar için kullanılabilir.
}

const fe_array_t* fe_perceiver_get_perceived_objects(const fe_perceiver_component_t* perceiver) {
    if (!perceiver || !fe_array_is_initialized(&perceiver->perceived_objects)) {
        FE_LOG_ERROR("fe_perceiver_get_perceived_objects: Invalid perceiver or uninitialized array.");
        return NULL;
    }
    return &perceiver->perceived_objects;
}
