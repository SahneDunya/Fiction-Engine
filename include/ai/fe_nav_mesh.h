#ifndef FE_NAV_MESH_H
#define FE_NAV_MESH_H

#include "core/utils/fe_types.h"
#include "core/math/fe_vec3.h"           // fe_vec3_t for vertices
#include "core/containers/fe_array.h"     // For dynamic arrays of polygons, vertices etc.
#include "core/memory/fe_memory_manager.h" // For FE_MALLOC, FE_FREE, etc.

// Sabitler
#define FE_NAV_MESH_MAX_VERTICES_PER_POLYGON 8 // Bir poligonun maksimum köşe sayısı (genellikle 3 veya 4)
#define FE_NAV_MESH_EPSILON 0.01f              // Yüzen nokta karşılaştırmaları için tolerans

// --- NavMesh Veri Yapıları ---

// Bir poli_gonun köşe indeksleri ve bağlantılarını temsil eder.
typedef struct fe_nav_mesh_polygon {
    uint32_t id;                                // Poli_gonun benzersiz ID'si
    uint32_t vertex_count;                      // Poli_gonu oluşturan köşe sayısı
    uint32_t vertex_indices[FE_NAV_MESH_MAX_VERTICES_PER_POLYGON]; // Köşe dizisindeki indeksler

    // Komşu poli_gonlar ve kenar indeksleri
    // Komşu poli_gon ID'si ve ortak kenarın başlangıç köşe indeksi (vertex_indices dizisinde)
    // Bu, poli_gonlar arası geçiş noktalarını belirlemeye yardımcı olur.
    // Örneğin, neighbors[i] = komşu_id, neighbor_edge_indices[i] = [köşe1_indeksi, köşe2_indeksi]
    uint32_t neighbor_count;
    uint32_t neighbor_poly_ids[FE_NAV_MESH_MAX_VERTICES_PER_POLYGON]; // Komşu poli_gonların ID'leri
    // Her komşu için ortak kenarı tanımlayan köşe indeksleri.
    // Bu indeksler, bu poligonun kendi vertex_indices dizisindeki indekslerdir.
    uint32_t neighbor_edge_vertex_indices[FE_NAV_MESH_MAX_VERTICES_PER_POLYGON][2]; 
    
    fe_vec3_t center;                          // Poli_gonun merkez noktası (yol bulma için hızlandırma)
    float area;                                // Poli_gonun alanı (isteğe bağlı, bazı hesaplamalar için)
} fe_nav_mesh_polygon_t;

// A* algoritması için düğüm durumu
typedef struct fe_nav_mesh_path_node {
    uint32_t polygon_id; // Bu düğümün temsil ettiği poli_gon
    float g_score;       // Başlangıçtan bu poli_gona olan gerçek maliyet
    float h_score;       // Bu poli_gondan hedefe olan tahmini maliyet (sezgisel)
    float f_score;       // g_score + h_score (toplam maliyet)
    uint32_t parent_poly_id; // Bu poli_gona hangi poli_gondan ulaşıldığı
    bool in_open_set;    // Açık kümede mi?
    bool in_closed_set;  // Kapalı kümede mi?
} fe_nav_mesh_path_node_t;

// Navigasyon Ağı ana yapısı
typedef struct fe_nav_mesh {
    fe_array_t vertices; // fe_vec3_t* türünde: Tüm NavMesh'in köşe noktaları
    fe_array_t polygons; // fe_nav_mesh_polygon_t* türünde: Tüm NavMesh poli_gonları
    
    // A* yolu bulma için yardımcı yapılar
    // Hash map veya sıralı dizi olabilir, burada basit dizi kullanıyoruz
    fe_nav_mesh_path_node_t* path_nodes; // Poli_gon ID'sine göre indekslenmiş yol düğümü bilgileri
    size_t path_node_capacity;           // path_nodes dizisinin kapasitesi

} fe_nav_mesh_t;

// --- NavMesh Fonksiyonları ---

/**
 * @brief Yeni bir NavMesh yapısı başlatır.
 * @param nav_mesh Başlatılacak NavMesh yapısının işaretçisi.
 * @param initial_vertex_capacity Başlangıç köşe dizisi kapasitesi.
 * @param initial_polygon_capacity Başlangıç poli_gon dizisi kapasitesi.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_nav_mesh_init(fe_nav_mesh_t* nav_mesh, size_t initial_vertex_capacity, size_t initial_polygon_capacity);

/**
 * @brief Bir NavMesh yapısını ve ilişkili tüm kaynakları serbest bırakır.
 * @param nav_mesh Serbest bırakılacak NavMesh yapısının işaretçisi.
 */
void fe_nav_mesh_destroy(fe_nav_mesh_t* nav_mesh);

/**
 * @brief NavMesh'e yeni bir köşe noktası ekler.
 * @param nav_mesh NavMesh yapısının işaretçisi.
 * @param position Eklenecek köşe noktasının konumu.
 * @return uint32_t Eklenen köşe noktasının indeksi, hata durumunda FE_INVALID_ID.
 */
uint32_t fe_nav_mesh_add_vertex(fe_nav_mesh_t* nav_mesh, fe_vec3_t position);

/**
 * @brief NavMesh'e yeni bir poli_gon ekler.
 * Poli_gonun köşe indeksleri ve komşu poli_gon bağlantıları önceden hesaplanmış olmalıdır.
 * Bu fonksiyon poli_gon verisini kopyalar.
 * @param nav_mesh NavMesh yapısının işaretçisi.
 * @param polygon Eklenecek poli_gon verisi.
 * @return uint32_t Eklenen poli_gonun ID'si, hata durumunda FE_INVALID_ID.
 */
uint32_t fe_nav_mesh_add_polygon(fe_nav_mesh_t* nav_mesh, const fe_nav_mesh_polygon_t* polygon);

/**
 * @brief NavMesh'i köşe ve poli_gon verileriyle doldurur.
 * Bu genellikle bir dosya yükleme veya runtime oluşturma sürecinden sonra çağrılır.
 * Poli_gonlar arasındaki komşuluk ilişkilerini de kurar.
 * @param nav_mesh NavMesh yapısının işaretçisi.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_nav_mesh_build_connections(fe_nav_mesh_t* nav_mesh);

/**
 * @brief Belirli bir noktanın hangi NavMesh poli_gonunun içinde olduğunu bulur.
 * @param nav_mesh NavMesh yapısının işaretçisi.
 * @param point Kontrol edilecek 3D nokta.
 * @param polygon_id_out Noktayı içeren poli_gonun ID'sinin yazılacağı işaretçi.
 * @return bool Nokta bir poli_gon içinde bulunursa true, aksi takdirde false.
 */
bool fe_nav_mesh_find_polygon_for_point(const fe_nav_mesh_t* nav_mesh, fe_vec3_t point, uint32_t* polygon_id_out);

/**
 * @brief NavMesh üzerinde başlangıç noktasından hedef noktasına giden bir yol hesaplar.
 * @param nav_mesh NavMesh yapısının işaretçisi.
 * @param start_pos Başlangıç noktası.
 * @param end_pos Hedef noktası.
 * @param out_path_polygons Bulunan yolu oluşturan poli_gon ID'lerinin doldurulacağı fe_array_t (uint32_t*).
 * @return bool Yol bulunursa true, aksi takdirde false.
 */
bool fe_nav_mesh_find_path(fe_nav_mesh_t* nav_mesh, fe_vec3_t start_pos, fe_vec3_t end_pos, fe_array_t* out_path_polygons);

/**
 * @brief Poli_gon yolunu, AI'ın takip edebileceği bir dizi ara noktaya (steering points) dönüştürür.
 * Funnel algoritmasını kullanır.
 * @param nav_mesh NavMesh yapısının işaretçisi.
 * @param path_polygons fe_nav_mesh_find_path tarafından bulunan poli_gon ID'leri dizisi.
 * @param start_pos Yolun başlangıç noktası.
 * @param end_pos Yolun hedef noktası.
 * @param out_steering_points Oluşturulan ara noktaların doldurulacağı fe_array_t (fe_vec3_t*).
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_nav_mesh_smooth_path(const fe_nav_mesh_t* nav_mesh, const fe_array_t* path_polygons,
                              fe_vec3_t start_pos, fe_vec3_t end_pos, fe_array_t* out_steering_points);

// --- Yardımcı Fonksiyonlar (Dahili veya Harici Kullanım İçin) ---

/**
 * @brief Bir poli_gonun merkezini hesaplar.
 * @param nav_mesh NavMesh yapısının işaretçisi.
 * @param polygon Hesaplaması yapılacak poli_gon.
 * @return fe_vec3_t Poli_gonun merkezi.
 */
fe_vec3_t fe_nav_mesh_polygon_calculate_center(const fe_nav_mesh_t* nav_mesh, const fe_nav_mesh_polygon_t* polygon);

/**
 * @brief Poli_gonun normalini hesaplar (sadece 3D NavMesh'ler için önemlidir).
 * @param nav_mesh NavMesh yapısının işaretçisi.
 * @param polygon Hesaplaması yapılacak poli_gon.
 * @return fe_vec3_t Poli_gonun normali.
 */
fe_vec3_t fe_nav_mesh_polygon_calculate_normal(const fe_nav_mesh_t* nav_mesh, const fe_nav_mesh_polygon_t* polygon);

#endif // FE_NAV_MESH_H
