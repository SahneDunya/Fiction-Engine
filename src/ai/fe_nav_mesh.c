#include "navigation/fe_nav_mesh.h"
#include "core/utils/fe_logger.h"
#include "core/math/fe_math.h" // For fe_vec3_dist_sq, fe_vec3_cross, fe_vec3_normalize etc.
#include "core/containers/fe_heap.h" // A* için min-heap (öncelik kuyruğu)
#include <float.h> // For FLT_MAX
#include <string.h> // For memset, memcpy

// --- Dahili Yardımcı Fonksiyonlar ve Makrolar ---

// Poli_gonlar arasındaki kenarın benzersiz bir anahtarını oluşturur
// Köşe indekslerinin sırası önemli değil
#define EDGE_KEY(v1_idx, v2_idx) ((v1_idx < v2_idx) ? ((uint64_t)v1_idx << 32 | v2_idx) : ((uint64_t)v2_idx << 32 | v1_idx))

// Poli_gon AABB hesaplama (sadeleştirilmiş, sadece 2D düşünerek)
static void fe_nav_mesh_polygon_calculate_aabb(const fe_nav_mesh_t* nav_mesh, const fe_nav_mesh_polygon_t* polygon, fe_vec3_t* min_bounds, fe_vec3_t* max_bounds) {
    if (polygon->vertex_count == 0) return;

    *min_bounds = *(fe_vec3_t*)fe_array_get_at(&nav_mesh->vertices, polygon->vertex_indices[0]);
    *max_bounds = *min_bounds;

    for (uint32_t i = 1; i < polygon->vertex_count; ++i) {
        fe_vec3_t v = *(fe_vec3_t*)fe_array_get_at(&nav_mesh->vertices, polygon->vertex_indices[i]);
        min_bounds->x = FE_MIN(min_bounds->x, v.x);
        min_bounds->y = FE_MIN(min_bounds->y, v.y);
        min_bounds->z = FE_MIN(min_bounds->z, v.z);
        max_bounds->x = FE_MAX(max_bounds->x, v.x);
        max_bounds->y = FE_MAX(max_bounds->y, v.y);
        max_bounds->z = FE_MAX(max_bounds->z, v.z);
    }
}

// Nokta-içi-poli_gon testi (sadece Y ekseninde düzlem düşünülüyor, 2D projeksiyon gibi)
// Ray casting algoritması
static bool fe_nav_mesh_is_point_in_polygon_2d(const fe_nav_mesh_t* nav_mesh, const fe_nav_mesh_polygon_t* polygon, fe_vec3_t point) {
    if (polygon->vertex_count < 3) return false; // En az üçgen olmalı

    int intersections = 0;
    float p_x = point.x;
    float p_z = point.z; // Y eksenini yukarı varsayıyoruz, bu yüzden XZ düzlemi
    
    // Köşeleri al
    fe_vec3_t polygon_vertices[FE_NAV_MESH_MAX_VERTICES_PER_POLYGON];
    for(uint32_t i = 0; i < polygon->vertex_count; ++i) {
        polygon_vertices[i] = *(fe_vec3_t*)fe_array_get_at(&nav_mesh->vertices, polygon->vertex_indices[i]);
    }

    for (uint32_t i = 0; i < polygon->vertex_count; i++) {
        uint32_t j = (i + 1) % polygon->vertex_count;
        
        float v_ix = polygon_vertices[i].x;
        float v_iz = polygon_vertices[i].z;
        float v_jx = polygon_vertices[j].x;
        float v_jz = polygon_vertices[j].z;

        // Noktanın köşelerden biri olup olmadığını kontrol et
        if ((fabsf(v_ix - p_x) < FE_NAV_MESH_EPSILON && fabsf(v_iz - p_z) < FE_NAV_MESH_EPSILON) ||
            (fabsf(v_jx - p_x) < FE_NAV_MESH_EPSILON && fabsf(v_jz - p_z) < FE_NAV_MESH_EPSILON)) {
            return true; // Nokta bir köşenin üzerinde
        }
        
        // Yatay bir ışın çizgisinin bir kenarı kesip kesmediğini kontrol et
        // Kaynak: https://wrf.ecse.rpi.edu//Classes/GamePhysics/Lectures/14_Pathfinding.pdf (sayfa 18)
        if (((v_iz <= p_z && v_jz > p_z) || (v_jz <= p_z && v_iz > p_z)) &&
            (p_x < (v_jx - v_ix) * (p_z - v_iz) / (v_jz - v_iz) + v_ix)) {
            intersections++;
        }
    }

    return (intersections % 2 == 1); // Tek sayı kesişim = içinde
}

// Bir poli_gonun merkezini hesaplar.
fe_vec3_t fe_nav_mesh_polygon_calculate_center(const fe_nav_mesh_t* nav_mesh, const fe_nav_mesh_polygon_t* polygon) {
    fe_vec3_t center = FE_VEC3_ZERO;
    if (!nav_mesh || !polygon || polygon->vertex_count == 0) {
        return center;
    }

    for (uint32_t i = 0; i < polygon->vertex_count; ++i) {
        center = fe_vec3_add(center, *(fe_vec3_t*)fe_array_get_at(&nav_mesh->vertices, polygon->vertex_indices[i]));
    }
    return fe_vec3_div_scalar(center, (float)polygon->vertex_count);
}

// Poli_gon normalini hesaplar (ilk 3 köşe kullanılarak)
fe_vec3_t fe_nav_mesh_polygon_calculate_normal(const fe_nav_mesh_t* nav_mesh, const fe_nav_mesh_polygon_t* polygon) {
    if (polygon->vertex_count < 3) {
        return FE_VEC3_ZERO; // Üçgenden az köşe normal hesaplayamaz
    }

    fe_vec3_t v0 = *(fe_vec3_t*)fe_array_get_at(&nav_mesh->vertices, polygon->vertex_indices[0]);
    fe_vec3_t v1 = *(fe_vec3_t*)fe_array_get_at(&nav_mesh->vertices, polygon->vertex_indices[1]);
    fe_vec3_t v2 = *(fe_vec3_t*)fe_array_get_at(&nav_mesh->vertices, polygon->vertex_indices[2]);

    fe_vec3_t edge1 = fe_vec3_sub(v1, v0);
    fe_vec3_t edge2 = fe_vec3_sub(v2, v0);

    return fe_vec3_normalize(fe_vec3_cross(edge1, edge2));
}

// A* algoritması için min-heap karşılaştırma fonksiyonu (f_score'a göre)
static int fe_nav_mesh_path_node_compare(const void* a, const void* b) {
    const fe_nav_mesh_path_node_t* node_a = (const fe_nav_mesh_path_node_t*)a;
    const fe_nav_mesh_path_node_t* node_b = (const fe_nav_mesh_path_node_t*)b;

    if (node_a->f_score < node_b->f_score) return -1;
    if (node_a->f_score > node_b->f_score) return 1;
    return 0;
}

// --- NavMesh Fonksiyon Implementasyonları ---

bool fe_nav_mesh_init(fe_nav_mesh_t* nav_mesh, size_t initial_vertex_capacity, size_t initial_polygon_capacity) {
    if (!nav_mesh) {
        FE_LOG_ERROR("fe_nav_mesh_init: NavMesh pointer is NULL.");
        return false;
    }

    memset(nav_mesh, 0, sizeof(fe_nav_mesh_t));

    if (!fe_array_init(&nav_mesh->vertices, sizeof(fe_vec3_t), initial_vertex_capacity, FE_MEM_TYPE_NAV_MESH_VERTICES)) {
        FE_LOG_CRITICAL("fe_nav_mesh_init: Failed to initialize vertices array.");
        return false;
    }
    fe_array_set_capacity(&nav_mesh->vertices, initial_vertex_capacity);

    if (!fe_array_init(&nav_mesh->polygons, sizeof(fe_nav_mesh_polygon_t), initial_polygon_capacity, FE_MEM_TYPE_NAV_MESH_POLYGONS)) {
        FE_LOG_CRITICAL("fe_nav_mesh_init: Failed to initialize polygons array.");
        fe_array_destroy(&nav_mesh->vertices);
        return false;
    }
    fe_array_set_capacity(&nav_mesh->polygons, initial_polygon_capacity);

    // Path nodes için hafıza ayır, maksimum poligon sayısı kadar.
    nav_mesh->path_node_capacity = initial_polygon_capacity;
    nav_mesh->path_nodes = (fe_nav_mesh_path_node_t*)FE_MALLOC(sizeof(fe_nav_mesh_path_node_t) * nav_mesh->path_node_capacity, FE_MEM_TYPE_NAV_MESH_PATH_NODES);
    if (!nav_mesh->path_nodes) {
        FE_LOG_CRITICAL("fe_nav_mesh_init: Failed to allocate path nodes array.");
        fe_array_destroy(&nav_mesh->vertices);
        fe_array_destroy(&nav_mesh->polygons);
        return false;
    }
    FE_LOG_INFO("NavMesh initialized with vertex capacity %zu, polygon capacity %zu.", initial_vertex_capacity, initial_polygon_capacity);
    return true;
}

void fe_nav_mesh_destroy(fe_nav_mesh_t* nav_mesh) {
    if (!nav_mesh) return;

    FE_LOG_INFO("Destroying NavMesh.");

    if (nav_mesh->path_nodes) {
        FE_FREE(nav_mesh->path_nodes, FE_MEM_TYPE_NAV_MESH_PATH_NODES);
        nav_mesh->path_nodes = NULL;
    }

    fe_array_destroy(&nav_mesh->polygons);
    fe_array_destroy(&nav_mesh->vertices);

    memset(nav_mesh, 0, sizeof(fe_nav_mesh_t)); // Yapıyı sıfırla
}

uint32_t fe_nav_mesh_add_vertex(fe_nav_mesh_t* nav_mesh, fe_vec3_t position) {
    if (!nav_mesh) {
        FE_LOG_ERROR("fe_nav_mesh_add_vertex: NavMesh is NULL.");
        return FE_INVALID_ID;
    }
    uint32_t index = fe_array_get_size(&nav_mesh->vertices);
    if (!fe_array_add_element(&nav_mesh->vertices, &position)) {
        FE_LOG_ERROR("fe_nav_mesh_add_vertex: Failed to add vertex.");
        return FE_INVALID_ID;
    }
    return index;
}

uint32_t fe_nav_mesh_add_polygon(fe_nav_mesh_t* nav_mesh, const fe_nav_mesh_polygon_t* polygon) {
    if (!nav_mesh || !polygon) {
        FE_LOG_ERROR("fe_nav_mesh_add_polygon: NavMesh or polygon is NULL.");
        return FE_INVALID_ID;
    }
    // Yeni poli_gona benzersiz bir ID ata (dizideki sırasını kullan)
    uint32_t new_id = fe_array_get_size(&nav_mesh->polygons);
    
    fe_nav_mesh_polygon_t new_polygon = *polygon; // Kopyasını al
    new_polygon.id = new_id; // ID'yi ayarla
    new_polygon.center = fe_nav_mesh_polygon_calculate_center(nav_mesh, &new_polygon); // Merkezi hesapla

    if (!fe_array_add_element(&nav_mesh->polygons, &new_polygon)) {
        FE_LOG_ERROR("fe_nav_mesh_add_polygon: Failed to add polygon.");
        return FE_INVALID_ID;
    }
    return new_id;
}


bool fe_nav_mesh_build_connections(fe_nav_mesh_t* nav_mesh) {
    if (!nav_mesh) {
        FE_LOG_ERROR("fe_nav_mesh_build_connections: NavMesh is NULL.");
        return false;
    }

    FE_LOG_INFO("Building NavMesh connections...");

    // Poli_gonları döngüye al
    size_t num_polygons = fe_array_get_size(&nav_mesh->polygons);
    for (size_t i = 0; i < num_polygons; ++i) {
        fe_nav_mesh_polygon_t* poly_i = (fe_nav_mesh_polygon_t*)fe_array_get_at(&nav_mesh->polygons, i);
        poly_i->neighbor_count = 0; // Komşu sayısını sıfırla

        // Her bir poli_gonun her bir kenarını kontrol et
        for (uint32_t v_idx1 = 0; v_idx1 < poly_i->vertex_count; ++v_idx1) {
            uint32_t v_idx2 = (v_idx1 + 1) % poly_i->vertex_count; // Sonraki köşe
            uint32_t edge_v1_global_idx = poly_i->vertex_indices[v_idx1];
            uint32_t edge_v2_global_idx = poly_i->vertex_indices[v_idx2];

            // Diğer tüm poli_gonlarla karşılaştır
            for (size_t j = i + 1; j < num_polygons; ++j) {
                fe_nav_mesh_polygon_t* poly_j = (fe_nav_mesh_polygon_t*)fe_array_get_at(&nav_mesh->polygons, j);

                // Ortak kenar bul
                uint32_t common_vertex_count = 0;
                uint32_t common_v_indices[2] = {FE_INVALID_ID, FE_INVALID_ID};

                for (uint32_t pv_idx1 = 0; pv_idx1 < poly_j->vertex_count; ++pv_idx1) {
                    uint32_t pv_idx2 = (pv_idx1 + 1) % poly_j->vertex_count;
                    uint32_t p_edge_v1_global_idx = poly_j->vertex_indices[pv_idx1];
                    uint32_t p_edge_v2_global_idx = poly_j->vertex_indices[pv_idx2];

                    // İki kenarın aynı köşeleri paylaşıp paylaşmadığını kontrol et
                    bool match_v1_v1 = (edge_v1_global_idx == p_edge_v1_global_idx);
                    bool match_v1_v2 = (edge_v1_global_idx == p_edge_v2_global_idx);
                    bool match_v2_v1 = (edge_v2_global_idx == p_edge_v1_global_idx);
                    bool match_v2_v2 = (edge_v2_global_idx == p_edge_v2_global_idx);

                    if ((match_v1_v1 && match_v2_v2) || (match_v1_v2 && match_v2_v1)) {
                        // Ortak kenar bulundu!
                        // Poli_gon i için
                        poly_i->neighbor_poly_ids[poly_i->neighbor_count] = poly_j->id;
                        poly_i->neighbor_edge_vertex_indices[poly_i->neighbor_count][0] = v_idx1;
                        poly_i->neighbor_edge_vertex_indices[poly_i->neighbor_count][1] = v_idx2;
                        poly_i->neighbor_count++;

                        // Poli_gon j için
                        poly_j->neighbor_poly_ids[poly_j->neighbor_count] = poly_i->id;
                        poly_j->neighbor_edge_vertex_indices[poly_j->neighbor_count][0] = pv_idx1;
                        poly_j->neighbor_edge_vertex_indices[poly_j->neighbor_count][1] = pv_idx2;
                        poly_j->neighbor_count++;
                        break; // Bu poli_gon (j) için sonraki poli_gonu kontrol et
                    }
                }
            }
        }
    }
    FE_LOG_INFO("NavMesh connections built for %zu polygons.", num_polygons);
    return true;
}


bool fe_nav_mesh_find_polygon_for_point(const fe_nav_mesh_t* nav_mesh, fe_vec3_t point, uint32_t* polygon_id_out) {
    if (!nav_mesh || !polygon_id_out) {
        FE_LOG_ERROR("fe_nav_mesh_find_polygon_for_point: Invalid arguments.");
        return false;
    }

    // Basit bir yaklaşım: Tüm poli_gonları kontrol et.
    // Daha büyük ağlar için uzamsal bölümlendirme (örn. KD-tree, Quadtree) kullanılmalıdır.
    size_t num_polygons = fe_array_get_size(&nav_mesh->polygons);
    for (size_t i = 0; i < num_polygons; ++i) {
        fe_nav_mesh_polygon_t* polygon = (fe_nav_mesh_polygon_t*)fe_array_get_at(&nav_mesh->polygons, i);
        // Önce AABB (Bounding Box) testi yap, daha hızlı bir reddetme için
        fe_vec3_t min_b, max_b;
        fe_nav_mesh_polygon_calculate_aabb(nav_mesh, polygon, &min_b, &max_b);
        if (point.x >= min_b.x && point.x <= max_b.x &&
            point.y >= min_b.y && point.y <= max_b.y && // Y ekseni için de kontrol et
            point.z >= min_b.z && point.z <= max_b.z) {

            // AABB içinde ise, daha pahalı olan nokta-içi-poli_gon testini yap
            if (fe_nav_mesh_is_point_in_polygon_2d(nav_mesh, polygon, point)) {
                *polygon_id_out = polygon->id;
                FE_LOG_DEBUG("Point (%.2f,%.2f,%.2f) found in polygon ID %u.", point.x, point.y, point.z, *polygon_id_out);
                return true;
            }
        }
    }

    *polygon_id_out = FE_INVALID_ID;
    FE_LOG_WARN("Point (%.2f,%.2f,%.2f) not found in any NavMesh polygon.", point.x, point.y, point.z);
    return false;
}

bool fe_nav_mesh_find_path(fe_nav_mesh_t* nav_mesh, fe_vec3_t start_pos, fe_vec3_t end_pos, fe_array_t* out_path_polygons) {
    if (!nav_mesh || !out_path_polygons) {
        FE_LOG_ERROR("fe_nav_mesh_find_path: Invalid arguments (NavMesh or out_path_polygons is NULL).");
        return false;
    }

    fe_array_clear(out_path_polygons); // Önceki yolu temizle

    uint32_t start_poly_id = FE_INVALID_ID;
    uint32_t end_poly_id = FE_INVALID_ID;

    if (!fe_nav_mesh_find_polygon_for_point(nav_mesh, start_pos, &start_poly_id) || start_poly_id == FE_INVALID_ID) {
        FE_LOG_ERROR("fe_nav_mesh_find_path: Start position not found in any polygon.");
        return false;
    }
    if (!fe_nav_mesh_find_polygon_for_point(nav_mesh, end_pos, &end_poly_id) || end_poly_id == FE_INVALID_ID) {
        FE_LOG_ERROR("fe_nav_mesh_find_path: End position not found in any polygon.");
        return false;
    }

    if (start_poly_id == end_poly_id) {
        FE_LOG_INFO("Start and end points are in the same polygon (%u). Path found.", start_poly_id);
        fe_array_add_element(out_path_polygons, &start_poly_id);
        return true;
    }

    // A* Algoritması
    // Hazırlık: path_nodes dizisini sıfırla
    for (size_t i = 0; i < fe_array_get_size(&nav_mesh->polygons); ++i) {
        fe_nav_mesh_path_node_t* node_info = &nav_mesh->path_nodes[i];
        node_info->polygon_id = ((fe_nav_mesh_polygon_t*)fe_array_get_at(&nav_mesh->polygons, i))->id; // Polygon ID'sini doğrula
        node_info->g_score = FLT_MAX;
        node_info->h_score = FLT_MAX;
        node_info->f_score = FLT_MAX;
        node_info->parent_poly_id = FE_INVALID_ID;
        node_info->in_open_set = false;
        node_info->in_closed_set = false;
    }

    // Min-heap'i başlat
    fe_heap_t open_set_heap;
    if (!fe_heap_init(&open_set_heap, sizeof(fe_nav_mesh_path_node_t), nav_mesh->path_node_capacity, FE_MEM_TYPE_NAV_MESH_HEAP, fe_nav_mesh_path_node_compare)) {
        FE_LOG_ERROR("fe_nav_mesh_find_path: Failed to initialize A* open set heap.");
        return false;
    }

    fe_nav_mesh_path_node_t* start_path_node = &nav_mesh->path_nodes[start_poly_id];
    start_path_node->g_score = 0.0f;
    start_path_node->h_score = fe_vec3_dist(nav_mesh->polygons.data[start_poly_id].center, nav_mesh->polygons.data[end_poly_id].center);
    start_path_node->f_score = start_path_node->h_score;
    start_path_node->in_open_set = true;
    fe_heap_insert(&open_set_heap, start_path_node);

    bool path_found = false;

    while (fe_heap_get_size(&open_set_heap) > 0) {
        fe_nav_mesh_path_node_t current_node_info;
        fe_heap_extract_min(&open_set_heap, &current_node_info);
        uint32_t current_poly_id = current_node_info.polygon_id;

        if (current_poly_id == end_poly_id) {
            path_found = true;
            break; // Yol bulundu
        }

        nav_mesh->path_nodes[current_poly_id].in_closed_set = true;

        fe_nav_mesh_polygon_t* current_polygon = (fe_nav_mesh_polygon_t*)fe_array_get_at(&nav_mesh->polygons, current_poly_id);

        for (uint32_t i = 0; i < current_polygon->neighbor_count; ++i) {
            uint32_t neighbor_poly_id = current_polygon->neighbor_poly_ids[i];
            
            // Eğer komşu geçersiz veya kapalı kümedeyse atla
            if (neighbor_poly_id == FE_INVALID_ID || nav_mesh->path_nodes[neighbor_poly_id].in_closed_set) {
                continue;
            }

            fe_nav_mesh_path_node_t* neighbor_path_node = &nav_mesh->path_nodes[neighbor_poly_id];
            fe_nav_mesh_polygon_t* neighbor_polygon = (fe_nav_mesh_polygon_t*)fe_array_get_at(&nav_mesh->polygons, neighbor_poly_id);

            // G-score hesapla: current_polygon'dan neighbor_polygon'a geçiş maliyeti
            float dist_between_centers = fe_vec3_dist(current_polygon->center, neighbor_polygon->center);
            float tentative_g_score = nav_mesh->path_nodes[current_poly_id].g_score + dist_between_centers;

            if (tentative_g_score < neighbor_path_node->g_score) {
                neighbor_path_node->parent_poly_id = current_poly_id;
                neighbor_path_node->g_score = tentative_g_score;
                neighbor_path_node->h_score = fe_vec3_dist(neighbor_polygon->center, nav_mesh->polygons.data[end_poly_id].center);
                neighbor_path_node->f_score = neighbor_path_node->g_score + neighbor_path_node->h_score;

                if (!neighbor_path_node->in_open_set) {
                    neighbor_path_node->in_open_set = true;
                    fe_heap_insert(&open_set_heap, neighbor_path_node);
                } else {
                    // Heap'i güncelle (f_score değiştiği için)
                    fe_heap_update_element(&open_set_heap, neighbor_path_node);
                }
            }
        }
    }

    fe_heap_destroy(&open_set_heap);

    if (path_found) {
        FE_LOG_INFO("A* path found from polygon %u to %u.", start_poly_id, end_poly_id);
        // Yolu geri izle
        uint32_t current_poly = end_poly_id;
        fe_array_t temp_path;
        fe_array_init(&temp_path, sizeof(uint32_t), 16, FE_MEM_TYPE_TEMP); // Geçici dizi
        while (current_poly != FE_INVALID_ID) {
            fe_array_add_element(&temp_path, &current_poly);
            if (current_poly == start_poly_id) break; // Başlangıca ulaşıldı
            current_poly = nav_mesh->path_nodes[current_poly].parent_poly_id;
        }
        // Yolu ters çevir
        for (int i = fe_array_get_size(&temp_path) - 1; i >= 0; --i) {
            uint32_t poly_id = *(uint32_t*)fe_array_get_at(&temp_path, i);
            fe_array_add_element(out_path_polygons, &poly_id);
        }
        fe_array_destroy(&temp_path);
        FE_LOG_DEBUG("Path has %zu polygons.", fe_array_get_size(out_path_polygons));
        return true;
    } else {
        FE_LOG_WARN("No A* path found from polygon %u to %u.", start_poly_id, end_poly_id);
        return false;
    }
}

// Funnel algoritması için yardımcı fonksiyon: Bir doğrunun doğru tarafında mı?
// Eğer c'nin (x,z) düzlemindeki koordinatları (a,b) doğrusunun sağındaysa pozitif, solundaysa negatif
// Sıfır, doğru üzerindeyse.
static float triangle_area_2d(fe_vec3_t a, fe_vec3_t b, fe_vec3_t c) {
    return (b.x - a.x) * (c.z - a.z) - (b.z - a.z) * (c.x - a.x);
}

// Funnel algoritması
bool fe_nav_mesh_smooth_path(const fe_nav_mesh_t* nav_mesh, const fe_array_t* path_polygons,
                              fe_vec3_t start_pos, fe_vec3_t end_pos, fe_array_t* out_steering_points) {
    if (!nav_mesh || !path_polygons || !out_steering_points || fe_array_get_size(path_polygons) == 0) {
        FE_LOG_ERROR("fe_nav_mesh_smooth_path: Invalid arguments or empty path_polygons.");
        return false;
    }

    fe_array_clear(out_steering_points);
    fe_array_add_element(out_steering_points, &start_pos); // Başlangıç noktası her zaman ilk steering point'tir

    if (fe_array_get_size(path_polygons) == 1) {
        fe_array_add_element(out_steering_points, &end_pos); // Tek poligon varsa direkt hedefe git
        return true;
    }

    // Funnel algoritması için gerekli değişkenler
    fe_vec3_t portal_apex = start_pos;
    fe_vec3_t portal_left = start_pos;
    fe_vec3_t portal_right = start_pos;
    
    int left_idx = 0;
    int right_idx = 0;

    // Poli_gon yolunun ilk poli_gonundan başla
    uint32_t current_poly_id = *(uint32_t*)fe_array_get_at(path_polygons, 0);
    fe_nav_mesh_polygon_t* current_poly = (fe_nav_mesh_polygon_t*)fe_array_get_at(&nav_mesh->polygons, current_poly_id);

    // Tüm poli_gonları ve geçiş kenarlarını dolaş
    for (size_t i = 1; i < fe_array_get_size(path_polygons); ++i) {
        uint32_t next_poly_id = *(uint32_t*)fe_array_get_at(path_polygons, i);
        fe_nav_mesh_polygon_t* next_poly = (fe_nav_mesh_polygon_t*)fe_array_get_at(&nav_mesh->polygons, next_poly_id);

        // Ortak kenarı bul (portal)
        fe_vec3_t portal_v1 = FE_VEC3_ZERO; // Sol köşe
        fe_vec3_t portal_v2 = FE_VEC3_ZERO; // Sağ köşe
        bool found_portal = false;

        for (uint32_t c = 0; c < current_poly->neighbor_count; ++c) {
            if (current_poly->neighbor_poly_ids[c] == next_poly_id) {
                uint32_t v_idx_curr_1 = current_poly->neighbor_edge_vertex_indices[c][0];
                uint32_t v_idx_curr_2 = current_poly->neighbor_edge_vertex_indices[c][1];

                fe_vec3_t edge_p1 = *(fe_vec3_t*)fe_array_get_at(&nav_mesh->vertices, current_poly->vertex_indices[v_idx_curr_1]);
                fe_vec3_t edge_p2 = *(fe_vec3_t*)fe_array_get_at(&nav_mesh->vertices, current_poly->vertex_indices[v_idx_curr_2]);
                
                // Kenarın yönünü belirle: portal_v1 "sol", portal_v2 "sağ" olmalı
                // Ajanın hareket yönüne göre kenarın sol/sağını belirlemek gerekir.
                // Basitçe: portal_apex'ten, next_poly'nin merkezine doğru vektör ile çapraz çarpım yaparak yön belirle.
                fe_vec3_t current_to_next_center = fe_vec3_sub(next_poly->center, current_poly->center);
                fe_vec3_t edge_vec = fe_vec3_sub(edge_p2, edge_p1);

                if (fe_vec3_cross(current_to_next_center, edge_vec).y > 0) { // Y ekseni yukarı
                    portal_v1 = edge_p1; // Sol
                    portal_v2 = edge_p2; // Sağ
                } else {
                    portal_v1 = edge_p2; // Sol
                    portal_v2 = edge_p1; // Sağ
                }
                found_portal = true;
                break;
            }
        }

        if (!found_portal) {
            FE_LOG_ERROR("Smooth Path: Could not find common edge between polygon %u and %u.", current_poly_id, next_poly_id);
            fe_array_destroy(out_steering_points); // Temizle
            return false;
        }

        // --- Funnel Algoritması Mantığı ---
        // Yeni sol kenar
        if (triangle_area_2d(portal_apex, portal_left, portal_v1) >= 0) { // Yeni sol, mevcut solun solunda veya üzerinde
            portal_left = portal_v1;
            left_idx = i;
        } else { // Yeni sol, mevcut solun sağında (yani funnel daralıyor)
            // Eğer yeni sol, mevcut sağın sağındaysa, apex'i değiştir ve yeni steering point ekle
            if (triangle_area_2d(portal_apex, portal_right, portal_v1) < 0) {
                fe_array_add_element(out_steering_points, &portal_right);
                portal_apex = portal_right; // Apex'i sağ köşeye taşı
                left_idx = right_idx;
                portal_left = portal_apex; // Solu yeniden tanımla
                portal_right = portal_apex; // Sağı yeniden tanımla

                // Funnel'ı yeniden başlat, eklenen noktadan itibaren
                int restart_poly_idx = right_idx; // Funnel'ı buradan yeniden başlat
                if (restart_poly_idx < fe_array_get_size(path_polygons)) {
                     // Sonraki geçişten itibaren portal_apex'i ayarla
                    current_poly_id = *(uint32_t*)fe_array_get_at(path_polygons, restart_poly_idx);
                    current_poly = (fe_nav_mesh_polygon_t*)fe_array_get_at(&nav_mesh->polygons, current_poly_id);
                    i = restart_poly_idx; // Döngüyü sıfırla
                    continue;
                }
            }
        }

        // Yeni sağ kenar
        if (triangle_area_2d(portal_apex, portal_right, portal_v2) <= 0) { // Yeni sağ, mevcut sağın sağında veya üzerinde
            portal_right = portal_v2;
            right_idx = i;
        } else { // Yeni sağ, mevcut solun solunda (yani funnel daralıyor)
            // Eğer yeni sağ, mevcut solun solundaysa, apex'i değiştir ve yeni steering point ekle
            if (triangle_area_2d(portal_apex, portal_left, portal_v2) > 0) {
                fe_array_add_element(out_steering_points, &portal_left);
                portal_apex = portal_left; // Apex'i sol köşeye taşı
                right_idx = left_idx;
                portal_right = portal_apex; // Sağı yeniden tanımla
                portal_left = portal_apex; // Solu yeniden tanımla

                // Funnel'ı yeniden başlat, eklenen noktadan itibaren
                int restart_poly_idx = left_idx;
                if (restart_poly_idx < fe_array_get_size(path_polygons)) {
                    current_poly_id = *(uint32_t*)fe_array_get_at(path_polygons, restart_poly_idx);
                    current_poly = (fe_nav_mesh_polygon_t*)fe_array_get_at(&nav_mesh->polygons, current_poly_id);
                    i = restart_poly_idx; // Döngüyü sıfırla
                    continue;
                }
            }
        }
        current_poly_id = next_poly_id;
        current_poly = next_poly;
    }

    // Son hedef noktasını ekle
    fe_array_add_element(out_steering_points, &end_pos);
    FE_LOG_INFO("Path smoothed. %zu steering points generated.", fe_array_get_size(out_steering_points));
    return true;
}
