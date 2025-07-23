#ifndef FE_AUDIO_ENGINE_H
#define FE_AUDIO_ENGINE_H

#include "core/utils/fe_types.h" // fe_vec3 için
#include "core/containers/fe_hash_map.h" // Sesleri kaydetmek için
#include "core/containers/fe_string.h" // Ses ID'leri ve dosya yolları için

// SDL_mixer kütüphanesini dahil et (gerçek projede kütüphane kurulumu gereklidir)
// #include <SDL_mixer.h> // Bu gerçek kullanımda gerekli olacaktır.
// Ancak soyutlama için burada doğrudan kullanmayız, sadece referans veririz.

// --- Ses Motoru Hata Kodları ---
typedef enum fe_audio_error {
    FE_AUDIO_SUCCESS = 0,
    FE_AUDIO_NOT_INITIALIZED,
    FE_AUDIO_ALREADY_INITIALIZED,
    FE_AUDIO_INVALID_ARGUMENT,
    FE_AUDIO_OUT_OF_MEMORY,
    FE_AUDIO_LOAD_ERROR,       // Ses dosyası yüklenemedi
    FE_AUDIO_PLAY_ERROR,       // Ses çalma hatası
    FE_AUDIO_MIXER_ERROR,      // Alt seviye SDL_mixer hatası
    FE_AUDIO_SOUND_NOT_FOUND,  // Ses ID'si bulunamadı
    FE_AUDIO_MUSIC_NOT_FOUND,  // Müzik ID'si bulunamadı
    FE_AUDIO_UNKNOWN_ERROR
} fe_audio_error_t;

// --- Ses ID'si ve Müzik ID'si Tipleri ---
// Hash map'lerde ve fonksiyonlarda kullanılacak benzersiz kimlikler
typedef fe_string_t fe_sound_id_t; // Ses efekti ID'si (örn: "shoot_sound")
typedef fe_string_t fe_music_id_t; // Müzik parçası ID'si (örn: "background_music_level1")

// --- Ses Dinleyicisi (Listener) Yapısı ---
// Uzamsal ses için dinleyicinin konumu ve yönü
typedef struct fe_audio_listener {
    fe_vec3 position;       // Dinleyicinin dünya koordinatları
    fe_vec3 forward_vector; // Dinleyicinin ileri yön vektörü (normalize edilmiş)
    fe_vec3 up_vector;      // Dinleyicinin yukarı yön vektörü (normalize edilmiş)
    // SDL_mixer velocity'yi doğrudan desteklemez, ama hız için bir alan eklenebilir.
    // fe_vec3 velocity;
} fe_audio_listener_t;

// --- Ses Yayıcı (Emitter) Yapısı ---
// Uzamsal ses için sesin kaynağının konumu ve özellikleri
typedef struct fe_audio_emitter {
    fe_vec3 position;       // Ses yayıcısının dünya koordinatları
    float volume;           // Ses yayıcısının yerel ses seviyesi (0.0 - 1.0)
    float pitch;            // Ses yayıcısının perdesi (genellikle 1.0)
    float min_distance;     // Sesin maksimum ses seviyesinde duyulacağı minimum mesafe
    float max_distance;     // Sesin hiç duyulmayacağı maksimum mesafe
} fe_audio_emitter_t;


// --- Fonksiyon Deklarasyonları ---

/**
 * @brief Ses motorunu başlatır ve SDL_mixer'ı yapılandırır.
 *
 * @param frequency Örnekleme hızı (örn. 44100).
 * @param channels Kanal sayısı (1=mono, 2=stereo).
 * @param chunk_size Ses tampon boyutu (örn. 2048).
 * @return fe_audio_error_t Başarı durumunu döner.
 */
fe_audio_error_t fe_audio_engine_init(int frequency, int channels, int chunk_size);

/**
 * @brief Ses motorunu kapatır ve tüm kaynakları serbest bırakır.
 */
void fe_audio_engine_shutdown();

/**
 * @brief Bir ses dosyasını belleğe yükler ve benzersiz bir ID ile kaydeder.
 * Yüklenen sesler, fe_audio_engine_play_sound kullanılarak çalınabilir.
 *
 * @param sound_id Ses için atanacak benzersiz kimlik.
 * @param file_path Ses dosyasının yolu (örn. "assets/sounds/explosion.wav").
 * @return fe_audio_error_t Başarı durumunu döner.
 */
fe_audio_error_t fe_audio_engine_load_sound(fe_sound_id_t sound_id, const fe_string_t* file_path);

/**
 * @brief Yüklenmiş bir sesi ID'si ile bellekten boşaltır.
 *
 * @param sound_id Boşaltılacak sesin kimliği.
 * @return fe_audio_error_t Başarı durumunu döner.
 */
fe_audio_error_t fe_audio_engine_unload_sound(fe_sound_id_t sound_id);

/**
 * @brief Bir müzik dosyasını belleğe yükler ve benzersiz bir ID ile kaydeder.
 * Yüklenen müzikler, fe_audio_engine_play_music kullanılarak çalınabilir.
 *
 * @param music_id Müzik için atanacak benzersiz kimlik.
 * @param file_path Müzik dosyasının yolu (örn. "assets/music/background_loop.ogg").
 * @return fe_audio_error_t Başarı durumunu döner.
 */
fe_audio_error_t fe_audio_engine_load_music(fe_music_id_t music_id, const fe_string_t* file_path);

/**
 * @brief Yüklenmiş bir müziği ID'si ile bellekten boşaltır.
 *
 * @param music_id Boşaltılacak müziğin kimliği.
 * @return fe_audio_error_t Başarı durumunu döner.
 */
fe_audio_error_t fe_audio_engine_unload_music(fe_music_id_t music_id);

/**
 * @brief Yüklenmiş bir ses efektini çalar.
 *
 * @param sound_id Çalınacak sesin kimliği.
 * @param loops Sesin kaç kez döngüye alınacağı (-1 sonsuz, 0 tek sefer).
 * @param channel_hint Tercih edilen kanal numarası (-1 herhangi bir kanal).
 * @param volume Ses seviyesi (0.0 - 1.0).
 * @param pitch Sesin perdesi (genellikle 1.0).
 * @return int Atanan kanal numarası, hata durumunda -1.
 */
int fe_audio_engine_play_sound(fe_sound_id_t sound_id, int loops, int channel_hint, float volume, float pitch);

/**
 * @brief Belirli bir kanalda çalınan bir sesi durdurur.
 *
 * @param channel Durdurulacak kanal numarası.
 */
void fe_audio_engine_stop_sound(int channel);

/**
 * @brief Yüklenmiş bir müziği çalar.
 *
 * @param music_id Çalınacak müziğin kimliği.
 * @param loops Müziğin kaç kez döngüye alınacağı (-1 sonsuz, 0 tek sefer).
 * @return fe_audio_error_t Başarı durumunu döner.
 */
fe_audio_error_t fe_audio_engine_play_music(fe_music_id_t music_id, int loops);

/**
 * @brief Çalmakta olan müziği duraklatır.
 * @return fe_audio_error_t Başarı durumunu döner.
 */
fe_audio_error_t fe_audio_engine_pause_music();

/**
 * @brief Duraklatılmış müziği devam ettirir.
 * @return fe_audio_error_t Başarı durumunu döner.
 */
fe_audio_error_t fe_audio_engine_resume_music();

/**
 * @brief Çalmakta olan müziği durdurur.
 * @return fe_audio_error_t Başarı durumunu döner.
 */
fe_audio_error_t fe_audio_engine_stop_music();

/**
 * @brief Genel ses seviyesini ayarlar (0.0 - 1.0 arası).
 *
 * @param volume Genel ses seviyesi.
 */
void fe_audio_engine_set_master_volume(float volume);

/**
 * @brief Mevcut genel ses seviyesini döndürür.
 * @return float Genel ses seviyesi.
 */
float fe_audio_engine_get_master_volume();

/**
 * @brief Müzik ses seviyesini ayarlar (0.0 - 1.0 arası).
 *
 * @param volume Müzik ses seviyesi.
 */
void fe_audio_engine_set_music_volume(float volume);

/**
 * @brief Ses efekti ses seviyesini ayarlar (0.0 - 1.0 arası).
 *
 * @param volume Ses efekti ses seviyesi.
 */
void fe_audio_engine_set_sound_volume(float volume);

/**
 * @brief Ses dinleyicisinin (kameranın) konumunu ve yönünü günceller.
 * Bu, uzamsal ses hesaplamaları için kritik öneme sahiptir.
 *
 * @param listener Güncellenmiş dinleyici konumu ve yönü.
 * @return fe_audio_error_t Başarı durumunu döner.
 */
fe_audio_error_t fe_audio_engine_set_listener(const fe_audio_listener_t* listener);

/**
 * @brief Belirli bir ses efektini uzamsal olarak (3D) çalar.
 * Dinleyicinin konumuna göre sesin ses seviyesi ve panlaması ayarlanır.
 *
 * @param sound_id Çalınacak sesin kimliği.
 * @param emitter Sesin yayıldığı nokta ve özellikleri.
 * @param loops Sesin kaç kez döngüye alınacağı (-1 sonsuz, 0 tek sefer).
 * @return int Atanan kanal numarası, hata durumunda -1.
 */
int fe_audio_engine_play_spatial_sound(fe_sound_id_t sound_id, const fe_audio_emitter_t* emitter, int loops);

#endif // FE_AUDIO_ENGINE_H
