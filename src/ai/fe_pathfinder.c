#include "navigation/fe_pathfinder.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h" // fe_array_t için
#include "core/math/fe_math.h" // fe_vec3_dist_sq için

// --- Yardımcı Fonksiyonlar ---

const char* fe_path_status_to_string(fe_path_status_t status) {
    switch (status) {
        case FE_PATH_STATUS_NONE: return "NONE";
        case FE_PATH_STATUS_COMPUTING: return "COMPUTING";
        case FE_PATH_STATUS_SUCCESS: return "SUCCESS";
        case FE_PATH_STATUS_FAILURE_NO_PATH: return "FAILURE_NO_PATH";
        case FE_PATH_STATUS_FAILURE_INVALID_ARGS: return "FAILURE_INVALID_ARGS";
        case FE_PATH_STATUS_COMPLETED: return "COMPLETED";
        default: return "UNKNOWN_STATUS";
    }
}

// --- fe_path_t Fonksiyonları ---

void fe_path_init_empty(fe_path_t* path, uint32_t agent_id) {
    if (!path) {
        FE_LOG_ERROR("fe_path_init_empty: Path pointer is NULL.");
        return;
    }
    memset(path, 0, sizeof(fe_path_t));
    if (!fe_array_init(&path->steering_points, sizeof(fe_vec3_t), 32, FE_MEM_TYPE_PATHFINDER_STEERING_POINTS)) {
        FE_LOG_CRITICAL("fe_path_init_empty: Failed to initialize steering points array.");
        // Hata durumunda, path'in durumu zaten 0 (NONE) ve diğer alanlar da 0.
        // Array init başarısız olursa, path yine de kullanılabilir durumda olmayacaktır.
    }
    path->status = FE_PATH_STATUS_NONE;
    path->current_point_idx = 0;
    path->agent_id = agent_id;
    path->start_pos = FE_VEC3_ZERO;
    path->end_pos = FE_VEC3_ZERO;
    FE_LOG_DEBUG("Path initialized for agent ID %u.", agent_id);
}

void fe_path_destroy(fe_path_t* path) {
    if (!path) return;

    if (fe_array_is_initialized(&path->steering_points)) {
        fe_array_destroy(&path->steering_points);
    }
    memset(path, 0, sizeof(fe_path_t)); // Yapıyı sıfırla
    FE_LOG_DEBUG("Path destroyed.");
}

bool fe_path_get_next_point(fe_path_t* path, fe_vec3_t current_agent_pos, float tolerance, fe_vec3_t* out_next_point) {
    if (!path || !out_next_point || !fe_array_is_initialized(&path->steering_points)) {
        FE_LOG_ERROR("fe_path_get_next_point: Invalid arguments or uninitialized path.");
        return false;
    }

    if (path->status != FE_PATH_STATUS_SUCCESS && path->status != FE_PATH_STATUS_COMPUTING) {
        FE_LOG_WARN("fe_path_get_next_point: Path is not in a usable state (status: %s).", fe_path_status_to_string(path->status));
        return false;
    }

    // Yol tamamlanmışsa
    if (path->current_point_idx >= fe_array_get_size(&path->steering_points)) {
        path->status = FE_PATH_STATUS_COMPLETED;
        FE_LOG_INFO("Path for agent %u completed.", path->agent_id);
        return false;
    }

    fe_vec3_t target_point = *(fe_vec3_t*)fe_array_get_at(&path->steering_points, path->current_point_idx);

    // Mevcut hedefe yeterince yakın mı? (2D düzlemde XZ mesafesi)
    // Y ekseni genellikle zıplama/düşme gibi dikey hareketler için NavMesh'in kendisinden bağımsızdır.
    float dist_sq = fe_vec3_dist_sq(
        FE_VEC3_CREATE(current_agent_pos.x, 0.0f, current_agent_pos.z),
        FE_VEC3_CREATE(target_point.x, 0.0f, target_point.z)
    );

    if (dist_sq <= tolerance * tolerance) {
        // Hedefe ulaşıldı, bir sonraki noktaya geç
        path->current_point_idx++;
        FE_LOG_DEBUG("Agent %u reached steering point %u. Moving to next.", path->agent_id, path->current_point_idx -1);
        
        // Yolun sonuna ulaşıldıysa
        if (path->current_point_idx >= fe_array_get_size(&path->steering_points)) {
            path->status = FE_PATH_STATUS_COMPLETED;
            FE_LOG_INFO("Path for agent %u completed.", path->agent_id);
            return false;
        }
        target_point = *(fe_vec3_t*)fe_array_get_at(&path->steering_points, path->current_point_idx);
    }

    *out_next_point = target_point;
    return true;
}

bool fe_path_is_completed(const fe_path_t* path) {
    if (!path) return false;
    return path->status == FE_PATH_STATUS_COMPLETED;
}

// --- fe_pathfinder_t Fonksiyonları ---

bool fe_pathfinder_init(fe_pathfinder_t* pathfinder, fe_nav_mesh_t* nav_mesh) {
    if (!pathfinder || !nav_mesh) {
        FE_LOG_ERROR("fe_pathfinder_init: Pathfinder or NavMesh pointer is NULL.");
        return false;
    }
    if (!fe_array_is_initialized(&nav_mesh->polygons) || fe_array_get_size(&nav_mesh->polygons) == 0) {
        FE_LOG_WARN("fe_pathfinder_init: NavMesh is not initialized or empty. Pathfinding might fail.");
    }

    memset(pathfinder, 0, sizeof(fe_pathfinder_t));
    pathfinder->nav_mesh = nav_mesh; // NavMesh'e referans tut

    FE_LOG_INFO("Pathfinder initialized with NavMesh %p.", (void*)nav_mesh);
    return true;
}

void fe_pathfinder_destroy(fe_pathfinder_t* pathfinder) {
    if (!pathfinder) return;

    // Pathfinder'ın kendisi aktif yolları yönetiyorsa, burada onları serbest bırakmalı.
    // Şu anki implementasyonda fe_path_t dışarıdan yönetiliyor.
    FE_LOG_INFO("Pathfinder destroyed.");
    memset(pathfinder, 0, sizeof(fe_pathfinder_t));
}

fe_path_status_t fe_pathfinder_find_path(fe_pathfinder_t* pathfinder, 
                                         fe_vec3_t start_pos, fe_vec3_t end_pos, 
                                         uint32_t agent_id, fe_path_t* out_path) {
    if (!pathfinder || !pathfinder->nav_mesh || !out_path) {
        FE_LOG_ERROR("fe_pathfinder_find_path: Invalid arguments (pathfinder, nav_mesh, or out_path is NULL).");
        return FE_PATH_STATUS_FAILURE_INVALID_ARGS;
    }

    // out_path'i her zaman yeniden başlat.
    // Eğer fe_path_t daha önce kullanıldıysa ve serbest bırakılmadıysa, bellek sızıntısı olur.
    // Bu yüzden çağırıcı fe_path_destroy() sorumluluğunu almalıdır veya burada emin olmalıyız.
    // Basitlik için, burada her zaman yeniden başlatıyoruz.
    fe_path_destroy(out_path); 
    fe_path_init_empty(out_path, agent_id);
    out_path->start_pos = start_pos;
    out_path->end_pos = end_pos;

    FE_LOG_INFO("Pathfinding request for agent %u from (%.2f,%.2f,%.2f) to (%.2f,%.2f,%.2f).",
                agent_id, start_pos.x, start_pos.y, start_pos.z, end_pos.x, end_pos.y, end_pos.z);

    fe_array_t path_polygons;
    // Geçici dizi olduğu için küçük bir başlangıç kapasitesi
    if (!fe_array_init(&path_polygons, sizeof(uint32_t), 32, FE_MEM_TYPE_TEMP)) {
        FE_LOG_CRITICAL("fe_pathfinder_find_path: Failed to initialize temporary polygon path array.");
        out_path->status = FE_PATH_STATUS_FAILURE_NO_PATH; // Daha uygun bir hata durumu olabilir
        return out_path->status;
    }

    // NavMesh üzerinde ham poli_gon yolunu bul
    if (!fe_nav_mesh_find_path(pathfinder->nav_mesh, start_pos, end_pos, &path_polygons)) {
        FE_LOG_WARN("fe_pathfinder_find_path: fe_nav_mesh_find_path returned no path for agent %u.", agent_id);
        fe_array_destroy(&path_polygons);
        out_path->status = FE_PATH_STATUS_FAILURE_NO_PATH;
        return out_path->status;
    }

    // Bulunan poli_gon yolunu düzgün ara noktalara çevir
    if (!fe_nav_mesh_smooth_path(pathfinder->nav_mesh, &path_polygons, start_pos, end_pos, &out_path->steering_points)) {
        FE_LOG_WARN("fe_pathfinder_find_path: fe_nav_mesh_smooth_path failed for agent %u.", agent_id);
        fe_array_destroy(&path_polygons);
        out_path->status = FE_PATH_STATUS_FAILURE_NO_PATH; // Smooth yapılamazsa yol kullanılamaz
        return out_path->status;
    }

    fe_array_destroy(&path_polygons); // Geçici diziyi serbest bırak

    out_path->status = FE_PATH_STATUS_SUCCESS;
    FE_LOG_INFO("Pathfinding successful for agent %u. %zu steering points generated.", 
                agent_id, fe_array_get_size(&out_path->steering_points));
    return out_path->status;
}
