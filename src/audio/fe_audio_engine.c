#include "audio/fe_audio_engine.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h"
#include "core/math/fe_math.h" // fe_vec3_normalize, fe_vec3_dist, fe_rad_to_deg için

// SDL_mixer kütüphanesini dahil et
#include <SDL.h>
#include <SDL_mixer.h>

// --- Dahili Yapılar (SDL_mixer referansları için) ---

// Yüklenmiş ses efektleri için hash map'i
typedef struct fe_loaded_sound {
    Mix_Chunk* chunk; // SDL_mixer ses parçası
    // Ek metadata eklenebilir
} fe_loaded_sound_t;

// Yüklenmiş müzik parçaları için hash map'i
typedef struct fe_loaded_music {
    Mix_Music* music; // SDL_mixer müzik parçası
    // Ek metadata eklenebilir
} fe_loaded_music_t;

// --- Ses Motoru Durumu (Singleton) ---
typedef struct fe_audio_engine_state {
    bool is_initialized;
    fe_hash_map_t loaded_sounds; // fe_sound_id_t -> fe_loaded_sound_t*
    fe_hash_map_t loaded_music;  // fe_music_id_t -> fe_loaded_music_t*
    
    fe_audio_listener_t listener; // Tekil dinleyici

    float master_volume; // Ana ses seviyesi (0.0 - 1.0)
    float music_volume;  // Müzik ses seviyesi (0.0 - 1.0)
    float sound_volume;  // Efekt ses seviyesi (0.0 - 1.0)

    int max_channels; // SDL_mixer tarafından yönetilen maksimum kanal sayısı
} fe_audio_engine_state_t;

static fe_audio_engine_state_t g_audio_engine_state;

// --- Dahili Yardımcı Fonksiyonlar ---

// SDL_mixer ses seviyesini 0-128 aralığına dönüştürür.
static int fe_audio_mixer_volume_to_sdl(float volume) {
    return (int)(volume * MIX_MAX_VOLUME);
}

// SDL_mixer ses seviyesini 0.0-1.0 aralığına dönüştürür.
static float fe_audio_sdl_volume_to_mixer(int volume_sdl) {
    return (float)volume_sdl / MIX_MAX_VOLUME;
}

// SDL_mixer panlama değerini 0-255 aralığına dönüştürür.
// SDL_mixer'da 128 ortadır.
static uint8_t fe_audio_mixer_pan_to_sdl(float pan) {
    return (uint8_t)(pan * 255.0f);
}

// Uzamsal ses için pan ve mesafeye dayalı ses seviyesi hesaplaması
static void fe_audio_calculate_spatial_params(
    const fe_vec3* emitter_pos,
    const fe_audio_listener_t* listener,
    float min_dist, float max_dist,
    float base_volume,
    int* out_volume_sdl,
    uint8_t* out_left_pan,
    uint8_t* out_right_pan)
{
    fe_vec3 relative_pos = fe_vec3_sub(*emitter_pos, listener->position);
    float distance = fe_vec3_length(relative_pos);

    // Mesafe tabanlı ses düşüşü
    float attenuation = 1.0f;
    if (distance > max_dist) {
        attenuation = 0.0f;
    } else if (distance > min_dist) {
        attenuation = 1.0f - ((distance - min_dist) / (max_dist - min_dist));
    }
    attenuation = FE_CLAMP(attenuation, 0.0f, 1.0f);

    float final_volume = base_volume * attenuation;
    *out_volume_sdl = fe_audio_mixer_volume_to_sdl(final_volume);

    // Panlama (basit stereo panlama)
    // Sesin dinleyicinin solunda mı sağında mı olduğunu belirle
    fe_vec3 listener_right_vec = fe_vec3_cross(listener->forward_vector, listener->up_vector);
    fe_vec3_normalize(&listener_right_vec); // Sağ vektörü normalize et

    float dot_product = fe_vec3_dot(relative_pos, listener_right_vec);

    // Panlama aralığı: -1.0 (tam sol) ile 1.0 (tam sağ) arası.
    // Dot product'ı sınırlayarak aşırı değerleri önle
    float pan_factor = FE_CLAMP(dot_product / max_dist, -1.0f, 1.0f);

    // SDL_mixer'ın Mix_SetPanning fonksiyonu 0-255 arasında sol ve sağ kanallar için iki değer alır.
    // 255 sol kanal, 0 sağ kanal (yani ses tam solda)
    // 0 sol kanal, 255 sağ kanal (yani ses tam sağda)
    // 128, 128 ise ortada
    // Bu, kulağa biraz terstir, "sol" ve "sağ" değerleri sol ve sağ hoparlöre ne kadar gideceğini belirtir.
    
    // Basit panlama:
    // pan_factor -1 ise tam sol (yani sol hoparlör 255, sağ 0)
    // pan_factor 1 ise tam sağ (yani sol hoparlör 0, sağ 255)
    // pan_factor 0 ise orta (yani sol hoparlör 128, sağ 128)

    // Pan faktörü [-1, 1] aralığında
    // Sol kulaklık seviyesi: (1 - pan_factor) / 2
    // Sağ kulaklık seviyesi: (1 + pan_factor) / 2

    float left_level = (1.0f - pan_factor) / 2.0f;
    float right_level = (1.0f + pan_factor) / 2.0f;

    *out_left_pan = (uint8_t)(left_level * 255.0f);
    *out_right_pan = (uint8_t)(right_level * 255.0f);

    // Sadece debug için
    // FE_LOG_DEBUG("Emitter Dist: %.2f, Attenuation: %.2f, Pan Factor: %.2f, Left: %u, Right: %u", 
    //              distance, attenuation, pan_factor, *out_left_pan, *out_right_pan);
}


// --- Fonksiyon Uygulamaları ---

fe_audio_error_t fe_audio_engine_init(int frequency, int channels, int chunk_size) {
    if (g_audio_engine_state.is_initialized) {
        FE_LOG_WARN("Audio engine already initialized.");
        return FE_AUDIO_ALREADY_INITIALIZED;
    }

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        FE_LOG_CRITICAL("SDL audio could not initialize! SDL Error: %s", SDL_GetError());
        return FE_AUDIO_MIXER_ERROR;
    }

    // SDL_mixer'ı başlat
    // MIX_INIT_OGG ve MIX_INIT_MP3 gibi flag'leri yüklemek istediğiniz formatlara göre ekleyin
    int mix_flags = MIX_INIT_OGG | MIX_INIT_MP3; 
    int init_result = Mix_Init(mix_flags);
    if ((init_result & mix_flags) != mix_flags) {
        FE_LOG_CRITICAL("SDL_mixer could not initialize! SDL_mixer Error: %s", Mix_GetError());
        SDL_Quit();
        return FE_AUDIO_MIXER_ERROR;
    }

    // Sesi aç (audio device'ı aç)
    if (Mix_OpenAudio(frequency, MIX_DEFAULT_FORMAT, channels, chunk_size) < 0) {
        FE_LOG_CRITICAL("SDL_mixer could not open audio! SDL_mixer Error: %s", Mix_GetError());
        Mix_Quit();
        SDL_Quit();
        return FE_AUDIO_MIXER_ERROR;
    }

    g_audio_engine_state.is_initialized = true;
    g_audio_engine_state.max_channels = Mix_AllocateChannels(-1); // Tüm mevcut kanalları tahsis et
    if (g_audio_engine_state.max_channels < 0) {
        FE_LOG_WARN("Could not allocate mixer channels. Using default. SDL_mixer Error: %s", Mix_GetError());
        g_audio_engine_state.max_channels = 8; // Varsayılan bir değer
    }

    fe_hash_map_init(&g_audio_engine_state.loaded_sounds, sizeof(fe_loaded_sound_t), 16, FE_HASH_MAP_STRING_KEY, FE_MEM_TYPE_AUDIO, __FILE__, __LINE__);
    fe_hash_map_init(&g_audio_engine_state.loaded_music, sizeof(fe_loaded_music_t), 4, FE_HASH_MAP_STRING_KEY, FE_MEM_TYPE_AUDIO, __FILE__, __LINE__);
    
    // Varsayılan ses seviyelerini ayarla
    fe_audio_engine_set_master_volume(1.0f);
    fe_audio_engine_set_music_volume(1.0f);
    fe_audio_engine_set_sound_volume(1.0f);

    // Dinleyiciyi varsayılan değerlerle başlat
    g_audio_engine_state.listener.position = FE_VEC3_ZERO;
    g_audio_engine_state.listener.forward_vector = FE_VEC3_FORWARD;
    g_audio_engine_state.listener.up_vector = FE_VEC3_UP;

    FE_LOG_INFO("Audio engine initialized (Freq: %d, Channels: %d, Chunk: %d, Max Mixer Channels: %d)",
                frequency, channels, chunk_size, g_audio_engine_state.max_channels);
    return FE_AUDIO_SUCCESS;
}

void fe_audio_engine_shutdown() {
    if (!g_audio_engine_state.is_initialized) {
        FE_LOG_WARN("Audio engine not initialized. Nothing to shutdown.");
        return;
    }

    // Yüklü tüm sesleri boşalt
    // Hash map'i iterasyonla dolaşmak için fe_hash_map_for_each kullanılabilir.
    fe_hash_map_iterator_t sound_it = fe_hash_map_begin(&g_audio_engine_state.loaded_sounds);
    while (fe_hash_map_iterator_is_valid(&sound_it)) {
        fe_loaded_sound_t* sound = (fe_loaded_sound_t*)fe_hash_map_iterator_get_value(&sound_it);
        if (sound && sound->chunk) {
            Mix_FreeChunk(sound->chunk);
            sound->chunk = NULL;
        }
        fe_hash_map_iterator_next(&sound_it);
    }
    fe_hash_map_destroy(&g_audio_engine_state.loaded_sounds);

    // Yüklü tüm müzikleri boşalt
    fe_hash_map_iterator_t music_it = fe_hash_map_begin(&g_audio_engine_state.loaded_music);
    while (fe_hash_map_iterator_is_valid(&music_it)) {
        fe_loaded_music_t* music = (fe_loaded_music_t*)fe_hash_map_iterator_get_value(&music_it);
        if (music && music->music) {
            Mix_FreeMusic(music->music);
            music->music = NULL;
        }
        fe_hash_map_iterator_next(&music_it);
    }
    fe_hash_map_destroy(&g_audio_engine_state.loaded_music);

    // SDL_mixer'ı kapat
    Mix_CloseAudio();
    Mix_Quit();
    SDL_Quit(); // SDL ses alt sistemini kapat

    g_audio_engine_state.is_initialized = false;
    FE_LOG_INFO("Audio engine shutdown complete.");
}

fe_audio_error_t fe_audio_engine_load_sound(fe_sound_id_t sound_id, const fe_string_t* file_path) {
    if (!g_audio_engine_state.is_initialized) return FE_AUDIO_NOT_INITIALIZED;
    if (fe_string_is_empty(&sound_id) || fe_string_is_empty(file_path)) {
        FE_LOG_ERROR("Sound ID or file path is empty for sound loading.");
        return FE_AUDIO_INVALID_ARGUMENT;
    }

    if (fe_hash_map_get(&g_audio_engine_state.loaded_sounds, sound_id.data) != NULL) {
        FE_LOG_WARN("Sound '%s' already loaded. Skipping.", sound_id.data);
        return FE_AUDIO_SUCCESS; // Zaten yüklüyse başarı dön
    }

    Mix_Chunk* chunk = Mix_LoadWAV(file_path->data);
    if (!chunk) {
        FE_LOG_ERROR("Failed to load sound '%s' from '%s'. SDL_mixer Error: %s", sound_id.data, file_path->data, Mix_GetError());
        return FE_AUDIO_LOAD_ERROR;
    }

    fe_loaded_sound_t loaded_sound;
    loaded_sound.chunk = chunk;

    if (!fe_hash_map_insert(&g_audio_engine_state.loaded_sounds, sound_id.data, &loaded_sound)) {
        FE_LOG_CRITICAL("Failed to insert loaded sound '%s' into hash map.", sound_id.data);
        Mix_FreeChunk(chunk); // Belleği serbest bırak
        return FE_AUDIO_OUT_OF_MEMORY;
    }

    FE_LOG_INFO("Loaded sound '%s' from '%s'.", sound_id.data, file_path->data);
    return FE_AUDIO_SUCCESS;
}

fe_audio_error_t fe_audio_engine_unload_sound(fe_sound_id_t sound_id) {
    if (!g_audio_engine_state.is_initialized) return FE_AUDIO_NOT_INITIALIZED;
    if (fe_string_is_empty(&sound_id)) {
        FE_LOG_ERROR("Sound ID is empty for sound unloading.");
        return FE_AUDIO_INVALID_ARGUMENT;
    }

    fe_loaded_sound_t* loaded_sound = (fe_loaded_sound_t*)fe_hash_map_get(&g_audio_engine_state.loaded_sounds, sound_id.data);
    if (!loaded_sound) {
        FE_LOG_WARN("Sound '%s' not found for unloading.", sound_id.data);
        return FE_AUDIO_SOUND_NOT_FOUND;
    }

    Mix_FreeChunk(loaded_sound->chunk);
    loaded_sound->chunk = NULL;

    if (!fe_hash_map_remove(&g_audio_engine_state.loaded_sounds, sound_id.data)) {
        FE_LOG_ERROR("Failed to remove sound '%s' from hash map.", sound_id.data);
        return FE_AUDIO_UNKNOWN_ERROR; // Veya daha spesifik bir hata
    }

    FE_LOG_INFO("Unloaded sound '%s'.", sound_id.data);
    return FE_AUDIO_SUCCESS;
}

fe_audio_error_t fe_audio_engine_load_music(fe_music_id_t music_id, const fe_string_t* file_path) {
    if (!g_audio_engine_state.is_initialized) return FE_AUDIO_NOT_INITIALIZED;
    if (fe_string_is_empty(&music_id) || fe_string_is_empty(file_path)) {
        FE_LOG_ERROR("Music ID or file path is empty for music loading.");
        return FE_AUDIO_INVALID_ARGUMENT;
    }

    if (fe_hash_map_get(&g_audio_engine_state.loaded_music, music_id.data) != NULL) {
        FE_LOG_WARN("Music '%s' already loaded. Skipping.", music_id.data);
        return FE_AUDIO_SUCCESS;
    }

    Mix_Music* music = Mix_LoadMUS(file_path->data);
    if (!music) {
        FE_LOG_ERROR("Failed to load music '%s' from '%s'. SDL_mixer Error: %s", music_id.data, file_path->data, Mix_GetError());
        return FE_AUDIO_LOAD_ERROR;
    }

    fe_loaded_music_t loaded_music;
    loaded_music.music = music;

    if (!fe_hash_map_insert(&g_audio_engine_state.loaded_music, music_id.data, &loaded_music)) {
        FE_LOG_CRITICAL("Failed to insert loaded music '%s' into hash map.", music_id.data);
        Mix_FreeMusic(music);
        return FE_AUDIO_OUT_OF_MEMORY;
    }

    FE_LOG_INFO("Loaded music '%s' from '%s'.", music_id.data, file_path->data);
    return FE_AUDIO_SUCCESS;
}

fe_audio_error_t fe_audio_engine_unload_music(fe_music_id_t music_id) {
    if (!g_audio_engine_state.is_initialized) return FE_AUDIO_NOT_INITIALIZED;
    if (fe_string_is_empty(&music_id)) {
        FE_LOG_ERROR("Music ID is empty for music unloading.");
        return FE_AUDIO_INVALID_ARGUMENT;
    }

    fe_loaded_music_t* loaded_music = (fe_loaded_music_t*)fe_hash_map_get(&g_audio_engine_state.loaded_music, music_id.data);
    if (!loaded_music) {
        FE_LOG_WARN("Music '%s' not found for unloading.", music_id.data);
        return FE_AUDIO_MUSIC_NOT_FOUND;
    }

    Mix_FreeMusic(loaded_music->music);
    loaded_music->music = NULL;

    if (!fe_hash_map_remove(&g_audio_engine_state.loaded_music, music_id.data)) {
        FE_LOG_ERROR("Failed to remove music '%s' from hash map.", music_id.data);
        return FE_AUDIO_UNKNOWN_ERROR;
    }

    FE_LOG_INFO("Unloaded music '%s'.", music_id.data);
    return FE_AUDIO_SUCCESS;
}

int fe_audio_engine_play_sound(fe_sound_id_t sound_id, int loops, int channel_hint, float volume, float pitch) {
    if (!g_audio_engine_state.is_initialized) {
        FE_LOG_ERROR("Audio engine not initialized. Cannot play sound.");
        return -1;
    }
    if (fe_string_is_empty(&sound_id)) {
        FE_LOG_ERROR("Sound ID is empty for playing sound.");
        return -1;
    }

    fe_loaded_sound_t* loaded_sound = (fe_loaded_sound_t*)fe_hash_map_get(&g_audio_engine_state.loaded_sounds, sound_id.data);
    if (!loaded_sound || !loaded_sound->chunk) {
        FE_LOG_ERROR("Sound '%s' not loaded or chunk is NULL. Cannot play.", sound_id.data);
        return -1;
    }

    int final_volume_sdl = fe_audio_mixer_volume_to_sdl(volume * g_audio_engine_state.sound_volume * g_audio_engine_state.master_volume);
    
    // Mix_PlayChannel'a -1 verirsek otomatik kanal seçer
    int channel = Mix_PlayChannel(channel_hint, loaded_sound->chunk, loops);
    if (channel == -1) {
        FE_LOG_ERROR("Failed to play sound '%s'. SDL_mixer Error: %s", sound_id.data, Mix_GetError());
        return -1;
    }

    Mix_Volume(channel, final_volume_sdl);

    // SDL_mixer doğrudan pitch değişimi desteklemez.
    // Pitch için ya sesi yeniden örneklemek ya da alternatif bir ses kütüphanesi kullanmak gerekir.
    // Burada pitch'i uygulamıyoruz, sadece API'de yer tutuyor.
    if (pitch != 1.0f) {
        FE_LOG_WARN("SDL_mixer does not directly support pitch adjustment. Pitch for sound '%s' will be ignored.", sound_id.data);
    }

    FE_LOG_DEBUG("Played sound '%s' on channel %d.", sound_id.data, channel);
    return channel;
}

void fe_audio_engine_stop_sound(int channel) {
    if (!g_audio_engine_state.is_initialized) {
        FE_LOG_WARN("Audio engine not initialized. Cannot stop sound.");
        return;
    }
    if (channel < 0 || channel >= g_audio_engine_state.max_channels) {
        FE_LOG_WARN("Invalid channel %d for stopping sound.", channel);
        return;
    }
    Mix_HaltChannel(channel);
    FE_LOG_DEBUG("Stopped sound on channel %d.", channel);
}

fe_audio_error_t fe_audio_engine_play_music(fe_music_id_t music_id, int loops) {
    if (!g_audio_engine_state.is_initialized) return FE_AUDIO_NOT_INITIALIZED;
    if (fe_string_is_empty(&music_id)) {
        FE_LOG_ERROR("Music ID is empty for playing music.");
        return FE_AUDIO_INVALID_ARGUMENT;
    }

    fe_loaded_music_t* loaded_music = (fe_loaded_music_t*)fe_hash_map_get(&g_audio_engine_state.loaded_music, music_id.data);
    if (!loaded_music || !loaded_music->music) {
        FE_LOG_ERROR("Music '%s' not loaded or chunk is NULL. Cannot play.", music_id.data);
        return FE_AUDIO_MUSIC_NOT_FOUND;
    }

    // Halihazırda müzik çalıyorsa durdur
    if (Mix_PlayingMusic()) {
        Mix_HaltMusic();
    }

    // Loops için SDL_mixer'da 0 tek sefer, -1 sonsuz döngü. Diğer pozitif sayılar o kadar tekrar.
    int mix_loops = loops;
    if (loops == 0) mix_loops = 0; // Tek seferlik
    else if (loops < 0) mix_loops = -1; // Sonsuz döngü

    if (Mix_PlayMusic(loaded_music->music, mix_loops) == -1) {
        FE_LOG_ERROR("Failed to play music '%s'. SDL_mixer Error: %s", music_id.data, Mix_GetError());
        return FE_AUDIO_PLAY_ERROR;
    }
    
    // O anki müzik ses seviyesini ayarla
    Mix_VolumeMusic(fe_audio_mixer_volume_to_sdl(g_audio_engine_state.music_volume * g_audio_engine_state.master_volume));

    FE_LOG_INFO("Played music '%s'. Loops: %d", music_id.data, loops);
    return FE_AUDIO_SUCCESS;
}

fe_audio_error_t fe_audio_engine_pause_music() {
    if (!g_audio_engine_state.is_initialized) return FE_AUDIO_NOT_INITIALIZED;
    if (Mix_PlayingMusic() && !Mix_PausedMusic()) {
        Mix_PauseMusic();
        FE_LOG_INFO("Music paused.");
    }
    return FE_AUDIO_SUCCESS;
}

fe_audio_error_t fe_audio_engine_resume_music() {
    if (!g_audio_engine_state.is_initialized) return FE_AUDIO_NOT_INITIALIZED;
    if (Mix_PausedMusic()) {
        Mix_ResumeMusic();
        FE_LOG_INFO("Music resumed.");
    }
    return FE_AUDIO_SUCCESS;
}

fe_audio_error_t fe_audio_engine_stop_music() {
    if (!g_audio_engine_state.is_initialized) return FE_AUDIO_NOT_INITIALIZED;
    if (Mix_PlayingMusic() || Mix_PausedMusic()) {
        Mix_HaltMusic();
        FE_LOG_INFO("Music stopped.");
    }
    return FE_AUDIO_SUCCESS;
}

void fe_audio_engine_set_master_volume(float volume) {
    if (!g_audio_engine_state.is_initialized) {
        FE_LOG_WARN("Audio engine not initialized. Cannot set master volume.");
        return;
    }
    g_audio_engine_state.master_volume = FE_CLAMP(volume, 0.0f, 1.0f);
    
    // Hem müzik hem de ses efektlerinin ses seviyelerini güncelleyin
    Mix_VolumeMusic(fe_audio_mixer_volume_to_sdl(g_audio_engine_state.music_volume * g_audio_engine_state.master_volume));
    Mix_Volume(-1, fe_audio_mixer_volume_to_sdl(g_audio_engine_state.sound_volume * g_audio_engine_state.master_volume));
    FE_LOG_DEBUG("Master volume set to %.2f.", g_audio_engine_state.master_volume);
}

float fe_audio_engine_get_master_volume() {
    return g_audio_engine_state.master_volume;
}

void fe_audio_engine_set_music_volume(float volume) {
    if (!g_audio_engine_state.is_initialized) {
        FE_LOG_WARN("Audio engine not initialized. Cannot set music volume.");
        return;
    }
    g_audio_engine_state.music_volume = FE_CLAMP(volume, 0.0f, 1.0f);
    Mix_VolumeMusic(fe_audio_mixer_volume_to_sdl(g_audio_engine_state.music_volume * g_audio_engine_state.master_volume));
    FE_LOG_DEBUG("Music volume set to %.2f.", g_audio_engine_state.music_volume);
}

void fe_audio_engine_set_sound_volume(float volume) {
    if (!g_audio_engine_state.is_initialized) {
        FE_LOG_WARN("Audio engine not initialized. Cannot set sound volume.");
        return;
    }
    g_audio_engine_state.sound_volume = FE_CLAMP(volume, 0.0f, 1.0f);
    // Tüm kanalların varsayılan ses seviyesini ayarla (şimdiki ve gelecek çalınacak sesler için)
    Mix_Volume(-1, fe_audio_mixer_volume_to_sdl(g_audio_engine_state.sound_volume * g_audio_engine_state.master_volume));
    FE_LOG_DEBUG("Sound effect volume set to %.2f.", g_audio_engine_state.sound_volume);
}

fe_audio_error_t fe_audio_engine_set_listener(const fe_audio_listener_t* listener) {
    if (!g_audio_engine_state.is_initialized) return FE_AUDIO_NOT_INITIALIZED;
    if (!listener) {
        FE_LOG_ERROR("Listener is NULL.");
        return FE_AUDIO_INVALID_ARGUMENT;
    }
    g_audio_engine_state.listener = *listener;
    fe_vec3_normalize(&g_audio_engine_state.listener.forward_vector);
    fe_vec3_normalize(&g_audio_engine_state.listener.up_vector);
    // FE_LOG_DEBUG("Listener position updated to (%.2f, %.2f, %.2f)", listener->position.x, listener->position.y, listener->position.z);
    return FE_AUDIO_SUCCESS;
}

int fe_audio_engine_play_spatial_sound(fe_sound_id_t sound_id, const fe_audio_emitter_t* emitter, int loops) {
    if (!g_audio_engine_state.is_initialized) {
        FE_LOG_ERROR("Audio engine not initialized. Cannot play spatial sound.");
        return -1;
    }
    if (fe_string_is_empty(&sound_id) || !emitter) {
        FE_LOG_ERROR("Sound ID is empty or emitter is NULL for playing spatial sound.");
        return -1;
    }

    fe_loaded_sound_t* loaded_sound = (fe_loaded_sound_t*)fe_hash_map_get(&g_audio_engine_state.loaded_sounds, sound_id.data);
    if (!loaded_sound || !loaded_sound->chunk) {
        FE_LOG_ERROR("Spatial sound '%s' not loaded or chunk is NULL. Cannot play.", sound_id.data);
        return -1;
    }

    int channel = Mix_PlayChannel(-1, loaded_sound->chunk, loops);
    if (channel == -1) {
        FE_LOG_ERROR("Failed to play spatial sound '%s'. SDL_mixer Error: %s", sound_id.data, Mix_GetError());
        return -1;
    }

    int final_volume_sdl;
    uint8_t left_pan, right_pan;

    fe_audio_calculate_spatial_params(
        &emitter->position,
        &g_audio_engine_state.listener,
        emitter->min_distance,
        emitter->max_distance,
        emitter->volume * g_audio_engine_state.sound_volume * g_audio_engine_state.master_volume,
        &final_volume_sdl,
        &left_pan,
        &right_pan
    );

    Mix_Volume(channel, final_volume_sdl);
    Mix_SetPanning(channel, left_pan, right_pan);

    // SDL_mixer mesafe tabanlı ses için Mix_SetDistance kullanır, ancak bu sadece mesafe içindir.
    // Pan ve volume için manuel hesaplama daha esneklik sağlar.
     Mix_SetDistance(channel, (uint8_t)(distance / max_dist * 255.0f)); // Mesafe 0 (yakın) -> 255 (uzak)

    FE_LOG_DEBUG("Played spatial sound '%s' on channel %d. Vol: %d, Pan L:%u R:%u", 
                 sound_id.data, channel, final_volume_sdl, left_pan, right_pan);
    return channel;
}
