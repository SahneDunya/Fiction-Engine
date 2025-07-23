#include "physics/fe_physics_manager.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h"
#include "core/math/fe_math.h" // fe_vec3_normalize vb. için

// --- Global Fizik Yönetici Durumu (Singleton) ---
static fe_physics_manager_state_t g_physics_manager_state;

// --- Dahili Yardımcı Fonksiyonlar ---

// Chaos'tan gelen çarpışma olaylarını Fiction Engine'ın geri çağırma sistemine dönüştüren Mock fonksiyonlar.
// Gerçek Chaos entegrasyonunda, Chaos'un kendi event listener mekanizmaları bu fonksiyonları çağıracaktır.

// Bu fonksiyonlar, Chaos'un dahili geri çağrılarında tetiklenecek ve ardından
// Fiction Engine'ın kayıtlı geri çağırmalarını çağıracaktır.
// NOT: Chaos'un event yapısı karmaşık olabilir. Bu sadece bir basitleştirilmiş örnektir.

static void fe_physics_manager_chaos_on_collision_enter(void* chaos_body_handle1, void* chaos_body_handle2,
                                                       fe_vec3 contact_point, fe_vec3 normal, float impulse) {
    if (!g_physics_manager_state.is_initialized) return;

    // Chaos tutaçlarından Fiction Engine'ın fe_chaos_rigid_body_t nesnelerini bul.
    // Bu, Chaos'un API'si aracılığıyla yapılmalıdır. Örneğin:
    // FChaosPhysicsActor* actor1 = (FChaosPhysicsActor*)chaos_body_handle1;
    // FChaosPhysicsActor* actor2 = (FChaosPhysicsActor*)chaos_body_handle2;
    // fe_chaos_rigid_body_t* body1 = (fe_chaos_rigid_body_t*)actor1->GetUserData(); // Eğer user_data'yı set ettiysek
    // fe_chaos_rigid_body_t* body2 = (fe_chaos_rigid_body_t*)actor2->GetUserData();
    
    // Geçici Mock:
    fe_chaos_rigid_body_t mock_body1;
    memset(&mock_body1, 0, sizeof(fe_chaos_rigid_body_t));
    fe_string_init(&mock_body1.id, "MockBodyA");
    mock_body1.chaos_actor_handle = chaos_body_handle1;

    fe_chaos_rigid_body_t mock_body2;
    memset(&mock_body2, 0, sizeof(fe_chaos_rigid_body_t));
    fe_string_init(&mock_body2.id, "MockBodyB");
    mock_body2.chaos_actor_handle = chaos_body_handle2;

    if (g_physics_manager_state.on_collision_enter_cb) {
        g_physics_manager_state.on_collision_enter_cb(&mock_body1, &mock_body2, contact_point, normal, impulse);
    }
    fe_string_destroy(&mock_body1.id);
    fe_string_destroy(&mock_body2.id);
}

static void fe_physics_manager_chaos_on_collision_stay(void* chaos_body_handle1, void* chaos_body_handle2,
                                                      fe_vec3 contact_point, fe_vec3 normal) {
    if (!g_physics_manager_state.is_initialized) return;

    // Gerçek Chaos entegrasyonunda benzer şekilde fe_chaos_rigid_body_t'ler bulunmalı
    // Geçici Mock:
    fe_chaos_rigid_body_t mock_body1;
    memset(&mock_body1, 0, sizeof(fe_chaos_rigid_body_t));
    fe_string_init(&mock_body1.id, "MockBodyA");
    mock_body1.chaos_actor_handle = chaos_body_handle1;

    fe_chaos_rigid_body_t mock_body2;
    memset(&mock_body2, 0, sizeof(fe_chaos_rigid_body_t));
    fe_string_init(&mock_body2.id, "MockBodyB");
    mock_body2.chaos_actor_handle = chaos_body_handle2;

    if (g_physics_manager_state.on_collision_stay_cb) {
        g_physics_manager_state.on_collision_stay_cb(&mock_body1, &mock_body2, contact_point, normal);
    }
    fe_string_destroy(&mock_body1.id);
    fe_string_destroy(&mock_body2.id);
}

static void fe_physics_manager_chaos_on_collision_exit(void* chaos_body_handle1, void* chaos_body_handle2) {
    if (!g_physics_manager_state.is_initialized) return;

    // Gerçek Chaos entegrasyonunda benzer şekilde fe_chaos_rigid_body_t'ler bulunmalı
    // Geçici Mock:
    fe_chaos_rigid_body_t mock_body1;
    memset(&mock_body1, 0, sizeof(fe_chaos_rigid_body_t));
    fe_string_init(&mock_body1.id, "MockBodyA");
    mock_body1.chaos_actor_handle = chaos_body_handle1;

    fe_chaos_rigid_body_t mock_body2;
    memset(&mock_body2, 0, sizeof(fe_chaos_rigid_body_t));
    fe_string_init(&mock_body2.id, "MockBodyB");
    mock_body2.chaos_actor_handle = chaos_body_handle2;

    if (g_physics_manager_state.on_collision_exit_cb) {
        g_physics_manager_state.on_collision_exit_cb(&mock_body1, &mock_body2);
    }
    fe_string_destroy(&mock_body1.id);
    fe_string_destroy(&mock_body2.id);
}


// --- Fonksiyon Uygulamaları ---

fe_physics_manager_error_t fe_physics_manager_init(fe_vec3 gravity, float fixed_timestep) {
    if (g_physics_manager_state.is_initialized) {
        FE_LOG_WARN("Physics manager already initialized.");
        return FE_PHYSICS_MANAGER_ALREADY_INITIALIZED;
    }

    // Chaos World'ü başlat
    fe_chaos_error_t chaos_err = fe_chaos_world_init(gravity);
    if (chaos_err != FE_CHAOS_SUCCESS) {
        FE_LOG_CRITICAL("Failed to initialize Chaos World: %d", chaos_err);
        return FE_PHYSICS_MANAGER_CHAOS_ERROR;
    }

    g_physics_manager_state.fixed_timestep = fixed_timestep;
    g_physics_manager_state.accumulator = 0.0f;
    g_physics_manager_state.on_collision_enter_cb = NULL;
    g_physics_manager_state.on_collision_stay_cb = NULL;
    g_physics_manager_state.on_collision_exit_cb = NULL;

    g_physics_manager_state.is_initialized = true;
    FE_LOG_INFO("Physics manager initialized with fixed timestep: %.4f", fixed_timestep);

    // TODO: Gerçek Chaos entegrasyonunda, Chaos'un olay sistemine bu geri çağırma fonksiyonlarını kaydet.
    // Örneğin: FChaosScene::RegisterCollisionCallback(fe_physics_manager_chaos_on_collision_enter);
    // Veya Chaos'tan gelen olayları ana döngüde çekip işleyen bir sistem kurulmalı.

    return FE_PHYSICS_MANAGER_SUCCESS;
}

void fe_physics_manager_shutdown() {
    if (!g_physics_manager_state.is_initialized) {
        FE_LOG_WARN("Physics manager not initialized. Nothing to shutdown.");
        return;
    }

    // Chaos World'ü kapat
    fe_chaos_world_shutdown();

    g_physics_manager_state.is_initialized = false;
    g_physics_manager_state.fixed_timestep = 0.0f;
    g_physics_manager_state.accumulator = 0.0f;
    g_physics_manager_state.on_collision_enter_cb = NULL;
    g_physics_manager_state.on_collision_stay_cb = NULL;
    g_physics_manager_state.on_collision_exit_cb = NULL;

    FE_LOG_INFO("Physics manager shutdown complete.");
}

fe_physics_manager_error_t fe_physics_manager_update(float delta_time) {
    if (!g_physics_manager_state.is_initialized) {
        FE_LOG_ERROR("Physics manager not initialized. Cannot update physics.");
        return FE_PHYSICS_MANAGER_NOT_INITIALIZED;
    }

    g_physics_manager_state.accumulator += delta_time;

    // Sabit zaman adımlarıyla fizik simülasyonunu ilerlet
    while (g_physics_manager_state.accumulator >= g_physics_manager_state.fixed_timestep) {
        fe_chaos_error_t chaos_err = fe_chaos_world_update(g_physics_manager_state.fixed_timestep);
        if (chaos_err != FE_CHAOS_SUCCESS) {
            FE_LOG_ERROR("Error updating Chaos world: %d", chaos_err);
            return FE_PHYSICS_MANAGER_CHAOS_ERROR;
        }
        g_physics_manager_state.accumulator -= g_physics_manager_state.fixed_timestep;
    }

    // TODO: Burada simülasyon sonrası gerekli olan işlevler olabilir.
    // Örneğin, Chaos'tan güncel transformları çekmek ve oyun nesnelerine dağıtmak.
    // Bu, genellikle her oyun nesnesinin kendi fizik bileşeni tarafından yapılır,
    // ancak merkezi bir senkronizasyon noktası da burada olabilir.

    return FE_PHYSICS_MANAGER_SUCCESS;
}

void fe_physics_manager_register_on_collision_enter(fe_on_collision_enter_callback_t callback) {
    g_physics_manager_state.on_collision_enter_cb = callback;
    FE_LOG_DEBUG("OnCollisionEnter callback registered.");
}

void fe_physics_manager_register_on_collision_stay(fe_on_collision_stay_callback_t callback) {
    g_physics_manager_state.on_collision_stay_cb = callback;
    FE_LOG_DEBUG("OnCollisionStay callback registered.");
}

void fe_physics_manager_register_on_collision_exit(fe_on_collision_exit_callback_t callback) {
    g_physics_manager_state.on_collision_exit_cb = callback;
    FE_LOG_DEBUG("OnCollisionExit callback registered.");
}

fe_physics_manager_error_t fe_physics_manager_set_rigid_body_user_data(const char* body_id, void* user_data) {
    fe_chaos_rigid_body_t* body = fe_chaos_world_get_rigid_body(body_id);
    if (!body) {
        FE_LOG_ERROR("Rigid body with ID '%s' not found to set user data.", body_id);
        return FE_PHYSICS_MANAGER_BODY_NOT_FOUND;
    }
    body->user_data = user_data;
    // TODO: Chaos API'sinde de user_data'yı set etmek gerekebilir.
    // FChaosPhysicsActor* actor = (FChaosPhysicsActor*)body->chaos_actor_handle;
    // actor->SetUserData(user_data);
    FE_LOG_DEBUG("User data set for rigid body '%s'.", body_id);
    return FE_PHYSICS_MANAGER_SUCCESS;
}

void* fe_physics_manager_get_rigid_body_user_data(const char* body_id) {
    fe_chaos_rigid_body_t* body = fe_chaos_world_get_rigid_body(body_id);
    if (!body) {
        FE_LOG_WARN("Rigid body with ID '%s' not found to get user data.", body_id);
        return NULL;
    }
    return body->user_data;
}

fe_physics_manager_error_t fe_physics_manager_raycast(
    fe_vec3 origin,
    fe_vec3 direction,
    float max_distance,
    fe_raycast_hit_t* out_hit,
    uint32_t collision_mask
) {
    if (!g_physics_manager_state.is_initialized) {
        FE_LOG_ERROR("Physics manager not initialized. Cannot perform raycast.");
        return FE_PHYSICS_MANAGER_NOT_INITIALIZED;
    }
    if (!out_hit) {
        FE_LOG_ERROR("Raycast: out_hit pointer is NULL.");
        return FE_PHYSICS_MANAGER_INVALID_ARGUMENT;
    }

    memset(out_hit, 0, sizeof(fe_raycast_hit_t));
    out_hit->hit = false;

    // Yön vektörünü normalize etmeyi unutma
    fe_vec3_normalize(&direction);

    // TODO: Gerçek Chaos API'si ile raycast işlemi yap.
    // Chaos'un kendi raycast fonksiyonunu çağır:
    // Örneğin: bool bHit = FChaosScene::Raycast(ConvertFromFeVec3(origin), ConvertFromFeVec3(direction), max_distance, /*out_hit_info*/, collision_mask);
    // Bu bHit'e ve out_hit_info'ya göre out_hit yapısını doldur.
    
    // Mock Raycast Sonucu (her zaman bir şeye vursun ve rastgele bir nokta/normal döndürsün)
    // Gerçek raycast'te, en yakın isabet döndürülür.
    if (rand() % 2 == 0) { // %50 ihtimalle isabet
        out_hit->hit = true;
        out_hit->distance = max_distance * ((float)rand() / RAND_MAX); // Rastgele bir uzaklık
        out_hit->position = fe_vec3_add(origin, fe_vec3_mul_scalar(direction, out_hit->distance));
        out_hit->normal = (fe_vec3){ (float)rand()/RAND_MAX * 2 - 1, (float)rand()/RAND_MAX * 2 - 1, (float)rand()/RAND_MAX * 2 - 1 };
        fe_vec3_normalize(&out_hit->normal);

        // Mock bir cisim bul ve user_data ata (gerçekte Chaos'tan gelen body'i kullanacağız)
        fe_chaos_rigid_body_t* mock_body = fe_chaos_world_get_rigid_body("PlayerRigidBody"); // Örnek için bir ID
        if (mock_body) {
            out_hit->body = mock_body;
            out_hit->user_data = mock_body->user_data;
        } else {
             // Hiçbir body bulunamazsa, sadece hit bilgisi döneriz.
             // Veya özel bir "varsayılan zemin" cismi döndürülebilir.
             out_hit->body = NULL;
             out_hit->user_data = NULL;
        }
        FE_LOG_DEBUG("Raycast hit detected at (%.2f, %.2f, %.2f) with distance %.2f.", 
                     out_hit->position.x, out_hit->position.y, out_hit->position.z, out_hit->distance);
    } else {
        FE_LOG_DEBUG("Raycast did not hit anything.");
    }
    
    return FE_PHYSICS_MANAGER_SUCCESS;
}
