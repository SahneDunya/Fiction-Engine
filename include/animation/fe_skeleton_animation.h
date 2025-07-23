#ifndef FE_SKELETON_ANIMATION_H
#define FE_SKELETON_ANIMATION_H

#include "core/utils/fe_types.h" // fe_vec3, fe_quat, fe_mat4, fe_string_t için
#include "core/containers/fe_hash_map.h" // Kemik ID'lerini ve animasyonları saklamak için
#include "core/containers/fe_array.h" // Kemik ve anahtar kare dizileri için

// --- Animasyon Hata Kodları ---
typedef enum fe_animation_error {
    FE_ANIMATION_SUCCESS = 0,
    FE_ANIMATION_INVALID_ARGUMENT,
    FE_ANIMATION_OUT_OF_MEMORY,
    FE_ANIMATION_NOT_FOUND,      // Animasyon veya kemik bulunamadı
    FE_ANIMATION_INVALID_STATE,  // Geçersiz animasyon durumu
    FE_ANIMATION_NULL_POINTER,
    FE_ANIMATION_UNKNOWN_ERROR
} fe_animation_error_t;

// --- Temel Animasyon Yapıları ---

/**
 * @brief Bir kemiğin yerel dönüşümünü (konum, rotasyon, ölçek) temsil eden anahtar kare verisi.
 */
typedef struct fe_animation_keyframe {
    float time;   // Bu anahtar karenin animasyondaki zamanı (saniye cinsinden)
    fe_vec3 position;
    fe_quat rotation;
    fe_vec3 scale;
} fe_animation_keyframe_t;

/**
 * @brief Bir kemiğe ait tüm anahtar kare kanallarını tutar.
 */
typedef struct fe_animation_bone_channel {
    fe_string_t bone_name; // Bu kanalın ait olduğu kemiğin adı
    fe_array_t position_keyframes; // fe_animation_keyframe_t (sadece konum verisi önemli)
    fe_array_t rotation_keyframes; // fe_animation_keyframe_t (sadece rotasyon verisi önemli)
    fe_array_t scale_keyframes;    // fe_animation_keyframe_t (sadece ölçek verisi önemli)
} fe_animation_bone_channel_t;

/**
 * @brief Tek bir animasyon klibini temsil eder (örneğin "walk", "run", "idle").
 * Tüm kemik kanallarını ve animasyonun süresini içerir.
 */
typedef struct fe_animation_clip {
    fe_string_t name;       // Animasyon klibinin adı (örn. "walk", "idle")
    float duration;         // Animasyon klibinin toplam süresi (saniye)
    float ticks_per_second; // Animasyonun orijinal hız çarpanı (FPS gibi)
    fe_array_t bone_channels; // fe_animation_bone_channel_t dizisi
    
    // Hızlı erişim için kemik adına göre kanal indeksini tutan bir hash map
    fe_hash_map_t bone_channel_map; // fe_string_t -> int (index of bone_channels array)
} fe_animation_clip_t;

/**
 * @brief Bir iskeletteki tek bir kemiği temsil eder.
 *
 * `local_transform`: Kemiğin ebeveynine göre dönüşümü (modelin bind pose'undaki hali).
 * `inverse_bind_transform`: Kemiğin bind pose'undaki dünya dönüşümünün tersi.
 * Bu, animasyon matrislerini modelin uzayına geri getirmek için kullanılır.
 * `final_transform`: Animasyon uygulandıktan sonraki nihai dünya dönüşüm matrisi.
 * Bu matrisler doğrudan GPU'ya gönderilir.
 */
typedef struct fe_skeleton_bone {
    fe_string_t name;           // Kemiğin adı (örn. "Hip", "RightThigh")
    int parent_index;           // Ebeveyn kemiğin iskelet dizisindeki indeksi (-1 ise kök kemik)
    fe_mat4 local_transform;    // Kemiğin ebeveynine göre dönüşümü (bind pose'da)
    fe_mat4 inverse_bind_transform; // Bind pose'daki dünya dönüşümünün tersi
    fe_mat4 final_transform;    // Animasyon uygulandıktan sonraki nihai dönüşüm matrisi (GPU'ya gönderilir)
} fe_skeleton_bone_t;

/**
 * @brief Karakterin veya nesnenin iskeletini temsil eder.
 * Tüm kemiklerin hiyerarşisini ve onların bind pose dönüşümlerini içerir.
 */
typedef struct fe_skeleton {
    fe_string_t name;       // İskeletin adı (örn. "HumanoidSkeleton")
    fe_array_t bones;       // fe_skeleton_bone_t dizisi. Kök kemik genellikle 0. indekstedir.
    
    // Hızlı erişim için kemik adına göre kemik indeksini tutan bir hash map
    fe_hash_map_t bone_map; // fe_string_t -> int (index of bones array)
} fe_skeleton_t;

// --- Animasyon Durumu Yönetimi ---

typedef enum fe_animation_loop_mode {
    FE_ANIM_LOOP_NONE = 0, // Animasyon bittiğinde durur
    FE_ANIM_LOOP_REPEAT    // Animasyon bittiğinde baştan başlar
} fe_animation_loop_mode_t;

/**
 * @brief Şu anda oynatılmakta olan bir animasyonun durumunu tutar.
 * Her animasyonlu varlık için ayrı bir örneği tutulur.
 */
typedef struct fe_animation_state {
    fe_animation_clip_t* current_clip; // Şu an oynatılan animasyon klibinin işaretçisi
    float current_time;                // Animasyonda o anki zaman (saniye)
    float playback_speed;              // Animasyonun oynatma hızı çarpanı (1.0 normal hız)
    fe_animation_loop_mode_t loop_mode; // Döngü modu
    bool is_playing;                   // Animasyon çalıyor mu
} fe_animation_state_t;


// --- Animasyon Klibi Fonksiyonları ---

/**
 * @brief Yeni bir animasyon klibi oluşturur.
 * Animasyonun süresi ve ticks_per_second gibi temel bilgileri ayarlanır.
 *
 * @param name Klibin adı.
 * @param duration Klibin süresi (saniye).
 * @param ticks_per_second Klibin orijinal kare hızı.
 * @return fe_animation_clip_t* Yeni oluşturulan klip, hata durumunda NULL.
 */
fe_animation_clip_t* fe_animation_clip_create(const char* name, float duration, float ticks_per_second);

/**
 * @brief Bir animasyon klibine yeni bir kemik kanalı ekler.
 *
 * @param clip Klibin işaretçisi.
 * @param bone_name Kanalın ait olduğu kemiğin adı.
 * @return fe_animation_bone_channel_t* Yeni oluşturulan kanalın işaretçisi, hata durumunda NULL.
 */
fe_animation_bone_channel_t* fe_animation_clip_add_bone_channel(fe_animation_clip_t* clip, const char* bone_name);

/**
 * @brief Bir kemik kanalına pozisyon anahtar karesi ekler.
 *
 * @param channel Kanalın işaretçisi.
 * @param time Anahtar karenin zamanı (saniye).
 * @param pos Konum vektörü.
 * @return fe_animation_error_t Başarı durumunu döner.
 */
fe_animation_error_t fe_animation_bone_channel_add_position_keyframe(fe_animation_bone_channel_t* channel, float time, fe_vec3 pos);

/**
 * @brief Bir kemik kanalına rotasyon anahtar karesi ekler.
 *
 * @param channel Kanalın işaretçisi.
 * @param time Anahtar karenin zamanı (saniye).
 * @param rot Rotasyon (Quaternion).
 * @return fe_animation_error_t Başarı durumunu döner.
 */
fe_animation_error_t fe_animation_bone_channel_add_rotation_keyframe(fe_animation_bone_channel_t* channel, float time, fe_quat rot);

/**
 * @brief Bir kemik kanalına ölçek anahtar karesi ekler.
 *
 * @param channel Kanalın işaretçisi.
 * @param time Anahtar karenin zamanı (saniye).
 * @param scale Ölçek vektörü.
 * @return fe_animation_error_t Başarı durumunu döner.
 */
fe_animation_error_t fe_animation_bone_channel_add_scale_keyframe(fe_animation_bone_channel_t* channel, float time, fe_vec3 scale);

/**
 * @brief Bir animasyon klibini temizler ve bellekten serbest bırakır.
 *
 * @param clip Serbest bırakılacak klibin işaretçisi.
 */
void fe_animation_clip_destroy(fe_animation_clip_t* clip);


// --- İskelet Fonksiyonları ---

/**
 * @brief Yeni bir iskelet oluşturur.
 *
 * @param name İskeletin adı.
 * @return fe_skeleton_t* Yeni oluşturulan iskelet, hata durumunda NULL.
 */
fe_skeleton_t* fe_skeleton_create(const char* name);

/**
 * @brief Bir iskelete yeni bir kemik ekler.
 * Kök kemik için parent_index -1 olmalıdır.
 *
 * @param skeleton İskeletin işaretçisi.
 * @param name Kemiğin adı.
 * @param parent_index Ebeveyn kemiğin iskeletin kemik dizisindeki indeksi (-1 ise kök).
 * @param local_transform Kemiğin ebeveynine göre bind pose'daki yerel dönüşümü.
 * @param inverse_bind_transform Kemiğin bind pose'daki dünya dönüşümünün tersi.
 * @return fe_skeleton_bone_t* Yeni eklenen kemiğin işaretçisi, hata durumunda NULL.
 */
fe_skeleton_bone_t* fe_skeleton_add_bone(fe_skeleton_t* skeleton, const char* name, int parent_index, fe_mat4 local_transform, fe_mat4 inverse_bind_transform);

/**
 * @brief Bir iskeleti temizler ve bellekten serbest bırakır.
 *
 * @param skeleton Serbest bırakılacak iskeletin işaretçisi.
 */
void fe_skeleton_destroy(fe_skeleton_t* skeleton);


// --- Animasyon Durumu Fonksiyonları ---

/**
 * @brief Yeni bir animasyon durumu başlatır.
 *
 * @return fe_animation_state_t* Yeni oluşturulan animasyon durumu, hata durumunda NULL.
 */
fe_animation_state_t* fe_animation_state_create();

/**
 * @brief Bir animasyon durumunu temizler ve bellekten serbest bırakır.
 *
 * @param state Serbest bırakılacak animasyon durumunun işaretçisi.
 */
void fe_animation_state_destroy(fe_animation_state_t* state);

/**
 * @brief Bir animasyon klibini çalmaya başlar veya değiştirir.
 *
 * @param state Animasyon durumu.
 * @param clip Çalınacak animasyon klibi.
 * @param playback_speed Oynatma hızı çarpanı (1.0 normal hız).
 * @param loop_mode Döngü modu.
 * @return fe_animation_error_t Başarı durumunu döner.
 */
fe_animation_error_t fe_animation_state_play(fe_animation_state_t* state, fe_animation_clip_t* clip, float playback_speed, fe_animation_loop_mode_t loop_mode);

/**
 * @brief Bir animasyonu duraklatır.
 *
 * @param state Animasyon durumu.
 * @return fe_animation_error_t Başarı durumunu döner.
 */
fe_animation_error_t fe_animation_state_pause(fe_animation_state_t* state);

/**
 * @brief Duraklatılmış bir animasyonu devam ettirir.
 *
 * @param state Animasyon durumu.
 * @return fe_animation_error_t Başarı durumunu döner.
 */
fe_animation_error_t fe_animation_state_resume(fe_animation_state_t* state);

/**
 * @brief Bir animasyonu durdurur.
 *
 * @param state Animasyon durumu.
 * @return fe_animation_error_t Başarı durumunu döner.
 */
fe_animation_error_t fe_animation_state_stop(fe_animation_state_t* state);

/**
 * @brief Animasyon durumunu günceller ve kemik dönüşüm matrislerini hesaplar.
 * Bu fonksiyon her karede çağrılmalıdır.
 *
 * @param state Animasyon durumu.
 * @param skeleton Animasyonun uygulanacağı iskelet.
 * @param delta_time Geçen zaman (saniye).
 * @return fe_animation_error_t Başarı durumunu döner.
 */
fe_animation_error_t fe_animation_state_update(fe_animation_state_t* state, fe_skeleton_t* skeleton, float delta_time);

/**
 * @brief Bir kemiğin nihai dönüşüm matrisini alır.
 * Bu matris, GPU'ya shader'da deformasyon için gönderilebilir.
 *
 * @param skeleton İskelet.
 * @param bone_index İstenen kemiğin indeksi.
 * @return const fe_mat4* Kemiğin nihai dönüşüm matrisinin işaretçisi, hata durumunda NULL.
 */
const fe_mat4* fe_skeleton_get_final_bone_transform(const fe_skeleton_t* skeleton, int bone_index);

#endif // FE_SKELETON_ANIMATION_H
