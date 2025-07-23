#ifndef FE_ANIM_CONTROLLER_H
#define FE_ANIM_CONTROLLER_H

#include "core/utils/fe_types.h"
#include "core/containers/fe_hash_map.h"
#include "animation/fe_skeleton_animation.h" // fe_skeleton_t, fe_animation_clip_t, fe_animation_state_t için

// --- Animasyon Kontrolcüsü Hata Kodları ---
typedef enum fe_anim_controller_error {
    FE_ANIM_CONTROLLER_SUCCESS = 0,
    FE_ANIM_CONTROLLER_INVALID_ARGUMENT,
    FE_ANIM_CONTROLLER_OUT_OF_MEMORY,
    FE_ANIM_CONTROLLER_NOT_INITIALIZED,
    FE_ANIM_CONTROLLER_ANIM_NOT_FOUND,
    FE_ANIM_CONTROLLER_INVALID_STATE,
    FE_ANIM_CONTROLLER_ALREADY_PLAYING,
    FE_ANIM_CONTROLLER_UNKNOWN_ERROR
} fe_anim_controller_error_t;

// --- Animasyon Katmanı Tanımları ---
// Basit bir öncelik katmanlandırması için kullanılabilir
typedef enum fe_anim_layer_priority {
    FE_ANIM_LAYER_BASE = 0, // Ana hareketler (yürüme, koşma, boşta)
    FE_ANIM_LAYER_UPPER_BODY, // Üst vücut hareketleri (silah tutma, el sallama)
    FE_ANIM_LAYER_EXPRESSION, // Yüz ifadeleri gibi düşük öncelikli küçük hareketler
    FE_ANIM_LAYER_OVERRIDE, // Her şeyi geçersiz kılan yüksek öncelikli animasyonlar (örn. ölüm, ragdoll başlangıcı)
    FE_ANIM_LAYER_COUNT       // Toplam katman sayısı
} fe_anim_layer_priority_t;

/**
 * @brief Animasyon katmanları için karıştırma modları.
 */
typedef enum fe_anim_blend_mode {
    FE_ANIM_BLEND_OVERRIDE = 0, // Önceki katmanı tamamen geçersiz kılar
    FE_ANIM_BLEND_ADDITIVE      // Önceki katmana ekler (henüz desteklenmiyor, gelecekteki özellik)
} fe_anim_blend_mode_t;


// --- Animasyon Katmanı Yapısı ---
typedef struct fe_anim_layer {
    fe_animation_state_t* anim_state;      // Bu katmanın mevcut animasyon durumu
    float weight;                          // Bu katmanın karıştırma ağırlığı (0.0 - 1.0)
    fe_anim_blend_mode_t blend_mode;       // Karıştırma modu
    fe_string_t affected_bone_root;        // Eğer katman sadece bir kemik hiyerarşisini etkiliyorsa, kök kemik adı
    bool use_partial_mask;                 // Kısmi maske kullanılıp kullanılmayacağı
    // fe_hash_map_t bone_mask;             // (Gelecekte) Hangi kemiklerin bu katmandan etkileneceğini belirten maske (bone_name -> bool)
} fe_anim_layer_t;


/**
 * @brief Animasyon geçişi için gerekli parametreler.
 */
typedef struct fe_anim_transition_params {
    fe_animation_clip_t* target_clip; // Geçiş yapılacak hedef animasyon klibi
    float transition_duration;        // Geçişin süresi (saniye)
    float elapsed_time;               // Geçişin başlangıcından bu yana geçen süre
    fe_anim_layer_priority_t layer;   // Geçişin yapılacağı katman
    fe_animation_loop_mode_t loop_mode; // Hedef klibin döngü modu
    float playback_speed;             // Hedef klibin oynatma hızı
} fe_anim_transition_params_t;


/**
 * @brief Animasyon kontrolcüsü ana yapısı.
 * Her animasyonlu varlık için bir örnek oluşturulmalıdır.
 */
typedef struct fe_anim_controller {
    fe_skeleton_t* skeleton;                          // Kontrolcünün bağlı olduğu iskelet
    fe_hash_map_t registered_clips;                   // fe_string_t (clip_name) -> fe_animation_clip_t*
    fe_anim_layer_t layers[FE_ANIM_LAYER_COUNT];      // Her katman için animasyon durumu
    
    fe_anim_transition_params_t* current_transition;  // Şu an aktif olan geçişin işaretçisi (NULL ise geçiş yok)
    fe_animation_clip_t* transition_from_clip;        // Geçişin başladığı klip (karıştırma için)

    bool is_initialized;
} fe_anim_controller_t;


// --- Animasyon Kontrolcüsü Fonksiyonları ---

/**
 * @brief Yeni bir animasyon kontrolcüsü oluşturur ve başlatır.
 *
 * @param skeleton Kontrolcünün yöneteceği iskelet.
 * @return fe_anim_controller_t* Yeni oluşturulan kontrolcü, hata durumunda NULL.
 */
fe_anim_controller_t* fe_anim_controller_create(fe_skeleton_t* skeleton);

/**
 * @brief Animasyon kontrolcüsünü temizler ve bellekten serbest bırakır.
 * Kayıtlı animasyon kliplerini *serbest bırakmaz* (çünkü onlar harici olarak yönetilir).
 *
 * @param controller Serbest bırakılacak kontrolcünün işaretçisi.
 */
void fe_anim_controller_destroy(fe_anim_controller_t* controller);

/**
 * @brief Bir animasyon klibini kontrolcüye kaydeder.
 * Kontrolcü, kaydettiği klipleri adlarıyla arayabilir. Klibin belleği kontrolcü tarafından yönetilmez.
 *
 * @param controller Kontrolcünün işaretçisi.
 * @param clip Kaydedilecek animasyon klibi.
 * @return fe_anim_controller_error_t Başarı durumunu döner.
 */
fe_anim_controller_error_t fe_anim_controller_register_clip(fe_anim_controller_t* controller, fe_animation_clip_t* clip);

/**
 * @brief Bir animasyon klibini kontrolcüden kaldırır.
 * Klibin belleğini serbest bırakmaz.
 *
 * @param controller Kontrolcünün işaretçisi.
 * @param clip_name Kaldırılacak klibin adı.
 * @return fe_anim_controller_error_t Başarı durumunu döner.
 */
fe_anim_controller_error_t fe_anim_controller_unregister_clip(fe_anim_controller_t* controller, const char* clip_name);

/**
 * @brief Belirli bir animasyon klibini belirli bir katmanda anında oynatmaya başlar.
 * Mevcut animasyon durdurulur ve yeni animasyon başlatılır. Geçiş yapılmaz.
 *
 * @param controller Kontrolcünün işaretçisi.
 * @param clip_name Oynatılacak klibin adı.
 * @param layer Oynatmanın yapılacağı animasyon katmanı.
 * @param loop_mode Döngü modu.
 * @param playback_speed Oynatma hızı çarpanı.
 * @return fe_anim_controller_error_t Başarı durumunu döner.
 */
fe_anim_controller_error_t fe_anim_controller_play(fe_anim_controller_t* controller, const char* clip_name, fe_anim_layer_priority_t layer, fe_animation_loop_mode_t loop_mode, float playback_speed);

/**
 * @brief Bir animasyondan diğerine yumuşak bir geçiş başlatır.
 *
 * @param controller Kontrolcünün işaretçisi.
 * @param target_clip_name Geçiş yapılacak hedef klibin adı.
 * @param transition_duration Geçişin süresi (saniye).
 * @param layer Geçişin yapılacağı animasyon katmanı.
 * @param loop_mode Hedef klibin döngü modu.
 * @param playback_speed Hedef klibin oynatma hızı.
 * @return fe_anim_controller_error_t Başarı durumunu döner.
 */
fe_anim_controller_error_t fe_anim_controller_crossfade(fe_anim_controller_t* controller, const char* target_clip_name, float transition_duration, fe_anim_layer_priority_t layer, fe_animation_loop_mode_t loop_mode, float playback_speed);

/**
 * @brief Belirli bir katmandaki animasyonu duraklatır.
 *
 * @param controller Kontrolcünün işaretçisi.
 * @param layer Duraklatılacak katman.
 * @return fe_anim_controller_error_t Başarı durumunu döner.
 */
fe_anim_controller_error_t fe_anim_controller_pause(fe_anim_controller_t* controller, fe_anim_layer_priority_t layer);

/**
 * @brief Belirli bir katmandaki duraklatılmış animasyonu devam ettirir.
 *
 * @param controller Kontrolcünün işaretçisi.
 * @param layer Devam ettirilecek katman.
 * @return fe_anim_controller_error_t Başarı durumunu döner.
 */
fe_anim_controller_error_t fe_anim_controller_resume(fe_anim_controller_t* controller, fe_anim_layer_priority_t layer);

/**
 * @brief Belirli bir katmandaki animasyonu durdurur.
 *
 * @param controller Kontrolcünün işaretçisi.
 * @param layer Durdurulacak katman.
 * @return fe_anim_controller_error_t Başarı durumunu döner.
 */
fe_anim_controller_error_t fe_anim_controller_stop(fe_anim_controller_t* controller, fe_anim_layer_priority_t layer);

/**
 * @brief Animasyon kontrolcüsünü ve bağlı iskeleti günceller.
 * Bu fonksiyon her karede çağrılmalıdır.
 *
 * @param controller Kontrolcünün işaretçisi.
 * @param delta_time Geçen zaman (saniye).
 * @return fe_anim_controller_error_t Başarı durumunu döner.
 */
fe_anim_controller_error_t fe_anim_controller_update(fe_anim_controller_t* controller, float delta_time);

/**
 * @brief Belirli bir katmanın ağırlığını ayarlar.
 *
 * @param controller Kontrolcünün işaretçisi.
 * @param layer Ayarlanacak katman.
 * @param weight Yeni ağırlık (0.0 - 1.0 arası).
 * @return fe_anim_controller_error_t Başarı durumunu döner.
 */
fe_anim_controller_error_t fe_anim_controller_set_layer_weight(fe_anim_controller_t* controller, fe_anim_layer_priority_t layer, float weight);

/**
 * @brief Belirli bir katmanın kısmi maske kullanımını ayarlar.
 *
 * @param controller Kontrolcünün işaretçisi.
 * @param layer Ayarlanacak katman.
 * @param use_mask Kısmi maske kullanılacaksa true, aksi takdirde false.
 * @param bone_root Eğer use_mask true ise, maskenin uygulanacağı kök kemiğin adı. NULL ise tüm iskelet etkilenir.
 * @return fe_anim_controller_error_t Başarı durumunu döner.
 */
fe_anim_controller_error_t fe_anim_controller_set_layer_partial_mask(fe_anim_controller_t* controller, fe_anim_layer_priority_t layer, bool use_mask, const char* bone_root);

#endif // FE_ANIM_CONTROLLER_H
