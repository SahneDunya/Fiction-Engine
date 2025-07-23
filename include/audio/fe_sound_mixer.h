#ifndef FE_SOUND_MIXER_H
#define FE_SOUND_MIXER_H

#include "core/utils/fe_types.h" // Temel tipler için
#include "core/containers/fe_string.h" // Dosya yolları için

// SDL_mixer kütüphanesini dahil et (gerçek projede kütüphane kurulumu ve build ayarları gereklidir)
// Sadece Mix_Chunk ve Mix_Music gibi tipleri bileşene ifşa etmek için burada dahil ediyoruz.
#include <SDL_mixer.h>

// --- Ses Karıştırıcı Hata Kodları ---
typedef enum fe_sound_mixer_error {
    FE_SOUND_MIXER_SUCCESS = 0,
    FE_SOUND_MIXER_NOT_INITIALIZED,
    FE_SOUND_MIXER_ALREADY_INITIALIZED,
    FE_SOUND_MIXER_INVALID_ARGUMENT,
    FE_SOUND_MIXER_OUT_OF_MEMORY,
    FE_SOUND_MIXER_SDL_ERROR,    // Genel SDL/SDL_mixer hatası
    FE_SOUND_MIXER_LOAD_ERROR,   // Ses dosyası yüklenemedi
    FE_SOUND_MIXER_PLAY_ERROR,   // Ses çalma hatası
    FE_SOUND_MIXER_CHANNEL_ERROR // Kanalla ilgili hata
} fe_sound_mixer_error_t;

// --- Düşük Seviye Ses ve Müzik Yapıları ---
// Bu yapılar, SDL_mixer'ın dahili Mix_Chunk* ve Mix_Music* türlerini soyutlar.
// fe_audio_engine'da doğrudan bunları kullanacağız.

/**
 * @brief Yüklenmiş bir ses efektini temsil eden yapı.
 * Doğrudan Mix_Chunk* sarmalar.
 */
typedef struct fe_mixer_chunk {
    Mix_Chunk* sdl_chunk;
} fe_mixer_chunk_t;

/**
 * @brief Yüklenmiş bir müzik parçasını temsil eden yapı.
 * Doğrudan Mix_Music* sarmalar.
 */
typedef struct fe_mixer_music {
    Mix_Music* sdl_music;
} fe_mixer_music_t;


// --- Fonksiyon Deklarasyonları ---

/**
 * @brief SDL_mixer ses sistemini başlatır.
 *
 * @param frequency Örnekleme hızı (örn. 44100).
 * @param channels Kanal sayısı (1=mono, 2=stereo).
 * @param chunk_size Ses tampon boyutu (örn. 2048).
 * @param max_mixer_channels SDL_mixer tarafından ayrılacak maksimum kanal sayısı.
 * -1 verilirse, SDL_mixer varsayılanı kullanır.
 * @return fe_sound_mixer_error_t Başarı durumunu döner.
 */
fe_sound_mixer_error_t fe_sound_mixer_init(int frequency, int channels, int chunk_size, int max_mixer_channels);

/**
 * @brief SDL_mixer ses sistemini kapatır ve tüm kaynaklarını serbest bırakır.
 */
void fe_sound_mixer_shutdown();

/**
 * @brief Bir ses efekt dosyasını yükler ve bir fe_mixer_chunk_t nesnesi oluşturur.
 *
 * @param file_path Ses dosyasının yolu.
 * @param out_chunk Yüklenen ses verilerinin yazılacağı fe_mixer_chunk_t işaretçisi.
 * @return fe_sound_mixer_error_t Başarı durumunu döner.
 */
fe_sound_mixer_error_t fe_sound_mixer_load_chunk(const fe_string_t* file_path, fe_mixer_chunk_t* out_chunk);

/**
 * @brief Bir ses efektini bellekten serbest bırakır.
 *
 * @param chunk Serbest bırakılacak fe_mixer_chunk_t nesnesinin işaretçisi.
 */
void fe_sound_mixer_free_chunk(fe_mixer_chunk_t* chunk);

/**
 * @brief Bir müzik dosyasını yükler ve bir fe_mixer_music_t nesnesi oluşturur.
 *
 * @param file_path Müzik dosyasının yolu.
 * @param out_music Yüklenen müzik verilerinin yazılacağı fe_mixer_music_t işaretçisi.
 * @return fe_sound_mixer_error_t Başarı durumunu döner.
 */
fe_sound_mixer_error_t fe_sound_mixer_load_music(const fe_string_t* file_path, fe_mixer_music_t* out_music);

/**
 * @brief Bir müziği bellekten serbest bırakır.
 *
 * @param music Serbest bırakılacak fe_mixer_music_t nesnesinin işaretçisi.
 */
void fe_sound_mixer_free_music(fe_mixer_music_t* music);

/**
 * @brief Bir ses efektini belirli bir kanalda çalar.
 *
 * @param chunk Çalınacak ses efektini içeren fe_mixer_chunk_t.
 * @param loops Sesin kaç kez döngüye alınacağı (-1 sonsuz, 0 tek sefer).
 * @param channel_hint Tercih edilen kanal numarası (-1 herhangi bir kanal).
 * @return int Atanan kanal numarası, hata durumunda -1.
 */
int fe_sound_mixer_play_chunk(const fe_mixer_chunk_t* chunk, int loops, int channel_hint);

/**
 * @brief Belirli bir kanalda çalınan sesi durdurur.
 *
 * @param channel Durdurulacak kanal numarası.
 */
void fe_sound_mixer_stop_channel(int channel);

/**
 * @brief Bir müzik parçasını çalar.
 *
 * @param music Çalınacak müziği içeren fe_mixer_music_t.
 * @param loops Müziğin kaç kez döngüye alınacağı (-1 sonsuz, 0 tek sefer).
 * @return fe_sound_mixer_error_t Başarı durumunu döner.
 */
fe_sound_mixer_error_t fe_sound_mixer_play_music(const fe_mixer_music_t* music, int loops);

/**
 * @brief Çalmakta olan müziği duraklatır.
 * @return fe_sound_mixer_error_t Başarı durumunu döner.
 */
fe_sound_mixer_error_t fe_sound_mixer_pause_music();

/**
 * @brief Duraklatılmış müziği devam ettirir.
 * @return fe_sound_mixer_error_t Başarı durumunu döner.
 */
fe_sound_mixer_error_t fe_sound_mixer_resume_music();

/**
 * @brief Çalmakta olan müziği durdurur.
 * @return fe_sound_mixer_error_t Başarı durumunu döner.
 */
fe_sound_mixer_error_t fe_sound_mixer_halt_music();

/**
 * @brief Bir kanalın ses seviyesini ayarlar.
 *
 * @param channel Ses seviyesi ayarlanacak kanal numarası (-1 tüm kanallar için).
 * @param volume Ses seviyesi (0-128 arası SDL_mixer değeri).
 */
void fe_sound_mixer_set_channel_volume(int channel, int volume);

/**
 * @brief Müzik ses seviyesini ayarlar.
 *
 * @param volume Ses seviyesi (0-128 arası SDL_mixer değeri).
 */
void fe_sound_mixer_set_music_volume(int volume);

/**
 * @brief Belirli bir kanal için panlama değerlerini ayarlar.
 *
 * @param channel Panlama ayarlanacak kanal numarası.
 * @param left_pan Sol kanalın ses seviyesi (0-255).
 * @param right_pan Sağ kanalın ses seviyesi (0-255).
 * @return fe_sound_mixer_error_t Başarı durumunu döner.
 */
fe_sound_mixer_error_t fe_sound_mixer_set_channel_panning(int channel, uint8_t left_pan, uint8_t right_pan);

/**
 * @brief SDL_mixer tarafından tahsis edilen maksimum kanal sayısını döndürür.
 * @return int Maksimum kanal sayısı.
 */
int fe_sound_mixer_get_max_channels();

#endif // FE_SOUND_MIXER_H
