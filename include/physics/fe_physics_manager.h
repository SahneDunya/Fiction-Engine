#ifndef FE_PHYSICS_MANAGER_H
#define FE_PHYSICS_MANAGER_H

#include "core/utils/fe_types.h" // fe_vec3, fe_quat, fe_mat4 için
#include "core/containers/fe_hash_map.h" // Fiziksel varlıkları kaydetmek için
#include "physics/fe_chaos_world.h"     // Alt seviye Chaos entegrasyonu için
#include "physics/fe_chaos_body.h"      // Tekil rijit cisim yönetimi için

// --- Fizik Yönetici Hata Kodları ---
typedef enum fe_physics_manager_error {
    FE_PHYSICS_MANAGER_SUCCESS = 0,
    FE_PHYSICS_MANAGER_NOT_INITIALIZED,
    FE_PHYSICS_MANAGER_ALREADY_INITIALIZED,
    FE_PHYSICS_MANAGER_INVALID_ARGUMENT,
    FE_PHYSICS_MANAGER_OUT_OF_MEMORY,
    FE_PHYSICS_MANAGER_CHAOS_ERROR,      // Alt seviye Chaos hatası
    FE_PHYSICS_MANAGER_BODY_NOT_FOUND,
    FE_PHYSICS_MANAGER_UNKNOWN_ERROR
} fe_physics_manager_error_t;

// --- Geri Çağırma (Callback) Fonksiyon Tipleri ---
// Bu fonksiyonlar, fizik olayları meydana geldiğinde oyun katmanına bilgi iletmek için kullanılır.

/**
 * @brief Çarpışma başlangıcı olayı için geri çağırma fonksiyonu prototipi.
 * İki rijit cisim ilk kez çarpıştığında çağrılır.
 *
 * @param body1 Çarpışmaya katılan ilk rijit cisim.
 * @param body2 Çarpışmaya katılan ikinci rijit cisim.
 * @param contact_point Çarpışmanın dünya koordinatlarındaki tahmini temas noktası.
 * @param normal Çarpışmanın dünya koordinatlarındaki temas normali (body1'den body2'ye doğru).
 * @param impulse Çarpışmada uygulanan tahmini itme miktarı.
 */
typedef void (*fe_on_collision_enter_callback_t)(
    const fe_chaos_rigid_body_t* body1,
    const fe_chaos_rigid_body_t* body2,
    fe_vec3 contact_point,
    fe_vec3 normal,
    float impulse
);

/**
 * @brief Çarpışma devamı olayı için geri çağırma fonksiyonu prototipi.
 * İki rijit cisim çarpışmaya devam ederken her fizik adımında çağrılır.
 *
 * @param body1 Çarpışmaya katılan ilk rijit cisim.
 * @param body2 Çarpışmaya katılan ikinci rijit cisim.
 * @param contact_point Çarpışmanın dünya koordinatlarındaki tahmini temas noktası.
 * @param normal Çarpışmanın dünya koordinatlarındaki temas normali.
 */
typedef void (*fe_on_collision_stay_callback_t)(
    const fe_chaos_rigid_body_t* body1,
    const fe_chaos_rigid_body_t* body2,
    fe_vec3 contact_point,
    fe_vec3 normal
);

/**
 * @brief Çarpışma bitişi olayı için geri çağırma fonksiyonu prototipi.
 * İki rijit cisim artık çarpışmıyorsa çağrılır.
 *
 * @param body1 Çarpışmaya katılan ilk rijit cisim.
 * @param body2 Çarpışmaya katılan ikinci rijit cisim.
 */
typedef void (*fe_on_collision_exit_callback_t)(
    const fe_chaos_rigid_body_t* body1,
    const fe_chaos_rigid_body_t* body2
);

// --- Raycast Sonucu ---
// Işın dökümü (raycast) operasyonunun sonucunu tutar.
typedef struct fe_raycast_hit {
    bool hit;                   // Bir cisme isabet etti mi?
    fe_chaos_rigid_body_t* body; // İsabet eden rijit cisim (varsa)
    fe_vec3 position;           // İsabet noktasının dünya koordinatları
    fe_vec3 normal;             // İsabet noktasındaki yüzey normali
    float distance;             // Işının başlangıç noktasından isabet noktasına olan uzaklık
    void* user_data;            // İsabet eden cismin kullanıcı verisi (örn. oyun nesnesi)
} fe_raycast_hit_t;

// --- Fizik Yönetici Durumu (Singleton) ---
typedef struct fe_physics_manager_state {
    // Kayıtlı geri çağırma fonksiyonları
    fe_on_collision_enter_callback_t on_collision_enter_cb;
    fe_on_collision_stay_callback_t on_collision_stay_cb;
    fe_on_collision_exit_callback_t on_collision_exit_cb;

    float fixed_timestep; // Fizik simülasyonu için sabit zaman adımı (örn. 1/60 s)
    float accumulator;    // Fizik adımını ilerletmek için biriken zaman

    bool is_initialized;
} fe_physics_manager_state_t;

// --- Fonksiyon Deklarasyonları ---

/**
 * @brief Fizik yöneticisini ve altındaki Chaos fizik motorunu başlatır.
 *
 * @param gravity Yerçekimi vektörü.
 * @param fixed_timestep Fizik simülasyonu için sabit zaman adımı (örn. 1.0f / 60.0f).
 * @return fe_physics_manager_error_t Başarı durumunu döner.
 */
fe_physics_manager_error_t fe_physics_manager_init(fe_vec3 gravity, float fixed_timestep);

/**
 * @brief Fizik yöneticisini ve tüm kaynaklarını serbest bırakır.
 */
void fe_physics_manager_shutdown();

/**
 * @brief Fizik simülasyonunu günceller.
 * Bu fonksiyon ana oyun döngüsünden çağrılmalıdır ve sabit zaman adımlarını yönetir.
 *
 * @param delta_time Geçen gerçek zaman (saniye cinsinden).
 * @return fe_physics_manager_error_t Başarı durumunu döner.
 */
fe_physics_manager_error_t fe_physics_manager_update(float delta_time);

/**
 * @brief Bir çarpışma başlangıcı geri çağırma fonksiyonunu kaydeder.
 *
 * @param callback Geri çağrılacak fonksiyon işaretçisi. NULL ile kayıt silinebilir.
 */
void fe_physics_manager_register_on_collision_enter(fe_on_collision_enter_callback_t callback);

/**
 * @brief Bir çarpışma devamı geri çağırma fonksiyonunu kaydeder.
 *
 * @param callback Geri çağrılacak fonksiyon işaretçisi. NULL ile kayıt silinebilir.
 */
void fe_physics_manager_register_on_collision_stay(fe_on_collision_stay_callback_t callback);

/**
 * @brief Bir çarpışma bitişi geri çağırma fonksiyonunu kaydeder.
 *
 * @param callback Geri çağrılacak fonksiyon işaretçisi. NULL ile kayıt silinebilir.
 */
void fe_physics_manager_register_on_collision_exit(fe_on_collision_exit_callback_t callback);

/**
 * @brief Belirtilen rijit cismin ID'sine göre kullanıcı verisini (genellikle oyun nesnesi işaretçisi) ayarlar.
 * Bu, çarpışma olaylarında hangi oyun nesnesinin çarptığını belirlemek için kullanılır.
 *
 * @param body_id Kullanıcı verisinin atanacağı rijit cismin ID'si.
 * @param user_data Atanacak kullanıcı verisi.
 * @return fe_physics_manager_error_t Başarı durumunu döner.
 */
fe_physics_manager_error_t fe_physics_manager_set_rigid_body_user_data(const char* body_id, void* user_data);

/**
 * @brief Belirtilen rijit cismin ID'sine göre kullanıcı verisini (genellikle oyun nesnesi işaretçisi) döndürür.
 *
 * @param body_id Kullanıcı verisinin alınacağı rijit cismin ID'si.
 * @return void* Rijit cismin kullanıcı verisi, bulunamazsa NULL.
 */
void* fe_physics_manager_get_rigid_body_user_data(const char* body_id);

/**
 * @brief Dünya üzerinde bir ışın dökümü (raycast) gerçekleştirir.
 *
 * @param origin Işının başlangıç noktası.
 * @param direction Işının yön vektörü (normalize edilmiş olmalı).
 * @param max_distance Işının gidebileceği maksimum uzaklık.
 * @param out_hit İsabet bilgisinin yazılacağı fe_raycast_hit_t yapısının işaretçisi.
 * @param collision_mask Hangi çarpışma gruplarının kontrol edileceğini belirten maske (isteğe bağlı, 0xFFFFFFFF tümü).
 * @return fe_physics_manager_error_t Başarı durumunu döner.
 */
fe_physics_manager_error_t fe_physics_manager_raycast(
    fe_vec3 origin,
    fe_vec3 direction,
    float max_distance,
    fe_raycast_hit_t* out_hit,
    uint32_t collision_mask
);

// Diğer faydalı sorgulama fonksiyonları (isteğe bağlı, gelecekte eklenebilir):
// fe_physics_manager_sphere_cast(...);
// fe_physics_manager_overlap_sphere(...);
// fe_physics_manager_overlap_box(...);

#endif // FE_PHYSICS_MANAGER_H
