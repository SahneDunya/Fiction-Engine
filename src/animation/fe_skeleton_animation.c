#include "animation/fe_skeleton_animation.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h"
#include "core/math/fe_math.h" // fe_vec3_lerp, fe_quat_slerp, fe_mat4_mul, fe_mat4_translate, fe_mat4_rotate, fe_mat4_scale vb. için

// --- Dahili Yardımcı Fonksiyonlar (İnterpolasyon) ---

/**
 * @brief Verilen zamana göre iki anahtar kare arasındaki interpolasyon faktörünü hesaplar.
 *
 * @param last_keyframe_time Önceki anahtar karenin zamanı.
 * @param next_keyframe_time Sonraki anahtar karenin zamanı.
 * @param animation_time Mevcut animasyon zamanı.
 * @return float İnterpolasyon faktörü (0.0 - 1.0 arası).
 */
static float calculate_interpolation_factor(float last_keyframe_time, float next_keyframe_time, float animation_time) {
    float time_diff = next_keyframe_time - last_keyframe_time;
    if (time_diff == 0.0f) {
        return 0.0f; // Aynı zamanda iki anahtar kare varsa, interpolasyona gerek yok
    }
    return (animation_time - last_keyframe_time) / time_diff;
}

/**
 * @brief Belirli bir zaman için anahtar kareleri arar ve önceki/sonraki anahtar karelerin indekslerini döndürür.
 *
 * @param keyframes Anahtar kare dizisi (fe_array_t of fe_animation_keyframe_t).
 * @param animation_time Mevcut animasyon zamanı.
 * @param out_prev_index Önceki anahtar karenin indeksi.
 * @param out_next_index Sonraki anahtar karenin indeksi.
 * @return fe_animation_error_t Başarı durumunu döner (FE_ANIMATION_NOT_FOUND eğer yoksa).
 */
static fe_animation_error_t find_keyframe_indices(const fe_array_t* keyframes, float animation_time, int* out_prev_index, int* out_next_index) {
    if (fe_array_get_size(keyframes) == 0) {
        return FE_ANIMATION_NOT_FOUND;
    }

    *out_prev_index = 0;
    *out_next_index = 0;

    if (fe_array_get_size(keyframes) == 1) {
        // Tek anahtar kare varsa, her ikisi de aynı kareye işaret eder
        return FE_ANIMATION_SUCCESS;
    }

    for (size_t i = 0; i < fe_array_get_size(keyframes) - 1; ++i) {
        fe_animation_keyframe_t* kf1 = (fe_animation_keyframe_t*)fe_array_get_at(keyframes, i);
        fe_animation_keyframe_t* kf2 = (fe_animation_keyframe_t*)fe_array_get_at(keyframes, i + 1);

        if (animation_time >= kf1->time && animation_time <= kf2->time) {
            *out_prev_index = i;
            *out_next_index = i + 1;
            return FE_ANIMATION_SUCCESS;
        }
    }
    // Eğer animasyon zamanı son anahtar karenin ötesindeyse, son anahtar kareyi döndür
    // Bu, döngülü animasyonlar için veya animasyonun sonunda durmak için önemlidir.
    *out_prev_index = fe_array_get_size(keyframes) - 1;
    *out_next_index = fe_array_get_size(keyframes) - 1;
    return FE_ANIMATION_SUCCESS;
}

/**
 * @brief Bir animasyon klibi için mevcut zamandaki kemik dönüşümünü hesaplar.
 *
 * @param bone_channel Kemiğin anahtar kare kanalı.
 * @param animation_time Mevcut animasyon zamanı.
 * @param out_position Hesaplanan konum.
 * @param out_rotation Hesaplanan rotasyon (Quaternion).
 * @param out_scale Hesaplanan ölçek.
 */
static void get_interpolated_bone_transform(
    const fe_animation_bone_channel_t* bone_channel,
    float animation_time,
    fe_vec3* out_position,
    fe_quat* out_rotation,
    fe_vec3* out_scale)
{
    // Konum interpolasyonu
    int prev_pos_idx, next_pos_idx;
    if (find_keyframe_indices(&bone_channel->position_keyframes, animation_time, &prev_pos_idx, &next_pos_idx) == FE_ANIMATION_SUCCESS) {
        fe_animation_keyframe_t* prev_kf = (fe_animation_keyframe_t*)fe_array_get_at(&bone_channel->position_keyframes, prev_pos_idx);
        fe_animation_keyframe_t* next_kf = (fe_animation_keyframe_t*)fe_array_get_at(&bone_channel->position_keyframes, next_pos_idx);

        if (prev_pos_idx == next_pos_idx) {
            *out_position = prev_kf->position;
        } else {
            float factor = calculate_interpolation_factor(prev_kf->time, next_kf->time, animation_time);
            *out_position = fe_vec3_lerp(prev_kf->position, next_kf->position, factor);
        }
    } else {
        *out_position = FE_VEC3_ZERO; // Varsayılan değer
    }

    // Rotasyon interpolasyonu (SLERP)
    int prev_rot_idx, next_rot_idx;
    if (find_keyframe_indices(&bone_channel->rotation_keyframes, animation_time, &prev_rot_idx, &next_rot_idx) == FE_ANIMATION_SUCCESS) {
        fe_animation_keyframe_t* prev_kf = (fe_animation_keyframe_t*)fe_array_get_at(&bone_channel->rotation_keyframes, prev_rot_idx);
        fe_animation_keyframe_t* next_kf = (fe_animation_keyframe_t*)fe_array_get_at(&bone_channel->rotation_keyframes, next_rot_idx);

        if (prev_rot_idx == next_rot_idx) {
            *out_rotation = prev_kf->rotation;
        } else {
            float factor = calculate_interpolation_factor(prev_kf->time, next_kf->time, animation_time);
            *out_rotation = fe_quat_slerp(prev_kf->rotation, next_kf->rotation, factor);
        }
    } else {
        *out_rotation = FE_QUAT_IDENTITY; // Varsayılan değer
    }

    // Ölçek interpolasyonu
    int prev_scale_idx, next_scale_idx;
    if (find_keyframe_indices(&bone_channel->scale_keyframes, animation_time, &prev_scale_idx, &next_scale_idx) == FE_ANIMATION_SUCCESS) {
        fe_animation_keyframe_t* prev_kf = (fe_animation_keyframe_t*)fe_array_get_at(&bone_channel->scale_keyframes, prev_scale_idx);
        fe_animation_keyframe_t* next_kf = (fe_animation_keyframe_t*)fe_array_get_at(&bone_channel->scale_keyframes, next_scale_idx);

        if (prev_scale_idx == next_scale_idx) {
            *out_scale = prev_kf->scale;
        } else {
            float factor = calculate_interpolation_factor(prev_kf->time, next_kf->time, animation_time);
            *out_scale = fe_vec3_lerp(prev_kf->scale, next_kf->scale, factor);
        }
    } else {
        *out_scale = FE_VEC3_ONE; // Varsayılan değer
    }
}


// --- Animasyon Klibi Uygulamaları ---

fe_animation_clip_t* fe_animation_clip_create(const char* name, float duration, float ticks_per_second) {
    if (!name || duration < 0.0f || ticks_per_second <= 0.0f) {
        FE_LOG_ERROR("Invalid arguments for animation clip creation.");
        return NULL;
    }

    fe_animation_clip_t* clip = FE_MALLOC(sizeof(fe_animation_clip_t), FE_MEM_TYPE_ANIMATION);
    if (!clip) {
        FE_LOG_CRITICAL("Failed to allocate memory for animation clip.");
        return NULL;
    }

    fe_string_init(&clip->name, name);
    clip->duration = duration;
    clip->ticks_per_second = ticks_per_second;
    fe_array_init(&clip->bone_channels, sizeof(fe_animation_bone_channel_t), 4, FE_MEM_TYPE_ANIMATION, __FILE__, __LINE__);
    fe_hash_map_init(&clip->bone_channel_map, sizeof(int), 4, FE_HASH_MAP_STRING_KEY, FE_MEM_TYPE_ANIMATION, __FILE__, __LINE__);

    FE_LOG_DEBUG("Animation clip '%s' created (Duration: %.2f, Ticks/Sec: %.2f).", name, duration, ticks_per_second);
    return clip;
}

fe_animation_bone_channel_t* fe_animation_clip_add_bone_channel(fe_animation_clip_t* clip, const char* bone_name) {
    if (!clip || !bone_name) {
        FE_LOG_ERROR("Invalid arguments for adding bone channel to animation clip.");
        return NULL;
    }

    int* existing_index = (int*)fe_hash_map_get(&clip->bone_channel_map, bone_name);
    if (existing_index) {
        FE_LOG_WARN("Bone channel '%s' already exists in animation clip '%s'. Returning existing.", bone_name, clip->name.data);
        return (fe_animation_bone_channel_t*)fe_array_get_at(&clip->bone_channels, *existing_index);
    }

    fe_animation_bone_channel_t new_channel;
    fe_string_init(&new_channel.bone_name, bone_name);
    fe_array_init(&new_channel.position_keyframes, sizeof(fe_animation_keyframe_t), 8, FE_MEM_TYPE_ANIMATION, __FILE__, __LINE__);
    fe_array_init(&new_channel.rotation_keyframes, sizeof(fe_animation_keyframe_t), 8, FE_MEM_TYPE_ANIMATION, __FILE__, __LINE__);
    fe_array_init(&new_channel.scale_keyframes, sizeof(fe_animation_keyframe_t), 8, FE_MEM_TYPE_ANIMATION, __FILE__, __LINE__);

    int index = fe_array_add(&clip->bone_channels, &new_channel);
    if (index == -1) {
        FE_LOG_CRITICAL("Failed to add bone channel to animation clip array.");
        fe_string_destroy(&new_channel.bone_name);
        fe_array_destroy(&new_channel.position_keyframes);
        fe_array_destroy(&new_channel.rotation_keyframes);
        fe_array_destroy(&new_channel.scale_keyframes);
        return NULL;
    }

    if (!fe_hash_map_insert(&clip->bone_channel_map, bone_name, &index)) {
        FE_LOG_CRITICAL("Failed to insert bone channel index into hash map for clip '%s'.", clip->name.data);
        // Array'dan geri alma veya hata yönetimi burada daha karmaşık olabilir
        // Basitçe burada bırakıyoruz ve bellek kaçağını kabul ediyoruz, veya array'den elementi kaldırmayı deneyebiliriz.
        return NULL;
    }

    fe_animation_bone_channel_t* added_channel = (fe_animation_bone_channel_t*)fe_array_get_at(&clip->bone_channels, index);
    FE_LOG_DEBUG("Added bone channel '%s' to clip '%s'.", bone_name, clip->name.data);
    return added_channel;
}

fe_animation_error_t fe_animation_bone_channel_add_keyframe(fe_array_t* keyframes, float time, fe_vec3 pos, fe_quat rot, fe_vec3 scale) {
    if (!keyframes || time < 0.0f) {
        return FE_ANIMATION_INVALID_ARGUMENT;
    }
    fe_animation_keyframe_t kf = { .time = time, .position = pos, .rotation = rot, .scale = scale };
    if (fe_array_add(keyframes, &kf) == -1) {
        return FE_ANIMATION_OUT_OF_MEMORY;
    }
    return FE_ANIMATION_SUCCESS;
}

fe_animation_error_t fe_animation_bone_channel_add_position_keyframe(fe_animation_bone_channel_t* channel, float time, fe_vec3 pos) {
    if (!channel) return FE_ANIMATION_INVALID_ARGUMENT;
    return fe_animation_bone_channel_add_keyframe(&channel->position_keyframes, time, pos, FE_QUAT_IDENTITY, FE_VEC3_ONE);
}

fe_animation_error_t fe_animation_bone_channel_add_rotation_keyframe(fe_animation_bone_channel_t* channel, float time, fe_quat rot) {
    if (!channel) return FE_ANIMATION_INVALID_ARGUMENT;
    return fe_animation_bone_channel_add_keyframe(&channel->rotation_keyframes, time, FE_VEC3_ZERO, rot, FE_VEC3_ONE);
}

fe_animation_error_t fe_animation_bone_channel_add_scale_keyframe(fe_animation_bone_channel_t* channel, float time, fe_vec3 scale) {
    if (!channel) return FE_ANIMATION_INVALID_ARGUMENT;
    return fe_animation_bone_channel_add_keyframe(&channel->scale_keyframes, time, FE_VEC3_ZERO, FE_QUAT_IDENTITY, scale);
}

void fe_animation_clip_destroy(fe_animation_clip_t* clip) {
    if (!clip) return;

    for (size_t i = 0; i < fe_array_get_size(&clip->bone_channels); ++i) {
        fe_animation_bone_channel_t* channel = (fe_animation_bone_channel_t*)fe_array_get_at(&clip->bone_channels, i);
        fe_string_destroy(&channel->bone_name);
        fe_array_destroy(&channel->position_keyframes);
        fe_array_destroy(&channel->rotation_keyframes);
        fe_array_destroy(&channel->scale_keyframes);
    }
    fe_array_destroy(&clip->bone_channels);
    fe_hash_map_destroy(&clip->bone_channel_map);
    fe_string_destroy(&clip->name);
    FE_FREE(clip, FE_MEM_TYPE_ANIMATION);
    FE_LOG_DEBUG("Animation clip destroyed.");
}

// --- İskelet Uygulamaları ---

fe_skeleton_t* fe_skeleton_create(const char* name) {
    if (!name) {
        FE_LOG_ERROR("Invalid name for skeleton creation.");
        return NULL;
    }
    fe_skeleton_t* skeleton = FE_MALLOC(sizeof(fe_skeleton_t), FE_MEM_TYPE_ANIMATION);
    if (!skeleton) {
        FE_LOG_CRITICAL("Failed to allocate memory for skeleton.");
        return NULL;
    }

    fe_string_init(&skeleton->name, name);
    fe_array_init(&skeleton->bones, sizeof(fe_skeleton_bone_t), 16, FE_MEM_TYPE_ANIMATION, __FILE__, __LINE__);
    fe_hash_map_init(&skeleton->bone_map, sizeof(int), 16, FE_HASH_MAP_STRING_KEY, FE_MEM_TYPE_ANIMATION, __FILE__, __LINE__);

    FE_LOG_DEBUG("Skeleton '%s' created.", name);
    return skeleton;
}

fe_skeleton_bone_t* fe_skeleton_add_bone(fe_skeleton_t* skeleton, const char* name, int parent_index, fe_mat4 local_transform, fe_mat4 inverse_bind_transform) {
    if (!skeleton || !name || parent_index >= (int)fe_array_get_size(&skeleton->bones) || (parent_index != -1 && parent_index < 0)) {
        FE_LOG_ERROR("Invalid arguments for adding bone to skeleton.");
        return NULL;
    }

    int* existing_index = (int*)fe_hash_map_get(&skeleton->bone_map, name);
    if (existing_index) {
        FE_LOG_WARN("Bone '%s' already exists in skeleton '%s'. Returning existing.", name, skeleton->name.data);
        return (fe_skeleton_bone_t*)fe_array_get_at(&skeleton->bones, *existing_index);
    }

    fe_skeleton_bone_t new_bone;
    fe_string_init(&new_bone.name, name);
    new_bone.parent_index = parent_index;
    new_bone.local_transform = local_transform;
    new_bone.inverse_bind_transform = inverse_bind_transform;
    new_bone.final_transform = FE_MAT4_IDENTITY; // Başlangıçta kimlik matrisi

    int index = fe_array_add(&skeleton->bones, &new_bone);
    if (index == -1) {
        FE_LOG_CRITICAL("Failed to add bone '%s' to skeleton array.", name);
        fe_string_destroy(&new_bone.name);
        return NULL;
    }

    if (!fe_hash_map_insert(&skeleton->bone_map, name, &index)) {
        FE_LOG_CRITICAL("Failed to insert bone '%s' index into hash map for skeleton '%s'.", name, skeleton->name.data);
        // Hata yönetimi
        return NULL;
    }

    fe_skeleton_bone_t* added_bone = (fe_skeleton_bone_t*)fe_array_get_at(&skeleton->bones, index);
    FE_LOG_DEBUG("Added bone '%s' to skeleton '%s' at index %d.", name, skeleton->name.data, index);
    return added_bone;
}

void fe_skeleton_destroy(fe_skeleton_t* skeleton) {
    if (!skeleton) return;

    for (size_t i = 0; i < fe_array_get_size(&skeleton->bones); ++i) {
        fe_skeleton_bone_t* bone = (fe_skeleton_bone_t*)fe_array_get_at(&skeleton->bones, i);
        fe_string_destroy(&bone->name);
    }
    fe_array_destroy(&skeleton->bones);
    fe_hash_map_destroy(&skeleton->bone_map);
    fe_string_destroy(&skeleton->name);
    FE_FREE(skeleton, FE_MEM_TYPE_ANIMATION);
    FE_LOG_DEBUG("Skeleton destroyed.");
}

const fe_mat4* fe_skeleton_get_final_bone_transform(const fe_skeleton_t* skeleton, int bone_index) {
    if (!skeleton || bone_index < 0 || bone_index >= (int)fe_array_get_size(&skeleton->bones)) {
        FE_LOG_ERROR("Invalid skeleton or bone index %d.", bone_index);
        return NULL;
    }
    fe_skeleton_bone_t* bone = (fe_skeleton_bone_t*)fe_array_get_at(&skeleton->bones, bone_index);
    return &bone->final_transform;
}


// --- Animasyon Durumu Uygulamaları ---

fe_animation_state_t* fe_animation_state_create() {
    fe_animation_state_t* state = FE_MALLOC(sizeof(fe_animation_state_t), FE_MEM_TYPE_ANIMATION);
    if (!state) {
        FE_LOG_CRITICAL("Failed to allocate memory for animation state.");
        return NULL;
    }
    state->current_clip = NULL;
    state->current_time = 0.0f;
    state->playback_speed = 1.0f;
    state->loop_mode = FE_ANIM_LOOP_NONE;
    state->is_playing = false;

    FE_LOG_DEBUG("Animation state created.");
    return state;
}

void fe_animation_state_destroy(fe_animation_state_t* state) {
    if (!state) return;
    FE_FREE(state, FE_MEM_TYPE_ANIMATION);
    FE_LOG_DEBUG("Animation state destroyed.");
}

fe_animation_error_t fe_animation_state_play(fe_animation_state_t* state, fe_animation_clip_t* clip, float playback_speed, fe_animation_loop_mode_t loop_mode) {
    if (!state || !clip || playback_speed <= 0.0f) {
        FE_LOG_ERROR("Invalid arguments for playing animation.");
        return FE_ANIMATION_INVALID_ARGUMENT;
    }

    state->current_clip = clip;
    state->current_time = 0.0f; // Animasyonu baştan başlat
    state->playback_speed = playback_speed;
    state->loop_mode = loop_mode;
    state->is_playing = true;

    FE_LOG_INFO("Playing animation clip '%s' (Speed: %.2f, Loop: %s).",
                clip->name.data, playback_speed, loop_mode == FE_ANIM_LOOP_REPEAT ? "Repeat" : "None");
    return FE_ANIMATION_SUCCESS;
}

fe_animation_error_t fe_animation_state_pause(fe_animation_state_t* state) {
    if (!state) return FE_ANIMATION_INVALID_ARGUMENT;
    if (!state->is_playing) {
        FE_LOG_WARN("Animation is not playing, cannot pause.");
        return FE_ANIMATION_INVALID_STATE;
    }
    state->is_playing = false;
    FE_LOG_INFO("Animation paused.");
    return FE_ANIMATION_SUCCESS;
}

fe_animation_error_t fe_animation_state_resume(fe_animation_state_t* state) {
    if (!state) return FE_ANIMATION_INVALID_ARGUMENT;
    if (state->is_playing) {
        FE_LOG_WARN("Animation is already playing, cannot resume.");
        return FE_ANIMATION_INVALID_STATE;
    }
    if (!state->current_clip) {
        FE_LOG_ERROR("No animation clip set to resume.");
        return FE_ANIMATION_INVALID_STATE;
    }
    state->is_playing = true;
    FE_LOG_INFO("Animation resumed.");
    return FE_ANIMATION_SUCCESS;
}

fe_animation_error_t fe_animation_state_stop(fe_animation_state_t* state) {
    if (!state) return FE_ANIMATION_INVALID_ARGUMENT;
    if (!state->current_clip) {
        FE_LOG_WARN("No animation clip set to stop.");
        return FE_ANIMATION_INVALID_STATE;
    }
    state->is_playing = false;
    state->current_time = 0.0f;
    state->current_clip = NULL; // Klibi sıfırla
    FE_LOG_INFO("Animation stopped.");
    return FE_ANIMATION_SUCCESS;
}


fe_animation_error_t fe_animation_state_update(fe_animation_state_t* state, fe_skeleton_t* skeleton, float delta_time) {
    if (!state || !skeleton) {
        FE_LOG_ERROR("Invalid arguments for animation state update.");
        return FE_ANIMATION_INVALID_ARGUMENT;
    }
    if (!state->is_playing || !state->current_clip) {
        return FE_ANIMATION_INVALID_STATE; // Animasyon çalmıyorsa veya klip yoksa güncelleme yapma
    }

    // Animasyon zamanını güncelle
    state->current_time += delta_time * state->playback_speed;

    // Animasyon süresinin dışına çıkarsa döngü moduna göre ayarla
    float animation_total_ticks = state->current_clip->duration * state->current_clip->ticks_per_second;
    float current_animation_ticks = state->current_time * state->current_clip->ticks_per_second;

    if (current_animation_ticks >= animation_total_ticks) {
        if (state->loop_mode == FE_ANIM_LOOP_REPEAT) {
            // Döngüye al: zamanı sıfırla veya modulo al
            state->current_time = fmodf(state->current_time, state->current_clip->duration);
            // FE_LOG_DEBUG("Animation '%s' looped. Current time: %.3f", state->current_clip->name.data, state->current_time);
        } else {
            // Döngü yoksa durdur ve son karede kal
            state->current_time = state->current_clip->duration; // Son kareye sabitle
            state->is_playing = false;
            FE_LOG_INFO("Animation '%s' finished playing (no loop).", state->current_clip->name.data);
            return FE_ANIMATION_SUCCESS; // Bitince başarılı say
        }
    }

    // Her bir kemik için dönüşüm matrislerini hesapla
    fe_mat4 global_bone_transforms[fe_array_get_size(&skeleton->bones)]; // Geçici global dönüşümler

    for (size_t i = 0; i < fe_array_get_size(&skeleton->bones); ++i) {
        fe_skeleton_bone_t* bone = (fe_skeleton_bone_t*)fe_array_get_at(&skeleton->bones, i);

        fe_mat4 bone_local_anim_transform = FE_MAT4_IDENTITY;

        // Kemiğin animasyon kanalını bul
        int* channel_index_ptr = (int*)fe_hash_map_get(&state->current_clip->bone_channel_map, bone->name.data);
        if (channel_index_ptr) {
            fe_animation_bone_channel_t* channel = (fe_animation_bone_channel_t*)fe_array_get_at(&state->current_clip->bone_channels, *channel_index_ptr);

            fe_vec3 animated_pos;
            fe_quat animated_rot;
            fe_vec3 animated_scale;

            // Interpolasyon ile o anki transformu al
            get_interpolated_bone_transform(channel, state->current_time, &animated_pos, &animated_rot, &animated_scale);

            // Yerel animasyon dönüşüm matrisini oluştur
            fe_mat4 mat_pos = fe_mat4_translate(FE_MAT4_IDENTITY, animated_pos);
            fe_mat4 mat_rot = fe_quat_to_mat4(animated_rot);
            fe_mat4 mat_scale = fe_mat4_scale(FE_MAT4_IDENTITY, animated_scale);
            
            // Dönüşüm sırası: Scale -> Rotate -> Translate
            bone_local_anim_transform = fe_mat4_mul(mat_rot, mat_scale); // Scale'i rotasyonla çarp
            bone_local_anim_transform = fe_mat4_mul(mat_pos, bone_local_anim_transform); // Sonucu konumla çarp
            
        } else {
            // Eğer kemik için animasyon kanalı yoksa, bind pose'daki yerel dönüşümünü kullan
            bone_local_anim_transform = bone->local_transform;
        }

        // Global dönüşümü hesapla
        if (bone->parent_index == -1) {
            // Kök kemik: global dönüşümü yerel animasyon dönüşümüdür
            global_bone_transforms[i] = bone_local_anim_transform;
        } else {
            // Çocuk kemik: ebeveynin global dönüşümü ile kendi yerel animasyon dönüşümünü çarp
            // Sıra: Ebeveyn_Global * Kendi_Yerel
            global_bone_transforms[i] = fe_mat4_mul(global_bone_transforms[bone->parent_index], bone_local_anim_transform);
        }

        // Nihai dönüşüm matrisini hesapla
        // Nihai matris = Global_Dönüşüm * Ters_Bağlama_Matrisi
        bone->final_transform = fe_mat4_mul(global_bone_transforms[i], bone->inverse_bind_transform);
    }

    return FE_ANIMATION_SUCCESS;
}
