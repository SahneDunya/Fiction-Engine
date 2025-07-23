#ifndef FE_PATHFINDER_H
#define FE_PATHFINDER_H

#include "core/utils/fe_types.h"
#include "core/math/fe_vec3.h"
#include "core/containers/fe_array.h" // fe_array_t için
#include "navigation/fe_nav_mesh.h"   // fe_nav_mesh_t için

// --- Yol Durumu (Path Status) ---
typedef enum fe_path_status {
    FE_PATH_STATUS_NONE = 0,     // Yol bulunamadı veya henüz bir sorgu yapılmadı.
    FE_PATH_STATUS_COMPUTING,    // Yol hesaplanıyor (asenkron pathfinding için).
    FE_PATH_STATUS_SUCCESS,      // Yol başarıyla bulundu.
    FE_PATH_STATUS_FAILURE_NO_PATH, // Başlangıç veya bitiş noktası geçerli değil veya yol yok.
    FE_PATH_STATUS_FAILURE_INVALID_ARGS, // Geçersiz argümanlar nedeniyle başarısız.
    FE_PATH_STATUS_COMPLETED     // Yol takip edildi ve tamamlandı.
} fe_path_status_t;

// --- Yol Veri Yapısı (Path Data Structure) ---
// Bir ajan tarafından kullanılacak aktif yolu temsil eder.
typedef struct fe_path {
    fe_array_t steering_points; // fe_vec3_t*: AI'ın takip edeceği ara noktalar (smooth path).
    fe_path_status_t status;    // Mevcut yolun durumu.
    uint32_t current_point_idx; // Ajanın şu anda hedeflediği steering point'in indeksi.
    fe_vec3_t start_pos;        // Yolun başlangıç pozisyonu.
    fe_vec3_t end_pos;          // Yolun hedef pozisyonu.
    uint32_t agent_id;          // Bu yolu kullanan ajanın ID'si (isteğe bağlı, izleme için).
} fe_path_t;

// --- Pathfinder Ana Yapısı ---
// Tüm yol bulma sistemi için ana arayüz.
typedef struct fe_pathfinder {
    fe_nav_mesh_t* nav_mesh; // Kullanılacak NavMesh'in işaretçisi.
    // Gelecekte eklenebilecek diğer özellikler:
    // fe_array_t active_paths; // fe_path_t*: Aynı anda birden fazla ajanın yolunu yönetmek için.
    // fe_thread_pool_t* thread_pool; // Asenkron yol bulma için.
} fe_pathfinder_t;

// --- Pathfinder Fonksiyonları ---

/**
 * @brief Pathfinder sistemini başlatır.
 * @param pathfinder Başlatılacak pathfinder yapısının işaretçisi.
 * @param nav_mesh Yol bulma işlemleri için kullanılacak NavMesh'in işaretçisi.
 * NavMesh daha önceden başlatılmış ve bağlantıları kurulmuş olmalıdır.
 * Pathfinder NavMesh'in sahipliğini almaz, sadece bir referans tutar.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_pathfinder_init(fe_pathfinder_t* pathfinder, fe_nav_mesh_t* nav_mesh);

/**
 * @brief Pathfinder sistemini ve ilişkili tüm kaynakları serbest bırakır.
 * Pathfinder'ın yönettiği aktif yolları da serbest bırakır.
 * @param pathfinder Serbest bırakılacak pathfinder yapısının işaretçisi.
 */
void fe_pathfinder_destroy(fe_pathfinder_t* pathfinder);

/**
 * @brief Yeni bir yol sorgusu başlatır ve bulunan yolu fe_path_t yapısına doldurur.
 * Bu fonksiyon senkron çalışır. Asenkron yol bulma için farklı bir arayüz gerekir.
 *
 * @param pathfinder Pathfinder yapısının işaretçisi.
 * @param start_pos Yolun başlangıç 3D konumu.
 * @param end_pos Yolun hedef 3D konumu.
 * @param agent_id Bu yolu talep eden ajanın ID'si (isteğe bağlı).
 * @param out_path Oluşturulan yol verisinin doldurulacağı fe_path_t işaretçisi.
 * Bu yapının fe_path_init_empty() ile başlatılması ve fe_path_destroy() ile serbest bırakılması gerekir.
 * @return fe_path_status_t Yol bulma işleminin durumu.
 */
fe_path_status_t fe_pathfinder_find_path(fe_pathfinder_t* pathfinder, 
                                         fe_vec3_t start_pos, fe_vec3_t end_pos, 
                                         uint32_t agent_id, fe_path_t* out_path);

/**
 * @brief Bir fe_path_t yapısını başlatır ve temizler.
 * @param path Başlatılacak yol yapısının işaretçisi.
 * @param agent_id Yolu kullanacak ajanın ID'si.
 */
void fe_path_init_empty(fe_path_t* path, uint32_t agent_id);

/**
 * @brief Bir fe_path_t yapısının içerdiği kaynakları serbest bırakır.
 * @param path Serbest bırakılacak yol yapısının işaretçisi.
 */
void fe_path_destroy(fe_path_t* path);

/**
 * @brief Yoldaki bir sonraki hedef noktayı döndürür.
 * Ajan bu noktaya ulaştığında bu fonksiyon tekrar çağrılmalıdır.
 *
 * @param path Yol yapısının işaretçisi.
 * @param current_agent_pos Ajanın mevcut konumu.
 * @param tolerance Ajanın hedef noktaya ne kadar yakın sayılacağı.
 * @param out_next_point Bir sonraki hedef noktanın doldurulacağı işaretçi.
 * @return bool Bir sonraki nokta varsa true, yol tamamlanmışsa veya geçersizse false.
 */
bool fe_path_get_next_point(fe_path_t* path, fe_vec3_t current_agent_pos, float tolerance, fe_vec3_t* out_next_point);

/**
 * @brief Yolun tamamlanıp tamamlanmadığını kontrol eder.
 * @param path Yol yapısının işaretçisi.
 * @return bool Yol tamamlanmışsa true, aksi takdirde false.
 */
bool fe_path_is_completed(const fe_path_t* path);

/**
 * @brief Yolun durumunu string olarak döndürür (debugging için).
 * @param status Durum enum değeri.
 * @return const char* Durumun string karşılığı.
 */
const char* fe_path_status_to_string(fe_path_status_t status);

#endif // FE_PATHFINDER_H
