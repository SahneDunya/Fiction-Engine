#include "animation/fe_anim_controller.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h"
#include "core/math/fe_math.h" // fe_mat4_lerp, fe_clampf gibi matematik fonksiyonları için

// --- Dahili Yardımcı Fonksiyonlar ---

/**
 * @brief Belirli bir katmandaki kemiğin animasyonlu dönüşümünü alır.
 * Eğer katman kısmi maske kullanıyorsa ve kemik maske dışında kalıyorsa, kimlik matrisi döner.
 *
 * @param layer Animasyon katmanı.
 * @param skeleton Kemiğin ait olduğu iskelet.
 * @param bone_index Kemiğin indeksi.
 * @param animation_time Mevcut animasyon zamanı.
 * @return fe_mat4 Kemiğin animasyonlu yerel dönüşüm matrisi.
 */
static fe_mat4 get_animated_local_bone_transform_for_layer(
    const fe_anim_layer_t* layer,
    const fe_skeleton_t* skeleton,
    int bone_index,
    float animation_time)
{
    fe_mat4 animated_transform = FE_MAT4_IDENTITY;
    fe_skeleton_bone_t* bone = (fe_skeleton_bone_t*)fe_array_get_at(&skeleton->bones, bone_index);

    // Kısmi maske kontrolü
    if (layer->use_partial_mask) {
        // Eğer bir kök kemik adı belirtilmişse ve bu kemik hiyerarşide değilse, bu katmanı atla
        // Bu basit bir kontrol, tam maskeleme için daha karmaşık bir ağaç geçişi gerekebilir.
        if (fe_string_is_empty(&layer->affected_bone_root) || !fe_string_equal(&layer->affected_bone_root, bone->name.data)) {
            // Eğer kemik, maskenin kök kemiği veya onun çocuğu değilse, sadece bind pose dönüşümünü kullan
            // Daha gelişmiş maskeleme için, bu kemiğin layer->affected_bone_root kemiğinin çocuğu olup olmadığını kontrol etmeliyiz.
            // Şimdilik sadece kök kemik ise maske uygular, diğerleri bind pose'dan gelir
            // TODO: A tree traversal to check if bone_index is a descendant of affected_bone_root is needed here.
            
            // Basit maskeleme için: Eğer kemik kök maske kemiği değilse ve üst katmanda bir transform varsa
            // Bu kemik için bu katmanın etkisini sıfırla veya varsayılan yerel dönüşümü kullan.
            // Şimdilik, sadece ilgili kemiğin kendisini maskeleyebiliriz.
            
            // Eğer kemik adı maskenin kök kemik adı değilse, bu katman bu kemiği etkilemez.
            bool is_affected_bone = false;
            if (!fe_string_is_empty(&layer->affected_bone_root)) {
                // Kemiğin hiyerarşisinde affected_bone_root olup olmadığını kontrol et.
                int current_bone_idx = bone_index;
                while (current_bone_idx != -1) {
                    fe_skeleton_bone_t* current_bone = (fe_skeleton_bone_t*)fe_array_get_at(&skeleton->bones, current_bone_idx);
                    if (fe_string_equal(&layer->affected_bone_root, current_bone->name.data)) {
                        is_affected_bone = true;
                        break;
                    }
                    current_bone_idx = current_bone->parent_index;
                }
            } else {
                // Kök kemik belirtilmemişse, maske tüm iskeleti etkiler
                is_affected_bone = true;
            }

            if (!is_affected_bone) {
                // Kemiğin yerel dönüşümünü bind pose'dan al
                return bone->local_transform;
            }
        }
    }

    // Animasyon durumundan yerel dönüşümü al
    if (layer->anim_state && layer->anim_state->current_clip && layer->anim_state->is_playing) {
        fe_animation_clip_t* clip = layer->anim_state->current_clip;
        int* channel_index_ptr = (int*)fe_hash_map_get(&clip->bone_channel_map, bone->name.data);

        if (channel_index_ptr) {
            fe_animation_bone_channel_t* channel = (fe_animation_bone_channel_t*)fe_array_get_at(&clip->bone_channels, *channel_index_ptr);

            fe_vec3 animated_pos;
            fe_quat animated_rot;
            fe_vec3 animated_scale;

            get_interpolated_bone_transform(channel, animation_time, &animated_pos, &animated_rot, &animated_scale);

            fe_mat4 mat_pos = fe_mat4_translate(FE_MAT4_IDENTITY, animated_pos);
            fe_mat4 mat_rot = fe_quat_to_mat4(animated_rot);
            fe_mat4 mat_scale = fe_mat4_scale(FE_MAT4_IDENTITY, animated_scale);
            
            animated_transform = fe_mat4_mul(mat_rot, mat_scale);
            animated_transform = fe_mat4_mul(mat_pos, animated_transform);
        } else {
            // Eğer kemik için animasyon kanalı yoksa, bind pose'daki yerel dönüşümünü kullan
            animated_transform = bone->local_transform;
        }
    } else {
        // Eğer katmanda aktif bir animasyon yoksa, kemiğin yerel dönüşümünü bind pose'dan al
        animated_transform = bone->local_transform;
    }

    return animated_transform;
}

// --- Animasyon Kontrolcüsü Uygulamaları ---

fe_anim_controller_t* fe_anim_controller_create(fe_skeleton_t* skeleton) {
    if (!skeleton) {
        FE_LOG_ERROR("Cannot create animation controller without a skeleton.");
        return NULL;
    }

    fe_anim_controller_t* controller = FE_MALLOC(sizeof(fe_anim_controller_t), FE_MEM_TYPE_ANIMATION_CONTROLLER);
    if (!controller) {
        FE_LOG_CRITICAL("Failed to allocate memory for animation controller.");
        return NULL;
    }

    controller->skeleton = skeleton;
    controller->is_initialized = false;
    controller->current_transition = NULL;
    controller->transition_from_clip = NULL;

    fe_hash_map_init(&controller->registered_clips, sizeof(fe_animation_clip_t*), 8, FE_HASH_MAP_STRING_KEY, FE_MEM_TYPE_ANIMATION_CONTROLLER, __FILE__, __LINE__);

    // Animasyon katmanlarını başlat
    for (int i = 0; i < FE_ANIM_LAYER_COUNT; ++i) {
        controller->layers[i].anim_state = fe_animation_state_create();
        if (!controller->layers[i].anim_state) {
            FE_LOG_CRITICAL("Failed to create animation state for layer %d.", i);
            // Kısmi temizleme ve hata dönüşü
            fe_anim_controller_destroy(controller); // Bu fonksiyon kendi başına bir temizlik yapar
            return NULL;
        }
        controller->layers[i].weight = (i == FE_ANIM_LAYER_BASE) ? 1.0f : 0.0f; // Varsayılan olarak sadece Base katmanı aktif
        controller->layers[i].blend_mode = FE_ANIM_BLEND_OVERRIDE; // Şu an sadece override destekleniyor
        fe_string_init(&controller->layers[i].affected_bone_root, "");
        controller->layers[i].use_partial_mask = false;
    }

    controller->is_initialized = true;
    FE_LOG_INFO("Animation controller created for skeleton '%s'.", skeleton->name.data);
    return controller;
}

void fe_anim_controller_destroy(fe_anim_controller_t* controller) {
    if (!controller) return;

    if (controller->current_transition) {
        FE_FREE(controller->current_transition, FE_MEM_TYPE_ANIMATION_CONTROLLER);
        controller->current_transition = NULL;
    }

    for (int i = 0; i < FE_ANIM_LAYER_COUNT; ++i) {
        if (controller->layers[i].anim_state) {
            fe_animation_state_destroy(controller->layers[i].anim_state);
            controller->layers[i].anim_state = NULL;
        }
        fe_string_destroy(&controller->layers[i].affected_bone_root);
    }
    
    fe_hash_map_destroy(&controller->registered_clips);
    FE_FREE(controller, FE_MEM_TYPE_ANIMATION_CONTROLLER);
    FE_LOG_DEBUG("Animation controller destroyed.");
}

fe_anim_controller_error_t fe_anim_controller_register_clip(fe_anim_controller_t* controller, fe_animation_clip_t* clip) {
    if (!controller || !clip || !clip->name.data) {
        FE_LOG_ERROR("Invalid arguments for registering animation clip.");
        return FE_ANIM_CONTROLLER_INVALID_ARGUMENT;
    }
    if (!controller->is_initialized) return FE_ANIM_CONTROLLER_NOT_INITIALIZED;

    if (fe_hash_map_insert(&controller->registered_clips, clip->name.data, &clip)) {
        FE_LOG_DEBUG("Registered animation clip '%s'.", clip->name.data);
        return FE_ANIM_CONTROLLER_SUCCESS;
    }
    FE_LOG_ERROR("Failed to register animation clip '%s'.", clip->name.data);
    return FE_ANIM_CONTROLLER_UNKNOWN_ERROR;
}

fe_anim_controller_error_t fe_anim_controller_unregister_clip(fe_anim_controller_t* controller, const char* clip_name) {
    if (!controller || !clip_name) {
        FE_LOG_ERROR("Invalid arguments for unregistering animation clip.");
        return FE_ANIM_CONTROLLER_INVALID_ARGUMENT;
    }
    if (!controller->is_initialized) return FE_ANIM_CONTROLLER_NOT_INITIALIZED;

    if (fe_hash_map_remove(&controller->registered_clips, clip_name)) {
        FE_LOG_DEBUG("Unregistered animation clip '%s'.", clip_name);
        return FE_ANIM_CONTROLLER_SUCCESS;
    }
    FE_LOG_WARN("Animation clip '%s' not found for unregistering.", clip_name);
    return FE_ANIM_CONTROLLER_ANIM_NOT_FOUND;
}

fe_anim_controller_error_t fe_anim_controller_play(fe_anim_controller_t* controller, const char* clip_name, fe_anim_layer_priority_t layer, fe_animation_loop_mode_t loop_mode, float playback_speed) {
    if (!controller || !clip_name || layer >= FE_ANIM_LAYER_COUNT || playback_speed <= 0.0f) {
        FE_LOG_ERROR("Invalid arguments for fe_anim_controller_play.");
        return FE_ANIM_CONTROLLER_INVALID_ARGUMENT;
    }
    if (!controller->is_initialized) return FE_ANIM_CONTROLLER_NOT_INITIALIZED;

    fe_animation_clip_t** clip_ptr = (fe_animation_clip_t**)fe_hash_map_get(&controller->registered_clips, clip_name);
    if (!clip_ptr || !*clip_ptr) {
        FE_LOG_ERROR("Animation clip '%s' not found.", clip_name);
        return FE_ANIM_CONTROLLER_ANIM_NOT_FOUND;
    }

    // Eğer aynı katmanda aktif bir geçiş varsa, iptal et
    if (controller->current_transition && controller->current_transition->layer == layer) {
        FE_LOG_WARN("Overriding active transition on layer %d.", layer);
        FE_FREE(controller->current_transition, FE_MEM_TYPE_ANIMATION_CONTROLLER);
        controller->current_transition = NULL;
        controller->transition_from_clip = NULL;
    }

    fe_animation_error_t anim_err = fe_animation_state_play(controller->layers[layer].anim_state, *clip_ptr, playback_speed, loop_mode);
    if (anim_err != FE_ANIMATION_SUCCESS) {
        FE_LOG_ERROR("Failed to play animation '%s' on layer %d: %d", clip_name, layer, anim_err);
        return FE_ANIM_CONTROLLER_UNKNOWN_ERROR; // fe_animation_error_t'yi fe_anim_controller_error_t'ye dönüştür
    }

    controller->layers[layer].weight = 1.0f; // Anında oynatılırken ağırlığı 1.0 yap
    FE_LOG_INFO("Played animation '%s' on layer %d.", clip_name, layer);
    return FE_ANIM_CONTROLLER_SUCCESS;
}

fe_anim_controller_error_t fe_anim_controller_crossfade(fe_anim_controller_t* controller, const char* target_clip_name, float transition_duration, fe_anim_layer_priority_t layer, fe_animation_loop_mode_t loop_mode, float playback_speed) {
    if (!controller || !target_clip_name || transition_duration < 0.0f || layer >= FE_ANIM_LAYER_COUNT || playback_speed <= 0.0f) {
        FE_LOG_ERROR("Invalid arguments for fe_anim_controller_crossfade.");
        return FE_ANIM_CONTROLLER_INVALID_ARGUMENT;
    }
    if (!controller->is_initialized) return FE_ANIM_CONTROLLER_NOT_INITIALIZED;

    fe_animation_clip_t** target_clip_ptr = (fe_animation_clip_t**)fe_hash_map_get(&controller->registered_clips, target_clip_name);
    if (!target_clip_ptr || !*target_clip_ptr) {
        FE_LOG_ERROR("Target animation clip '%s' not found for crossfade.", target_clip_name);
        return FE_ANIM_CONTROLLER_ANIM_NOT_FOUND;
    }

    // Mevcut bir geçiş varsa iptal et
    if (controller->current_transition && controller->current_transition->layer == layer) {
        FE_LOG_WARN("Cancelling existing crossfade on layer %d for new one.", layer);
        FE_FREE(controller->current_transition, FE_MEM_TYPE_ANIMATION_CONTROLLER);
        controller->current_transition = NULL;
        controller->transition_from_clip = NULL;
    }

    // Geçiş yapacağı klibi kaydet (eğer bir şey çalıyorsa)
    controller->transition_from_clip = controller->layers[layer].anim_state->current_clip;

    // Yeni geçiş parametrelerini oluştur
    fe_anim_transition_params_t* new_transition = FE_MALLOC(sizeof(fe_anim_transition_params_t), FE_MEM_TYPE_ANIMATION_CONTROLLER);
    if (!new_transition) {
        FE_LOG_CRITICAL("Failed to allocate memory for animation transition parameters.");
        return FE_ANIM_CONTROLLER_OUT_OF_MEMORY;
    }
    new_transition->target_clip = *target_clip_ptr;
    new_transition->transition_duration = transition_duration;
    new_transition->elapsed_time = 0.0f;
    new_transition->layer = layer;
    new_transition->loop_mode = loop_mode;
    new_transition->playback_speed = playback_speed;

    controller->current_transition = new_transition;

    // Hedef animasyonu layer'da başlat ama ağırlığı 0.0 yap
    fe_animation_state_play(controller->layers[layer].anim_state, new_transition->target_clip, new_transition->playback_speed, new_transition->loop_mode);
    controller->layers[layer].weight = 0.0f; // Geçiş başlangıcında hedef klip görünmez olmalı

    FE_LOG_INFO("Started crossfade from '%s' to '%s' on layer %d for %.2f seconds.",
                controller->transition_from_clip ? controller->transition_from_clip->name.data : "NULL",
                target_clip_name, layer, transition_duration);
    return FE_ANIM_CONTROLLER_SUCCESS;
}


fe_anim_controller_error_t fe_anim_controller_pause(fe_anim_controller_t* controller, fe_anim_layer_priority_t layer) {
    if (!controller || layer >= FE_ANIM_LAYER_COUNT) {
        return FE_ANIM_CONTROLLER_INVALID_ARGUMENT;
    }
    if (!controller->is_initialized) return FE_ANIM_CONTROLLER_NOT_INITIALIZED;

    fe_animation_error_t anim_err = fe_animation_state_pause(controller->layers[layer].anim_state);
    if (anim_err == FE_ANIMATION_SUCCESS || anim_err == FE_ANIMATION_INVALID_STATE) { // Invalid state ise zaten oynatmıyor demektir, uyarı veririz.
        FE_LOG_INFO("Paused animation on layer %d.", layer);
        return FE_ANIM_CONTROLLER_SUCCESS;
    }
    FE_LOG_ERROR("Failed to pause animation on layer %d: %d", layer, anim_err);
    return FE_ANIM_CONTROLLER_UNKNOWN_ERROR;
}

fe_anim_controller_error_t fe_anim_controller_resume(fe_anim_controller_t* controller, fe_anim_layer_priority_t layer) {
    if (!controller || layer >= FE_ANIM_LAYER_COUNT) {
        return FE_ANIM_CONTROLLER_INVALID_ARGUMENT;
    }
    if (!controller->is_initialized) return FE_ANIM_CONTROLLER_NOT_INITIALIZED;

    fe_animation_error_t anim_err = fe_animation_state_resume(controller->layers[layer].anim_state);
    if (anim_err == FE_ANIMATION_SUCCESS || anim_err == FE_ANIMATION_INVALID_STATE) { // Invalid state ise zaten oynamıyor demektir, uyarı veririz.
        FE_LOG_INFO("Resumed animation on layer %d.", layer);
        return FE_ANIM_CONTROLLER_SUCCESS;
    }
    FE_LOG_ERROR("Failed to resume animation on layer %d: %d", layer, anim_err);
    return FE_ANIM_CONTROLLER_UNKNOWN_ERROR;
}

fe_anim_controller_error_t fe_anim_controller_stop(fe_anim_controller_t* controller, fe_anim_layer_priority_t layer) {
    if (!controller || layer >= FE_ANIM_LAYER_COUNT) {
        return FE_ANIM_CONTROLLER_INVALID_ARGUMENT;
    }
    if (!controller->is_initialized) return FE_ANIM_CONTROLLER_NOT_INITIALIZED;

    fe_animation_error_t anim_err = fe_animation_state_stop(controller->layers[layer].anim_state);
    if (anim_err == FE_ANIMATION_SUCCESS || anim_err == FE_ANIMATION_INVALID_STATE) {
        FE_LOG_INFO("Stopped animation on layer %d.", layer);
        return FE_ANIM_CONTROLLER_SUCCESS;
    }
    FE_LOG_ERROR("Failed to stop animation on layer %d: %d", layer, anim_err);
    return FE_ANIM_CONTROLLER_UNKNOWN_ERROR;
}

fe_anim_controller_error_t fe_anim_controller_set_layer_weight(fe_anim_controller_t* controller, fe_anim_layer_priority_t layer, float weight) {
    if (!controller || layer >= FE_ANIM_LAYER_COUNT || weight < 0.0f || weight > 1.0f) {
        FE_LOG_ERROR("Invalid arguments for setting layer weight.");
        return FE_ANIM_CONTROLLER_INVALID_ARGUMENT;
    }
    if (!controller->is_initialized) return FE_ANIM_CONTROLLER_NOT_INITIALIZED;

    controller->layers[layer].weight = fe_clampf(weight, 0.0f, 1.0f);
    FE_LOG_DEBUG("Layer %d weight set to %.2f.", layer, controller->layers[layer].weight);
    return FE_ANIM_CONTROLLER_SUCCESS;
}

fe_anim_controller_error_t fe_anim_controller_set_layer_partial_mask(fe_anim_controller_t* controller, fe_anim_layer_priority_t layer, bool use_mask, const char* bone_root) {
    if (!controller || layer >= FE_ANIM_LAYER_COUNT) {
        FE_LOG_ERROR("Invalid arguments for setting layer partial mask.");
        return FE_ANIM_CONTROLLER_INVALID_ARGUMENT;
    }
    if (!controller->is_initialized) return FE_ANIM_CONTROLLER_NOT_INITIALIZED;

    controller->layers[layer].use_partial_mask = use_mask;
    if (use_mask && bone_root) {
        fe_string_set(&controller->layers[layer].affected_bone_root, bone_root);
        FE_LOG_DEBUG("Layer %d partial mask enabled with root bone '%s'.", layer, bone_root);
    } else {
        fe_string_set(&controller->layers[layer].affected_bone_root, ""); // Kök kemik yok
        FE_LOG_DEBUG("Layer %d partial mask %s.", layer, use_mask ? "enabled (no specific root)" : "disabled");
    }
    return FE_ANIM_CONTROLLER_SUCCESS;
}


fe_anim_controller_error_t fe_anim_controller_update(fe_anim_controller_t* controller, float delta_time) {
    if (!controller || !controller->is_initialized) {
        return FE_ANIM_CONTROLLER_NOT_INITIALIZED;
    }

    // --- Geçişleri Yönet ---
    if (controller->current_transition) {
        controller->current_transition->elapsed_time += delta_time;
        float progress = controller->current_transition->elapsed_time / controller->current_transition->transition_duration;
        progress = fe_clampf(progress, 0.0f, 1.0f); // 0.0 ile 1.0 arasına sıkıştır

        fe_anim_layer_t* target_layer = &controller->layers[controller->current_transition->layer];

        // Hedef klibin ağırlığını artır (geçişin ilerlemesiyle)
        target_layer->weight = progress;

        if (progress >= 1.0f) {
            // Geçiş tamamlandı
            FE_LOG_INFO("Crossfade completed on layer %d. Target clip '%s' is now fully active.",
                        target_layer->anim_state->current_clip->name.data);
            
            // Eğer geçişten önceki klip vardıysa ve artık kullanılmıyorsa, durdur
            if (controller->transition_from_clip) {
                // Burada `fe_animation_state_stop` çağrılabilir veya klip sıfırlanabilir.
                // Basitçe: önceki klibi bırak ve artık onu karıştırma
            }

            FE_FREE(controller->current_transition, FE_MEM_TYPE_ANIMATION_CONTROLLER);
            controller->current_transition = NULL;
            controller->transition_from_clip = NULL;
            target_layer->weight = 1.0f; // Tam ağırlık
        }
    }

    // --- Katmanları Güncelle ve Kemik Dönüşümlerini Karıştır ---
    fe_mat4 temp_bone_local_transforms[fe_array_get_size(&controller->skeleton->bones)]; // Her kemiğin katmanlar arası karıştırılmış yerel dönüşümü

    // Tüm kemikler için yerel dönüşümleri hesapla
    for (size_t bone_idx = 0; bone_idx < fe_array_get_size(&controller->skeleton->bones); ++bone_idx) {
        temp_bone_local_transforms[bone_idx] = FE_MAT4_IDENTITY; // Başlangıçta kimlik matrisi

        // Her bir katmandaki animasyonu güncelle ve dönüşümleri karıştır
        fe_mat4 base_layer_transform = FE_MAT4_IDENTITY; // En düşük öncelikli katmanın dönüşümü
        bool base_layer_found = false;

        for (int i = 0; i < FE_ANIM_LAYER_COUNT; ++i) {
            fe_anim_layer_t* layer = &controller->layers[i];
            
            // Animasyon durumunu güncelle (her katman kendi zamanını ilerletir)
            fe_animation_state_update(layer->anim_state, controller->skeleton, delta_time);

            // Layer'dan bu kemik için animasyonlu yerel dönüşümü al
            fe_mat4 current_layer_bone_transform = get_animated_local_bone_transform_for_layer(
                layer, controller->skeleton, bone_idx, layer->anim_state->current_time);

            if (layer->weight > 0.0f) {
                if (i == FE_ANIM_LAYER_BASE) {
                    base_layer_transform = current_layer_bone_transform;
                    base_layer_found = true;
                } else if (layer->blend_mode == FE_ANIM_BLEND_OVERRIDE) {
                    // Üst katmanlarda override modunda ise, ağırlığına göre karıştır
                    // Eğer current_transition aktifse ve bu hedef katman ise
                    if (controller->current_transition && controller->current_transition->layer == i) {
                        // Geçişin "başlangıç" klibinden gelen transformu al
                        fe_mat4 from_clip_transform = FE_MAT4_IDENTITY;
                        if (controller->transition_from_clip) {
                            int* channel_index_ptr = (int*)fe_hash_map_get(&controller->transition_from_clip->bone_channel_map, 
                                ((fe_skeleton_bone_t*)fe_array_get_at(&controller->skeleton->bones, bone_idx))->name.data);
                            if (channel_index_ptr) {
                                fe_animation_bone_channel_t* channel = (fe_animation_bone_channel_t*)fe_array_get_at(
                                    &controller->transition_from_clip->bone_channels, *channel_index_ptr);
                                fe_vec3 pos, scale; fe_quat rot;
                                get_interpolated_bone_transform(channel, controller->layers[i].anim_state->current_time, &pos, &rot, &scale);
                                fe_mat4 mat_pos = fe_mat4_translate(FE_MAT4_IDENTITY, pos);
                                fe_mat4 mat_rot = fe_quat_to_mat4(rot);
                                fe_mat4 mat_scale = fe_mat4_scale(FE_MAT4_IDENTITY, scale);
                                from_clip_transform = fe_mat4_mul(mat_rot, mat_scale);
                                from_clip_transform = fe_mat4_mul(mat_pos, from_clip_transform);
                            } else {
                                from_clip_transform = ((fe_skeleton_bone_t*)fe_array_get_at(&controller->skeleton->bones, bone_idx))->local_transform;
                            }
                        } else {
                            from_clip_transform = ((fe_skeleton_bone_t*)fe_array_get_at(&controller->skeleton->bones, bone_idx))->local_transform; // Geçiş başlangıcında bir şey çalmadıysa
                        }
                        
                        // Geçiş faktörüne göre karıştır
                        temp_bone_local_transforms[bone_idx] = fe_mat4_lerp(from_clip_transform, current_layer_bone_transform, layer->weight);

                    } else {
                        // Normalde üst katmanlar, alt katmanların üzerine bind pose'u uygular
                        // Ancak katman ağırlığı ile karıştırıyoruz.
                        // (1-weight) * alt_katman_transform + weight * bu_katman_transform
                        // Şimdilik sadece basit override, alt katmanın üzerine yaz
                        // Karmaşık karıştırma için matris interpolasyonu gereklidir.
                        // Örneğin: fe_mat4_blend(base_layer_transform, current_layer_bone_transform, layer->weight);
                        
                        // En basit karıştırma: Yüksek ağırlıklı katman kazanır
                        // Daha doğru bir yaklaşım için:
                        // fe_mat4 mixed_transform = fe_mat4_lerp(base_layer_transform, current_layer_bone_transform, layer->weight);
                        // temp_bone_local_transforms[bone_idx] = mixed_transform;
                        // Veya sadece override:
                        // temp_bone_local_transforms[bone_idx] = current_layer_bone_transform;

                        // Geçiş yoksa ve ağırlığı 1.0 ise, doğrudan kullan
                        if (fe_is_nearly_equal(layer->weight, 1.0f, 0.001f)) {
                             temp_bone_local_transforms[bone_idx] = current_layer_bone_transform;
                        } else {
                            // Ağırlıklı karıştırma (LERP)
                            // Bu, konum, rotasyon ve ölçek için ayrı ayrı yapılmalıdır,
                            // ancak basitlik için doğrudan matris LERP kullanıyoruz.
                            // Not: Matris LERP her zaman en iyi sonucu vermez, özellikle rotasyonlar için.
                            // Ayrı ayrı vec3 ve quat lerp yapıp sonra matris oluşturmak daha iyidir.
                            temp_bone_local_transforms[bone_idx] = fe_mat4_lerp(base_layer_transform, current_layer_bone_transform, layer->weight);
                        }
                    }
                }
                // Additive mod gelecekte eklenecek.
            }

            // Temel katmanın matrisini diğer katmanlar tarafından kullanılmak üzere sakla
            if (i == FE_ANIM_LAYER_BASE && base_layer_found) {
                temp_bone_local_transforms[bone_idx] = base_layer_transform;
            }
        }
    }


    // --- Global Dönüşümleri Hesapla ve Nihai Matrisleri Belirle ---
    fe_mat4 global_bone_transforms[fe_array_get_size(&controller->skeleton->bones)];

    for (size_t i = 0; i < fe_array_get_size(&controller->skeleton->bones); ++i) {
        fe_skeleton_bone_t* bone = (fe_skeleton_bone_t*)fe_array_get_at(&controller->skeleton->bones, i);

        // Kemiğin yerel animasyon dönüşümü (katmanlar arası karıştırılmış veya varsayılan)
        fe_mat4 bone_local_anim_transform;
        if (temp_bone_local_transforms[i].m[0][0] == FE_MAT4_IDENTITY.m[0][0]) { // Eğer karıştırma yapılmadıysa
             // Tüm katmanlarda aktif bir animasyon veya yeterli ağırlık yoksa, bind pose kullan
            bone_local_anim_transform = bone->local_transform;
        } else {
            bone_local_anim_transform = temp_bone_local_transforms[i];
        }


        if (bone->parent_index == -1) {
            global_bone_transforms[i] = bone_local_anim_transform;
        } else {
            global_bone_transforms[i] = fe_mat4_mul(global_bone_transforms[bone->parent_index], bone_local_anim_transform);
        }
        
        // Nihai dönüşüm = Global_Anim_Transform * Inverse_Bind_Transform
        bone->final_transform = fe_mat4_mul(global_bone_transforms[i], bone->inverse_bind_transform);
    }

    return FE_ANIM_CONTROLLER_SUCCESS;
}
