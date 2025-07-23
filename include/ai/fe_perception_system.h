#ifndef FE_PERCEPTION_SYSTEM_H
#define FE_PERCEPTION_SYSTEM_H

#include "core/utils/fe_types.h"
#include "core/math/fe_vec3.h"
#include "core/containers/fe_array.h"     // Dinamik diziler için
#include "core/memory/fe_memory_manager.h" // Bellek yönetimi için

// --- Algılama Tipleri (Enums) ---
typedef enum fe_perception_type {
    FE_PERCEPTION_TYPE_NONE = 0,
    FE_PERCEPTION_TYPE_VISUAL,  // Görsel algılama (görme, görüş alanı)
    FE_PERCEPTION_TYPE_AUDITORY, // İşitsel algılama (sesler)
    FE_PERCEPTION_TYPE_PROXIMITY // Yakınlık algılaması (çarpışma, mesafeye dayalı)
    // Diğerleri: Koku, Dokunma vb.
} fe_perception_type_t;

// --- Algılanan Nesne Verisi ---
// Bir AI ajanı tarafından algılanan bir oyun nesnesi hakkında bilgi
typedef struct fe_perceived_object {
    uint32_t entity_id;         // Algılanan varlığın ID'si
    fe_vec3_t position;         // Algılanan varlığın pozisyonu
    fe_vec3_t last_known_position; // Son bilinen pozisyon (görüş kaybolursa)
    fe_perception_type_t type;  // Hangi algılama tipiyle algılandığı
    float strength;             // Algılama gücü (örn. ses şiddeti, görüş netliği)
    float distance;             // Algılayıcıdan uzaklık
    uint32_t timestamp_ms;      // Ne zaman algılandığı (milisaniye)
    bool is_hostile;            // Düşman mı? (AI'ın bağlamına göre değişir)
    // Diğer özellikler eklenebilir: is_hidden, is_alerted, object_type_id etc.
} fe_perceived_object_t;

// --- Algılanabilir Bileşen (Component) ---
// Bir oyun nesnesinin algılanabilir olmasını sağlayan veri
typedef struct fe_perceivable_component {
    uint32_t entity_id;         // Bu bileşenin ait olduğu varlık ID'si
    fe_vec3_t position;         // Dünya konumu (update edilmesi gerekir)
    float visual_radius;        // Görsel olarak algılanabilir yarıçap
    float auditory_strength;    // Yaydığı sesin gücü (maksimum algılama mesafesi için)
    bool is_visible;            // Şu anda görünür mü? (oyun mantığı tarafından ayarlanır)
    bool is_active;             // Algılama sistemi tarafından işlenmeli mi?
    // Diğer özellikler eklenebilir: is_stealthed, is_making_noise_actively etc.
} fe_perceivable_component_t;

// --- Algılayıcı Bileşen (Perceiver Component) ---
// Bir AI ajanının algılama yeteneğini temsil eden veri
typedef struct fe_perceiver_component {
    uint32_t entity_id;         // Bu bileşenin ait olduğu varlık ID'si (AI ajanı)
    fe_vec3_t position;         // Dünya konumu (AI ajanı pozisyonu)
    fe_vec3_t forward_dir;      // İleri yön vektörü (görüş alanı için)
    float field_of_view_angle_rad; // Görüş alanı açısı (radyan cinsinden, örn. PI/2 = 90 derece)
    float view_distance;        // Maksimum görüş mesafesi
    float hearing_distance;     // Maksimum işitme mesafesi
    float perception_update_interval_ms; // Algılama güncellemeleri arasındaki süre
    uint32_t last_update_time_ms; // Son algılama güncelleme zamanı

    fe_array_t perceived_objects; // fe_perceived_object_t*: Algılanan nesnelerin listesi
    // Bu liste her algılama döngüsünde yeniden oluşturulabilir veya güncellenebilir.

    // Callback fonksiyon işaretçileri (isteğe bağlı, oyun mantığına bildirim için)
    // PFN_on_object_perceived on_perceived_callback;
    // PFN_on_object_lost on_lost_callback;
} fe_perceiver_component_t;

// --- Algılama Sistemi Ana Yapısı ---
typedef struct fe_perception_system {
    fe_array_t perceivers;      // fe_perceiver_component_t*: Tüm algılayıcı ajanlar
    fe_array_t perceivables;    // fe_perceivable_component_t*: Tüm algılanabilir nesneler

    // Hızlandırma için uzamsal veri yapısı (örn. Quadtree/Octree)
    // struct fe_spatial_partition_tree* spatial_tree;

    // Engel kontrolü için çarpışma sistemi referansı
    // struct fe_collision_system* collision_system;

    uint32_t current_game_time_ms; // Oyunun mevcut zamanı
} fe_perception_system_t;

// --- Algılama Sistemi Fonksiyonları ---

/**
 * @brief Algılama sistemini başlatır.
 * @param system Başlatılacak sistemin işaretçisi.
 * @param initial_perceiver_capacity Başlangıç algılayıcı kapasitesi.
 * @param initial_perceivable_capacity Başlangıç algılanabilir kapasitesi.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_perception_system_init(fe_perception_system_t* system, 
                               size_t initial_perceiver_capacity, 
                               size_t initial_perceivable_capacity);

/**
 * @brief Algılama sistemini ve ilişkili tüm kaynakları serbest bırakır.
 * @param system Serbest bırakılacak sistemin işaretçisi.
 */
void fe_perception_system_destroy(fe_perception_system_t* system);

/**
 * @brief Algılama sistemine yeni bir algılayıcı bileşen ekler.
 * @param system Sistem işaretçisi.
 * @param perceiver Eklenecek algılayıcı bileşen verisi. Veri kopyalanır.
 * @return fe_perceiver_component_t* Eklenen bileşenin işaretçisi veya hata durumunda NULL.
 */
fe_perceiver_component_t* fe_perception_system_add_perceiver(fe_perception_system_t* system, const fe_perceiver_component_t* perceiver);

/**
 * @brief Algılama sistemine yeni bir algılanabilir bileşen ekler.
 * @param system Sistem işaretçisi.
 * @param perceivable Eklenecek algılanabilir bileşen verisi. Veri kopyalanır.
 * @return fe_perceivable_component_t* Eklenen bileşenin işaretçisi veya hata durumunda NULL.
 */
fe_perceivable_component_t* fe_perception_system_add_perceivable(fe_perception_system_t* system, const fe_perceivable_component_t* perceivable);

/**
 * @brief Algılayıcı bileşeni günceller (pozisyon, yön vb.).
 * @param perceiver_component Güncellenecek perceiver bileşeni.
 * @param new_pos Yeni pozisyon.
 * @param new_forward_dir Yeni ileri yön vektörü (normalize edilmiş olmalı).
 */
void fe_perceiver_component_update(fe_perceiver_component_t* perceiver_component, fe_vec3_t new_pos, fe_vec3_t new_forward_dir);

/**
 * @brief Algılanabilir bileşeni günceller (pozisyon, görünürlük vb.).
 * @param perceivable_component Güncellenecek perceivable bileşeni.
 * @param new_pos Yeni pozisyon.
 * @param is_visible Yeni görünürlük durumu.
 */
void fe_perceivable_component_update(fe_perceivable_component_t* perceivable_component, fe_vec3_t new_pos, bool is_visible);


/**
 * @brief Algılama sistemini günceller (her oyun tick'inde çağrılır).
 * Tüm algılayıcılar için algılama sorgularını tetikler.
 * @param system Sistem işaretçisi.
 * @param delta_time_ms Son güncellemeden bu yana geçen süre (milisaniye).
 * @param current_game_time_ms Oyunun mevcut genel zamanı (milisaniye).
 */
void fe_perception_system_update(fe_perception_system_t* system, uint32_t delta_time_ms, uint32_t current_game_time_ms);

/**
 * @brief Belirli bir algılayıcının algıladığı nesnelerin güncel listesini döndürür.
 * @param perceiver Algılayıcı bileşeninin işaretçisi.
 * @return const fe_array_t* Algılanan nesnelerin listesi.
 */
const fe_array_t* fe_perceiver_get_perceived_objects(const fe_perceiver_component_t* perceiver);

// --- Dahili Yardımcı Fonksiyonlar (Implementasyonda kullanılacak) ---

/**
 * @brief İki nokta arasında görüş hattı engeli olup olmadığını kontrol eder.
 * @param system Algılama sistemi.
 * @param p1 Başlangıç noktası.
 * @param p2 Bitiş noktası.
 * @param exclude_entity_id Kontrol sırasında hariç tutulacak varlık ID'si (genellikle algılayıcının kendisi).
 * @return bool Engel varsa true, yoksa false.
 * NOT: Bu fonksiyon, bir çarpışma sistemi entegrasyonuna ihtiyaç duyar.
 * Basitlik için şu an varsayımsal bir uygulamaya sahiptir.
 */
bool fe_perception_system_check_line_of_sight(const fe_perception_system_t* system, fe_vec3_t p1, fe_vec3_t p2, uint32_t exclude_entity_id);

/**
 * @brief İki 3D vektör arasındaki açıyı radyan cinsinden hesaplar.
 * @param v1 Birinci vektör.
 * @param v2 İkinci vektör.
 * @return float Açı (radyan).
 */
float fe_vec3_angle_between(fe_vec3_t v1, fe_vec3_t v2);


#endif // FE_PERCEPTION_SYSTEM_H
