#ifndef FE_CHAOS_WORLD_H
#define FE_CHAOS_WORLD_H

#include "core/utils/fe_types.h" // fe_vec3, fe_quat, fe_mat4 vb. için
#include "core/containers/fe_hash_map.h" // Fizik nesnelerini takip etmek için

// Chaos API Hata Kodları
typedef enum fe_chaos_error {
    FE_CHAOS_SUCCESS = 0,
    FE_CHAOS_NOT_INITIALIZED,
    FE_CHAOS_ALREADY_INITIALIZED,
    FE_CHAOS_INVALID_ARGUMENT,
    FE_CHAOS_OUT_OF_MEMORY,
    FE_CHAOS_SCENE_CREATION_FAILED,
    FE_CHAOS_BODY_CREATION_FAILED,
    FE_CHAOS_SHAPE_CREATION_FAILED,
    FE_CHAOS_UNKNOWN_ERROR
} fe_chaos_error_t;

// --- Fizik Nesnesi Tipleri ---
typedef enum fe_physics_body_type {
    FE_PHYSICS_BODY_STATIC,   // Hareketsiz, sadece çarpışma için
    FE_PHYSICS_BODY_DYNAMIC,  // Simülasyon tarafından hareket ettirilen rijit cisim
    FE_PHYSICS_BODY_KINEMATIC // Kod tarafından hareket ettirilen, diğerlerine çarpan cisim
} fe_physics_body_type_t;

// --- Çarpışma Şekli Tipleri ---
typedef enum fe_collision_shape_type {
    FE_COLLISION_SHAPE_SPHERE,
    FE_COLLISION_SHAPE_BOX,
    FE_COLLISION_SHAPE_CAPSULE,
    FE_COLLISION_SHAPE_PLANE,
    FE_COLLISION_SHAPE_MESH, // Karmaşık mesh çarpışması (pahalı olabilir)
    FE_COLLISION_SHAPE_CONVEX_HULL, // Convex mesh çarpışması
    FE_COLLISION_SHAPE_COUNT
} fe_collision_shape_type_t;

// --- Çarpışma Şekli Tanımları ---
// Her şekil tipi için özel veri içerir.
typedef struct fe_sphere_collision_shape {
    float radius;
} fe_sphere_collision_shape_t;

typedef struct fe_box_collision_shape {
    fe_vec3 half_extents; // Kutu boyutlarının yarısı (x, y, z)
} fe_box_collision_shape_t;

typedef struct fe_capsule_collision_shape {
    float radius;
    float half_height; // Silindirik kısmın yarı yüksekliği (uç kapaklar hariç)
} fe_capsule_collision_shape_t;

typedef struct fe_plane_collision_shape {
    fe_vec3 normal;
    float distance; // Orijinden uzaklık
} fe_plane_collision_shape_t;

// Mesh ve Convex Hull için
typedef struct fe_mesh_collision_shape {
    // Model verisine referans veya doğrudan mesh verisi
    // Bu genellikle varlık yönetim sistemimizden bir referans olacaktır.
    fe_string mesh_id; // Varlık yöneticisindeki mesh ID'si
    // Chaos'un beklediği formatta vertex/index verileri olabilir
    // const float* vertices;
    // const uint32_t* indices;
    // size_t vertex_count;
    // size_t index_count;
    bool is_convex; // Convex hull mı yoksa trimesh mi
} fe_mesh_collision_shape_t;

// Ortak çarpışma şekli birleşimi
typedef union fe_collision_shape_data {
    fe_sphere_collision_shape_t sphere;
    fe_box_collision_shape_t box;
    fe_capsule_collision_shape_t capsule;
    fe_plane_collision_shape_t plane;
    fe_mesh_collision_shape_t mesh;
} fe_collision_shape_data_t;

// --- Çarpışma Şekli Yapısı ---
typedef struct fe_collision_shape {
    fe_collision_shape_type_t type;
    fe_collision_shape_data_t data;
    fe_vec3 local_offset; // Rijit cismin merkezine göre yerel ofset
    fe_quat local_rotation; // Rijit cismin rotasyonuna göre yerel rotasyon
    // Chaos'a özgü dahili tutaçlar
    void* chaos_shape_handle;
} fe_collision_shape_t;

// --- Rijit Cisim Özellikleri ---
typedef struct fe_rigid_body_desc {
    fe_string name;             // Debugging için isim
    fe_physics_body_type_t type; // Dinamik, statik, kinematik
    float mass;                 // Kütle (dinamik cisimler için)
    fe_vec3 initial_position;   // Başlangıç konumu
    fe_quat initial_rotation;   // Başlangıç rotasyonu
    fe_vec3 initial_linear_velocity;  // Başlangıç doğrusal hızı
    fe_vec3 initial_angular_velocity; // Başlangıç açısal hızı
    float linear_damping;       // Doğrusal sönümleme
    float angular_damping;      // Açısal sönümleme
    float restitution;          // Geri sekme katsayısı (0-1)
    float static_friction;      // Statik sürtünme katsayısı
    float dynamic_friction;     // Dinamik sürtünme katsayısı
    bool enable_gravity;        // Yerçekimi etkilensin mi?
    bool is_kinematic;          // Kinematik mi? (fizik tarafından değil, kod tarafından kontrol edilir)
    bool is_trigger;            // Sadece tetikleyici mi? (çarpışma tespiti yapar ama fiziksel tepki vermez)

    // Çarpışma filtreleme/gruplama bilgileri
    uint32_t collision_group;
    uint32_t collision_mask;

    // Şekil verileri (Birden fazla şekil olabilir)
    fe_collision_shape_t* collision_shapes;
    size_t shape_count;
    size_t shape_capacity;
} fe_rigid_body_desc_t;


// --- FE Chaos Rijit Cisim Tutacı ---
// Bir Chaos fizik simülasyonundaki tekil bir rijit cismi temsil eder.
typedef struct fe_chaos_rigid_body {
    fe_string id;              // Benzersiz ID (UUID veya motor varlık ID'si)
    fe_rigid_body_desc_t desc; // Oluşturma desc'inin kopyası
    void* chaos_actor_handle;  // Chaos API'sindeki karşılık gelen aktör/cisim tutacı
    void* user_data;           // Gerekirse kullanıcı tanımlı veri (örn. oyun nesnesi işaretçisi)
} fe_chaos_rigid_body_t;


// --- FE Chaos Dünya Durumu (Singleton) ---
// Chaos fizik dünyasının ana yönetim yapısı.
typedef struct fe_chaos_world_state {
    void* chaos_scene_handle; // Chaos API'sindeki ana fizik sahnesi tutacı (FScene*)
    fe_vec3 gravity;          // Yerçekimi vektörü

    // Rijit cisimleri takip etmek için hash map
    fe_hash_map* rigid_bodies_map; // fe_string (ID) -> fe_chaos_rigid_body*

    bool is_initialized;
} fe_chaos_world_state_t;


// --- Fonksiyon Deklarasyonları ---

/**
 * @brief Chaos fizik dünyasını başlatır.
 * Chaos kütüphanesini yükler, ana fizik sahnesini oluşturur ve varsayılan ayarları yapar.
 * Bu fonksiyon çağrılmadan önce fe_memory_manager_init() çağrılmış olmalıdır.
 *
 * @param gravity Yerçekimi vektörü (örn. {0.0f, -9.81f, 0.0f}).
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_world_init(fe_vec3 gravity);

/**
 * @brief Chaos fizik dünyasını kapatır ve tüm kaynakları serbest bırakır.
 * Tüm rijit cisimleri, çarpışma şekillerini ve ana fizik sahnesini temizler.
 */
void fe_chaos_world_shutdown();

/**
 * @brief Chaos fizik dünyasını belirli bir zaman adımıyla günceller.
 * Bu, fizik simülasyonunu ilerletir.
 *
 * @param delta_time Geçen zaman (saniye cinsinden).
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_world_update(float delta_time);

/**
 * @brief Rijit cisim tanımlayıcısını varsayılan değerlerle doldurur.
 *
 * @param desc Doldurulacak tanımlayıcı yapısı.
 */
void fe_chaos_world_default_rigid_body_desc(fe_rigid_body_desc_t* desc);

/**
 * @brief Yeni bir rijit cisim oluşturur ve fizik dünyasına ekler.
 *
 * @param desc Rijit cismin özellikleri.
 * @return fe_chaos_rigid_body_t* Oluşturulan rijit cisim tutacına işaretçi, başarısız olursa NULL.
 */
fe_chaos_rigid_body_t* fe_chaos_world_create_rigid_body(const fe_rigid_body_desc_t* desc);

/**
 * @brief Belirtilen ID'ye sahip bir rijit cismi fizik dünyasından kaldırır ve serbest bırakır.
 *
 * @param body_id Kaldırılacak rijit cismin ID'si.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_world_destroy_rigid_body(const char* body_id);

/**
 * @brief Bir rijit cismin mevcut konumunu alır.
 *
 * @param body Rijit cisim tutacı.
 * @param out_position Konumun yazılacağı fe_vec3 işaretçisi.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_world_get_rigid_body_position(const fe_chaos_rigid_body_t* body, fe_vec3* out_position);

/**
 * @brief Bir rijit cismin mevcut rotasyonunu alır.
 *
 * @param body Rijit cisim tutacı.
 * @param out_rotation Rotasyonun yazılacağı fe_quat işaretçisi.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_world_get_rigid_body_rotation(const fe_chaos_rigid_body_t* body, fe_quat* out_rotation);

/**
 * @brief Bir rijit cismin mevcut dönüşüm matrisini (konum ve rotasyon) alır.
 *
 * @param body Rijit cisim tutacı.
 * @param out_transform Dönüşümün yazılacağı fe_mat4 işaretçisi.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_world_get_rigid_body_transform(const fe_chaos_rigid_body_t* body, fe_mat4* out_transform);

/**
 * @brief Bir rijit cismin konumunu ayarlar (yalnızca kinematik veya dinamik cisimler için).
 *
 * @param body Rijit cisim tutacı.
 * @param new_position Yeni konum.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_world_set_rigid_body_position(fe_chaos_rigid_body_t* body, fe_vec3 new_position);

/**
 * @brief Bir rijit cismin rotasyonunu ayarlar (yalnızca kinematik veya dinamik cisimler için).
 *
 * @param body Rijit cisim tutacı.
 * @param new_rotation Yeni rotasyon.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_world_set_rigid_body_rotation(fe_chaos_rigid_body_t* body, fe_quat new_rotation);

/**
 * @brief Bir rijit cisme doğrusal bir kuvvet uygular.
 *
 * @param body Rijit cisim tutacı.
 * @param force Uygulanacak kuvvet vektörü.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_world_apply_force(fe_chaos_rigid_body_t* body, fe_vec3 force);

/**
 * @brief Bir rijit cisme tek seferlik bir doğrusal itme uygular.
 *
 * @param body Rijit cisim tutacı.
 * @param impulse Uygulanacak itme vektörü.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_world_apply_impulse(fe_chaos_rigid_body_t* body, fe_vec3 impulse);

/**
 * @brief Bir rijit cisme açısal bir tork uygular.
 *
 * @param body Rijit cisim tutacı.
 * @param torque Uygulanacak tork vektörü.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_world_apply_torque(fe_chaos_rigid_body_t* body, fe_vec3 torque);

/**
 * @brief Bir rijit cisme tek seferlik bir açısal itme uygular.
 *
 * @param body Rijit cisim tutacı.
 * @param angular_impulse Uygulanacak açısal itme vektörü.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_world_apply_angular_impulse(fe_chaos_rigid_body_t* body, fe_vec3 angular_impulse);

/**
 * @brief Belirtilen ID'ye sahip rijit cismi döndürür.
 *
 * @param body_id Rijit cismin ID'si.
 * @return fe_chaos_rigid_body_t* Rijit cisim işaretçisi, bulunamazsa NULL.
 */
fe_chaos_rigid_body_t* fe_chaos_world_get_rigid_body(const char* body_id);

/**
 * @brief Bir çarpışma şekli tanımlayıcısını varsayılan değerlerle doldurur.
 *
 * @param shape Doldurulacak çarpışma şekli.
 */
void fe_chaos_world_default_collision_shape(fe_collision_shape_t* shape);

// Yardımcı Fonksiyonlar (Debug amaçlı veya bilgi için)
const char* fe_physics_body_type_to_string(fe_physics_body_type_t type);
const char* fe_collision_shape_type_to_string(fe_collision_shape_type_t type);


#endif // FE_CHAOS_WORLD_H
