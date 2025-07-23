#include "audio/fe_sound_mixer.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h" // Bellek yönetimi için

// SDL kütüphanesini dahil et (SDL_mixer'ın SDL'e bağımlılığı için)
#include <SDL.h>

// --- Ses Karıştırıcı Durumu (Singleton) ---
typedef struct fe_sound_mixer_state {
    bool is_initialized;
    int max_channels; // SDL_mixer tarafından yönetilen maksimum kanal sayısı
} fe_sound_mixer_state_t;

static fe_sound_mixer_state_t g_sound_mixer_state;


// --- Fonksiyon Uygulamaları ---

fe_sound_mixer_error_t fe_sound_mixer_init(int frequency, int channels, int chunk_size, int max_mixer_channels) {
    if (g_sound_mixer_state.is_initialized) {
        FE_LOG_WARN("Sound mixer already initialized.");
        return FE_SOUND_MIXER_ALREADY_INITIALIZED;
    }

    // SDL audio alt sistemini başlat
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        FE_LOG_CRITICAL("SDL audio could not initialize! SDL Error: %s", SDL_GetError());
        return FE_SOUND_MIXER_SDL_ERROR;
    }

    // SDL_mixer'ı başlat (OGG ve MP3 desteğiyle)
    int mix_flags = MIX_INIT_OGG | MIX_INIT_MP3; 
    int init_result = Mix_Init(mix_flags);
    if ((init_result & mix_flags) != mix_flags) {
        FE_LOG_CRITICAL("SDL_mixer could not initialize! SDL_mixer Error: %s", Mix_GetError());
        SDL_Quit(); // SDL audio'yu kapat
        return FE_SOUND_MIXER_SDL_ERROR;
    }

    // Sesi aç (audio device'ı aç)
    if (Mix_OpenAudio(frequency, MIX_DEFAULT_FORMAT, channels, chunk_size) < 0) {
        FE_LOG_CRITICAL("SDL_mixer could not open audio! SDL_mixer Error: %s", Mix_GetError());
        Mix_Quit(); // SDL_mixer'ı kapat
        SDL_Quit(); // SDL audio'yu kapat
        return FE_SOUND_MIXER_SDL_ERROR;
    }

    g_sound_mixer_state.is_initialized = true;

    // Kanal sayısını ayarla
    g_sound_mixer_state.max_channels = Mix_AllocateChannels(max_mixer_channels);
    if (g_sound_mixer_state.max_channels < 0) {
        FE_LOG_WARN("Could not allocate mixer channels. Using default. SDL_mixer Error: %s", Mix_GetError());
        // Mix_AllocateChannels -1 döndürürse ve hata mesajı yoksa, varsayılan sayıda kanal kullanılıyor demektir.
        // Bu durumda mevcut kanalların sayısını kontrol etmek en iyisidir.
        g_sound_mixer_state.max_channels = Mix_AllocateChannels(-1); // Tekrar dene, mevcut kanalları al
        if (g_sound_mixer_state.max_channels < 0) { // Hala hata varsa
            g_sound_mixer_state.max_channels = 8; // Güvenli varsayılan
            FE_LOG_ERROR("Failed to determine max mixer channels, falling back to 8.");
        }
    }


    FE_LOG_INFO("Sound mixer initialized (Freq: %d, Channels: %d, Chunk: %d, Max Mixer Channels: %d)",
                frequency, channels, chunk_size, g_sound_mixer_state.max_channels);
    return FE_SOUND_MIXER_SUCCESS;
}

void fe_sound_mixer_shutdown() {
    if (!g_sound_mixer_state.is_initialized) {
        FE_LOG_WARN("Sound mixer not initialized. Nothing to shutdown.");
        return;
    }

    // Tüm kanallardaki sesleri durdur
    Mix_HaltChannel(-1);
    // Çalmakta olan müziği durdur
    Mix_HaltMusic();

    // SDL_mixer ve SDL audio alt sistemlerini kapat
    Mix_CloseAudio();
    Mix_Quit();
    SDL_Quit();

    g_sound_mixer_state.is_initialized = false;
    FE_LOG_INFO("Sound mixer shutdown complete.");
}

fe_sound_mixer_error_t fe_sound_mixer_load_chunk(const fe_string_t* file_path, fe_mixer_chunk_t* out_chunk) {
    if (!g_sound_mixer_state.is_initialized) return FE_SOUND_MIXER_NOT_INITIALIZED;
    if (!file_path || fe_string_is_empty(file_path) || !out_chunk) {
        FE_LOG_ERROR("Invalid argument for loading chunk: file_path or out_chunk is NULL/empty.");
        return FE_SOUND_MIXER_INVALID_ARGUMENT;
    }

    out_chunk->sdl_chunk = Mix_LoadWAV(file_path->data);
    if (!out_chunk->sdl_chunk) {
        FE_LOG_ERROR("Failed to load WAV file '%s'. SDL_mixer Error: %s", file_path->data, Mix_GetError());
        return FE_SOUND_MIXER_LOAD_ERROR;
    }
    FE_LOG_DEBUG("Loaded chunk from '%s'.", file_path->data);
    return FE_SOUND_MIXER_SUCCESS;
}

void fe_sound_mixer_free_chunk(fe_mixer_chunk_t* chunk) {
    if (!g_sound_mixer_state.is_initialized) {
        FE_LOG_WARN("Sound mixer not initialized. Cannot free chunk.");
        return;
    }
    if (chunk && chunk->sdl_chunk) {
        Mix_FreeChunk(chunk->sdl_chunk);
        chunk->sdl_chunk = NULL;
        FE_LOG_DEBUG("Chunk freed.");
    }
}

fe_sound_mixer_error_t fe_sound_mixer_load_music(const fe_string_t* file_path, fe_mixer_music_t* out_music) {
    if (!g_sound_mixer_state.is_initialized) return FE_SOUND_MIXER_NOT_INITIALIZED;
    if (!file_path || fe_string_is_empty(file_path) || !out_music) {
        FE_LOG_ERROR("Invalid argument for loading music: file_path or out_music is NULL/empty.");
        return FE_SOUND_MIXER_INVALID_ARGUMENT;
    }

    out_music->sdl_music = Mix_LoadMUS(file_path->data);
    if (!out_music->sdl_music) {
        FE_LOG_ERROR("Failed to load music file '%s'. SDL_mixer Error: %s", file_path->data, Mix_GetError());
        return FE_SOUND_MIXER_LOAD_ERROR;
    }
    FE_LOG_DEBUG("Loaded music from '%s'.", file_path->data);
    return FE_SOUND_MIXER_SUCCESS;
}

void fe_sound_mixer_free_music(fe_mixer_music_t* music) {
    if (!g_sound_mixer_state.is_initialized) {
        FE_LOG_WARN("Sound mixer not initialized. Cannot free music.");
        return;
    }
    if (music && music->sdl_music) {
        Mix_FreeMusic(music->sdl_music);
        music->sdl_music = NULL;
        FE_LOG_DEBUG("Music freed.");
    }
}

int fe_sound_mixer_play_chunk(const fe_mixer_chunk_t* chunk, int loops, int channel_hint) {
    if (!g_sound_mixer_state.is_initialized) {
        FE_LOG_ERROR("Sound mixer not initialized. Cannot play chunk.");
        return -1;
    }
    if (!chunk || !chunk->sdl_chunk) {
        FE_LOG_ERROR("Invalid chunk for playing.");
        return -1;
    }

    int channel = Mix_PlayChannel(channel_hint, chunk->sdl_chunk, loops);
    if (channel == -1) {
        FE_LOG_ERROR("Failed to play chunk. SDL_mixer Error: %s", Mix_GetError());
        return -1;
    }
    FE_LOG_DEBUG("Chunk played on channel %d.", channel);
    return channel;
}

void fe_sound_mixer_stop_channel(int channel) {
    if (!g_sound_mixer_state.is_initialized) {
        FE_LOG_WARN("Sound mixer not initialized. Cannot stop channel.");
        return;
    }
    if (channel < -1 || channel >= g_sound_mixer_state.max_channels) {
        FE_LOG_WARN("Invalid channel %d for stopping.", channel);
        return;
    }
    Mix_HaltChannel(channel);
    FE_LOG_DEBUG("Channel %d stopped.", channel);
}

fe_sound_mixer_error_t fe_sound_mixer_play_music(const fe_mixer_music_t* music, int loops) {
    if (!g_sound_mixer_state.is_initialized) return FE_SOUND_MIXER_NOT_INITIALIZED;
    if (!music || !music->sdl_music) {
        FE_LOG_ERROR("Invalid music for playing.");
        return FE_SOUND_MIXER_INVALID_ARGUMENT;
    }

    // Daha önce çalmakta olan müziği durdur (isteğe bağlı, genellikle istenir)
    if (Mix_PlayingMusic() || Mix_PausedMusic()) {
        Mix_HaltMusic();
    }

    if (Mix_PlayMusic(music->sdl_music, loops) == -1) {
        FE_LOG_ERROR("Failed to play music. SDL_mixer Error: %s", Mix_GetError());
        return FE_SOUND_MIXER_PLAY_ERROR;
    }
    FE_LOG_DEBUG("Music started playing. Loops: %d", loops);
    return FE_SOUND_MIXER_SUCCESS;
}

fe_sound_mixer_error_t fe_sound_mixer_pause_music() {
    if (!g_sound_mixer_state.is_initialized) return FE_SOUND_MIXER_NOT_INITIALIZED;
    if (Mix_PlayingMusic() && !Mix_PausedMusic()) { // Sadece çalıyorsa ve duraklatılmamışsa duraklat
        Mix_PauseMusic();
        FE_LOG_DEBUG("Music paused.");
    } else {
        FE_LOG_DEBUG("Music is not playing or already paused.");
    }
    return FE_SOUND_MIXER_SUCCESS;
}

fe_sound_mixer_error_t fe_sound_mixer_resume_music() {
    if (!g_sound_mixer_state.is_initialized) return FE_SOUND_MIXER_NOT_INITIALIZED;
    if (Mix_PausedMusic()) { // Sadece duraklatılmışsa devam ettir
        Mix_ResumeMusic();
        FE_LOG_DEBUG("Music resumed.");
    } else {
        FE_LOG_DEBUG("Music is not paused.");
    }
    return FE_SOUND_MIXER_SUCCESS;
}

fe_sound_mixer_error_t fe_sound_mixer_halt_music() {
    if (!g_sound_mixer_state.is_initialized) return FE_SOUND_MIXER_NOT_INITIALIZED;
    if (Mix_PlayingMusic() || Mix_PausedMusic()) { // Çalıyorsa veya duraklatılmışsa durdur
        Mix_HaltMusic();
        FE_LOG_DEBUG("Music halted.");
    } else {
        FE_LOG_DEBUG("No music playing or paused to halt.");
    }
    return FE_SOUND_MIXER_SUCCESS;
}

void fe_sound_mixer_set_channel_volume(int channel, int volume) {
    if (!g_sound_mixer_state.is_initialized) {
        FE_LOG_WARN("Sound mixer not initialized. Cannot set channel volume.");
        return;
    }
    Mix_Volume(channel, volume);
    FE_LOG_DEBUG("Channel %d volume set to %d.", channel, volume);
}

void fe_sound_mixer_set_music_volume(int volume) {
    if (!g_sound_mixer_state.is_initialized) {
        FE_LOG_WARN("Sound mixer not initialized. Cannot set music volume.");
        return;
    }
    Mix_VolumeMusic(volume);
    FE_LOG_DEBUG("Music volume set to %d.", volume);
}

fe_sound_mixer_error_t fe_sound_mixer_set_channel_panning(int channel, uint8_t left_pan, uint8_t right_pan) {
    if (!g_sound_mixer_state.is_initialized) return FE_SOUND_MIXER_NOT_INITIALIZED;
    if (channel < -1 || channel >= g_sound_mixer_state.max_channels) {
        FE_LOG_ERROR("Invalid channel %d for panning.", channel);
        return FE_SOUND_MIXER_CHANNEL_ERROR;
    }
    if (Mix_SetPanning(channel, left_pan, right_pan) == -1) {
        FE_LOG_ERROR("Failed to set panning for channel %d. SDL_mixer Error: %s", channel, Mix_GetError());
        return FE_SOUND_MIXER_SDL_ERROR;
    }
    FE_LOG_DEBUG("Channel %d panning set to Left: %u, Right: %u.", channel, left_pan, right_pan);
    return FE_SOUND_MIXER_SUCCESS;
}

int fe_sound_mixer_get_max_channels() {
    if (!g_sound_mixer_state.is_initialized) {
        FE_LOG_WARN("Sound mixer not initialized. Returning 0 max channels.");
        return 0;
    }
    return g_sound_mixer_state.max_channels;
}
