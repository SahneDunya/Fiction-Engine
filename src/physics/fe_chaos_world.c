#include "physics/fe_chaos_world.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h"
#include "core/math/fe_math.h" // fe_vec3_add, fe_quat_multiply vb. için
#include "core/utils/fe_string_builder.h" // ID oluşturmak için
#include "core/utils/fe_time.h" // ID oluşturmak için

#include <string.h> // memset, strcmp
#include <stdio.h>  // snprintf

// --- Harici Chaos Kütüphanesi Başlıkları (Placeholder) ---
// Gerçek entegrasyon için Unreal Engine Chaos headers buraya dahil edilmelidir.
// Örneğin:
 #include "Chaos/Core.h"
 #include "Chaos/PhysicsEngine/ChaosScene.h"
 #include "Chaos/PBDPhysicsEngine.h"
 #include "Chaos/KinematicTargets.h"
 #include "Chaos/Framework/PhysicsProxy.h"
 #include "Chaos/Framework/PhysicsSolver.h"
 #include "Chaos/Particles.h"
 #include "Chaos/Particles.h"

// Unreal Engine'ın kendi hafıza yöneticisi ve veri yapıları olabilir.
// Bu entegrasyon, Fiction Engine'ın bellek yöneticisiyle uyumlu hale getirilmelidir.

// Global Chaos Dünya Durumu (Singleton)
static fe_chaos_world_state_t g_chaos_world_state;

// --- Dahili Yardımcı Fonksiyonlar ---

// UUID benzeri bir ID oluşturucu (geçici)
static fe_string fe_chaos_generate_id(const char* prefix) {
    fe_string_builder_t sb;
    fe_string_builder_init(&sb);
    fe_string_builder_append_c_str(&sb, prefix);
    fe_string_builder_append_c_str(&sb, "_");
    fe_string_builder_append_long(&sb, fe_get_current_time_ms());
    fe_string_builder_append_int(&sb, rand() % 100000); // Küçük bir rastgelelik
    fe_string result;
    fe_string_init(&result, fe_string_builder_get_string(&sb));
    fe_string_builder_destroy(&sb);
    return result;
}

// Çarpışma şeklini serbest bırakma
static void fe_chaos_free_collision_shape(fe_collision_shape_t* shape) {
    if (shape->type == FE_COLLISION_SHAPE_MESH || shape->type == FE_COLLISION_SHAPE_CONVEX_HULL) {
        fe_string_destroy(&shape->data.mesh.mesh_id);
    }
    // Eğer Chaos'tan dönen bir tutaç varsa burada temizlenmelidir
    shape->chaos_shape_handle = NULL;
}

// Rijit cismi serbest bırakma
static void fe_chaos_free_rigid_body(fe_chaos_rigid_body_t* body) {
    if (!body) return;

    fe_string_destroy(&body->id);
    fe_string_destroy(&body->desc.name);

    if (body->desc.collision_shapes) {
        for (size_t i = 0; i < body->desc.shape_count; ++i) {
            fe_chaos_free_collision_shape(&body->desc.collision_shapes[i]);
        }
        fe_free(body->desc.collision_shapes, FE_MEM_TYPE_PHYSICS, __FILE__, __LINE__);
    }
    // Chaos'tan dönen aktör tutacını burada serbest bırak
    // Örneğin: FChaosPhysicsActor::Release(body->chaos_actor_handle);
    body->chaos_actor_handle = NULL;

    fe_free(body, FE_MEM_TYPE_PHYSICS, __FILE__, __LINE__);
}

// Çarpışma şekli listesini yeniden boyutlandır
static fe_chaos_error_t fe_chaos_resize_shape_list(fe_rigid_body_desc_t* desc, size_t new_capacity) {
    fe_collision_shape_t* new_list = fe_realloc(desc->collision_shapes,
                                                new_capacity * sizeof(fe_collision_shape_t),
                                                FE_MEM_TYPE_PHYSICS, __FILE__, __LINE__);
    if (!new_list) {
        FE_LOG_CRITICAL("Failed to reallocate memory for collision shapes.");
        return FE_CHAOS_OUT_OF_MEMORY;
    }
    desc->collision_shapes = new_list;
    desc->shape_capacity = new_capacity;
    return FE_CHAOS_SUCCESS;
}


// --- API Enum'dan String'e Çeviriler ---
const char* fe_physics_body_type_to_string(fe_physics_body_type_t type) {
    switch (type) {
        case FE_PHYSICS_BODY_STATIC: return "Static";
        case FE_PHYSICS_BODY_DYNAMIC: return "Dynamic";
        case FE_PHYSICS_BODY_KINEMATIC: return "Kinematic";
        default: return "Unknown";
    }
}

const char* fe_collision_shape_type_to_string(fe_collision_shape_type_t type) {
    switch (type) {
        case FE_COLLISION_SHAPE_SPHERE: return "Sphere";
        case FE_COLLISION_SHAPE_BOX: return "Box";
        case FE_COLLISION_SHAPE_CAPSULE: return "Capsule";
        case FE_COLLISION_SHAPE_PLANE: return "Plane";
        case FE_COLLISION_SHAPE_MESH: return "Mesh";
        case FE_COLLISION_SHAPE_CONVEX_HULL: return "Convex Hull";
        default: return "Unknown";
    }
}


// --- Ana Fonksiyonlar Uygulaması ---

fe_chaos_error_t fe_chaos_world_init(fe_vec3 gravity) {
    if (g_chaos_world_state.is_initialized) {
        FE_LOG_WARN("Chaos world already initialized.");
        return FE_CHAOS_ALREADY_INITIALIZED;
    }

    g_chaos_world_state.gravity = gravity;

    // TODO: Gerçek Chaos motorunu başlatma çağrıları buraya gelir.
    // Örneğin: FChaosEngine::Init();
    // g_chaos_world_state.chaos_scene_handle = new FChaosScene(); // Veya benzeri
    // g_chaos_world_state.chaos_scene_handle->SetGravity(gravity.x, gravity.y, gravity.z);

    // Rijit cisimleri tutmak için hash map başlat
    g_chaos_world_state.rigid_bodies_map = fe_hash_map_create(FE_HASH_MAP_TYPE_STRING, FE_HASH_MAP_TYPE_POINTER);
    if (!g_chaos_world_state.rigid_bodies_map) {
        FE_LOG_CRITICAL("Failed to create rigid body hash map.");
        // TODO: Chaos başlatıldıysa burada geri al
        return FE_CHAOS_OUT_OF_MEMORY;
    }

    g_chaos_world_state.is_initialized = true;
    FE_LOG_INFO("Chaos world initialized with gravity: {%.2f, %.2f, %.2f}", gravity.x, gravity.y, gravity.z);
    return FE_CHAOS_SUCCESS;
}

void fe_chaos_world_shutdown() {
    if (!g_chaos_world_state.is_initialized) {
        FE_LOG_WARN("Chaos world not initialized.");
        return;
    }

    // Hash map'deki tüm rijit cisimleri serbest bırak
    fe_hash_map_iterator_t it;
    fe_hash_map_iterator_init(g_chaos_world_state.rigid_bodies_map, &it);
    fe_string* key;
    fe_chaos_rigid_body_t* body;
    while (fe_hash_map_iterator_next(&it, (void**)&key, (void**)&body)) {
        fe_chaos_free_rigid_body(body);
        fe_string_destroy(key); // Anahtarı da serbest bırak
    }
    fe_hash_map_destroy(g_chaos_world_state.rigid_bodies_map);
    g_chaos_world_state.rigid_bodies_map = NULL;

    // TODO: Gerçek Chaos motorunu kapatma çağrıları buraya gelir.
    // Örneğin: delete (FChaosScene*)g_chaos_world_state.chaos_scene_handle;
    // FChaosEngine::Shutdown();
    g_chaos_world_state.chaos_scene_handle = NULL;

    g_chaos_world_state.is_initialized = false;
    FE_LOG_INFO("Chaos world shutdown complete.");
}

fe_chaos_error_t fe_chaos_world_update(float delta_time) {
    if (!g_chaos_world_state.is_initialized) {
        FE_LOG_ERROR("Chaos world not initialized. Cannot update.");
        return FE_CHAOS_NOT_INITIALIZED;
    }

    // TODO: Gerçek Chaos simülasyon güncelleme çağrıları buraya gelir.
    // Örneğin: ((FChaosScene*)g_chaos_world_state.chaos_scene_handle)->Advance(delta_time);
    // Bu genellikle sabit zaman adımlarıyla yapılır (fixed timestep).
    // if (g_chaos_world_state.chaos_scene_handle) {
    //     // Simulate physics for fixed timestep, e.g., 0.01666f
    // }

    // Simülasyon sonrası cisimlerin yeni pozisyonlarını ve rotasyonlarını Chaos'tan al.
    // fe_hash_map_iterator_t it;
    // fe_hash_map_iterator_init(g_chaos_world_state.rigid_bodies_map, &it);
    // fe_string* key;
    // fe_chaos_rigid_body_t* body;
    // while (fe_hash_map_iterator_next(&it, (void**)&key, (void**)&body)) {
    //     // Update body's internal position/rotation from Chaos's state
    //     // fe_chaos_world_get_rigid_body_position(body, &body->current_position); // Or similar internal storage
    //     // fe_chaos_world_get_rigid_body_rotation(body, &body->current_rotation);
    // }

    FE_LOG_DEBUG("Chaos world updated with delta_time: %.4f", delta_time);
    return FE_CHAOS_SUCCESS;
}

void fe_chaos_world_default_rigid_body_desc(fe_rigid_body_desc_t* desc) {
    if (!desc) return;
    memset(desc, 0, sizeof(fe_rigid_body_desc_t));
    desc->type = FE_PHYSICS_BODY_DYNAMIC;
    desc->mass = 1.0f;
    desc->initial_position = (fe_vec3){0.0f, 0.0f, 0.0f};
    desc->initial_rotation = (fe_quat){0.0f, 0.0f, 0.0f, 1.0f};
    desc->initial_linear_velocity = (fe_vec3){0.0f, 0.0f, 0.0f};
    desc->initial_angular_velocity = (fe_vec3){0.0f, 0.0f, 0.0f};
    desc->linear_damping = 0.01f;
    desc->angular_damping = 0.05f;
    desc->restitution = 0.3f;
    desc->static_friction = 0.5f;
    desc->dynamic_friction = 0.4f;
    desc->enable_gravity = true;
    desc->is_kinematic = false;
    desc->is_trigger = false;
    desc->collision_group = 1;
    desc->collision_mask = 0xFFFFFFFF; // Tüm gruplarla çarpışsın
    desc->collision_shapes = NULL;
    desc->shape_count = 0;
    desc->shape_capacity = 0;
}

void fe_chaos_world_default_collision_shape(fe_collision_shape_t* shape) {
    if (!shape) return;
    memset(shape, 0, sizeof(fe_collision_shape_t));
    shape->local_offset = (fe_vec3){0.0f, 0.0f, 0.0f};
    shape->local_rotation = (fe_quat){0.0f, 0.0f, 0.0f, 1.0f};
    shape->chaos_shape_handle = NULL;
}


fe_chaos_rigid_body_t* fe_chaos_world_create_rigid_body(const fe_rigid_body_desc_t* desc) {
    if (!g_chaos_world_state.is_initialized) {
        FE_LOG_ERROR("Chaos world not initialized. Cannot create rigid body.");
        return NULL;
    }
    if (!desc || desc->shape_count == 0) {
        FE_LOG_ERROR("Rigid body description is NULL or has no collision shapes.");
        return NULL;
    }

    fe_chaos_rigid_body_t* new_body = fe_malloc(sizeof(fe_chaos_rigid_body_t), FE_MEM_TYPE_PHYSICS, __FILE__, __LINE__);
    if (!new_body) {
        FE_LOG_CRITICAL("Failed to allocate memory for fe_chaos_rigid_body_t.");
        return NULL;
    }
    memset(new_body, 0, sizeof(fe_chaos_rigid_body_t));

    // Tanımlayıcıyı kopyala
    new_body->desc = *desc;
    fe_string_init(&new_body->desc.name, desc->name.data ? desc->name.data : "UnnamedBody");
    new_body->id = fe_chaos_generate_id("RigidBody");

    // Çarpışma şekillerini kopyala ve Chaos'a ekle (Placeholder)
    if (desc->shape_count > 0) {
        new_body->desc.collision_shapes = fe_malloc(desc->shape_count * sizeof(fe_collision_shape_t), FE_MEM_TYPE_PHYSICS, __FILE__, __LINE__);
        if (!new_body->desc.collision_shapes) {
            FE_LOG_CRITICAL("Failed to allocate memory for rigid body collision shapes.");
            fe_chaos_free_rigid_body(new_body);
            return NULL;
        }
        new_body->desc.shape_count = desc->shape_count;
        new_body->desc.shape_capacity = desc->shape_count;

        for (size_t i = 0; i < desc->shape_count; ++i) {
            new_body->desc.collision_shapes[i] = desc->collision_shapes[i];
            // Eğer mesh_id varsa kopyala
            if (desc->collision_shapes[i].type == FE_COLLISION_SHAPE_MESH || desc->collision_shapes[i].type == FE_COLLISION_SHAPE_CONVEX_HULL) {
                fe_string_init(&new_body->desc.collision_shapes[i].data.mesh.mesh_id, desc->collision_shapes[i].data.mesh.mesh_id.data);
            }
            // TODO: Chaos API'ye şekli ekle ve tutaç al
            // new_body->desc.collision_shapes[i].chaos_shape_handle = Chaos::CreateShape(...);
            new_body->desc.collision_shapes[i].chaos_shape_handle = (void*)((uintptr_t)i + 1); // Mock handle
        }
    }

    // TODO: Chaos API'ye rijit cismi ekle ve tutaç al
    // Örneğin: new_body->chaos_actor_handle = g_chaos_world_state.chaos_scene_handle->AddActor(...);
    new_body->chaos_actor_handle = (void*)((uintptr_t)new_body + 1); // Mock handle

    // Hash map'e ekle
    fe_hash_map_insert(g_chaos_world_state.rigid_bodies_map, fe_string_copy(&new_body->id), new_body);

    FE_LOG_INFO("Created rigid body '%s' (ID: %s) of type %s.", new_body->desc.name.data, new_body->id.data, fe_physics_body_type_to_string(new_body->desc.type));
    return new_body;
}

fe_chaos_error_t fe_chaos_world_destroy_rigid_body(const char* body_id) {
    if (!g_chaos_world_state.is_initialized) {
        FE_LOG_ERROR("Chaos world not initialized. Cannot destroy rigid body.");
        return FE_CHAOS_NOT_INITIALIZED;
    }
    if (!body_id) {
        FE_LOG_ERROR("Cannot destroy rigid body: body_id is NULL.");
        return FE_CHAOS_INVALID_ARGUMENT;
    }

    fe_chaos_rigid_body_t* body = fe_hash_map_get(g_chaos_world_state.rigid_bodies_map, body_id);
    if (!body) {
        FE_LOG_WARN("Rigid body with ID '%s' not found for destruction.", body_id);
        return FE_CHAOS_INVALID_ARGUMENT;
    }

    fe_hash_map_remove(g_chaos_world_state.rigid_bodies_map, body_id);
    fe_chaos_free_rigid_body(body);

    FE_LOG_INFO("Destroyed rigid body with ID '%s'.", body_id);
    return FE_CHAOS_SUCCESS;
}

fe_chaos_error_t fe_chaos_world_get_rigid_body_position(const fe_chaos_rigid_body_t* body, fe_vec3* out_position) {
    if (!g_chaos_world_state.is_initialized || !body || !out_position) {
        return FE_CHAOS_INVALID_ARGUMENT;
    }
    // TODO: Chaos API'den gerçek pozisyonu al.
    // Örneğin: FChaosPhysicsActor* actor = (FChaosPhysicsActor*)body->chaos_actor_handle;
              *out_position = ConvertToFeVec3(actor->GetPosition());
    *out_position = (fe_vec3){body->desc.initial_position.x + fe_get_current_time_ms() * 0.0001f,
                              body->desc.initial_position.y,
                              body->desc.initial_position.z}; // Mock değişim
    return FE_CHAOS_SUCCESS;
}

fe_chaos_error_t fe_chaos_world_get_rigid_body_rotation(const fe_chaos_rigid_body_t* body, fe_quat* out_rotation) {
    if (!g_chaos_world_state.is_initialized || !body || !out_rotation) {
        return FE_CHAOS_INVALID_ARGUMENT;
    }
    // TODO: Chaos API'den gerçek rotasyonu al.
    *out_rotation = body->desc.initial_rotation; // Mock değer
    return FE_CHAOS_SUCCESS;
}

fe_chaos_error_t fe_chaos_world_get_rigid_body_transform(const fe_chaos_rigid_body_t* body, fe_mat4* out_transform) {
    if (!g_chaos_world_state.is_initialized || !body || !out_transform) {
        return FE_CHAOS_INVALID_ARGUMENT;
    }
    fe_vec3 pos;
    fe_quat rot;
    fe_chaos_world_get_rigid_body_position(body, &pos);
    fe_chaos_world_get_rigid_body_rotation(body, &rot);
    fe_mat4_from_pos_quat(&pos, &rot, out_transform); // Kendi matematik kütüphanenizden
    return FE_CHAOS_SUCCESS;
}

fe_chaos_error_t fe_chaos_world_set_rigid_body_position(fe_chaos_rigid_body_t* body, fe_vec3 new_position) {
    if (!g_chaos_world_state.is_initialized || !body) {
        return FE_CHAOS_INVALID_ARGUMENT;
    }
    // TODO: Chaos API'sinde pozisyonu ayarla.
    // Bu genellikle sadece kinematik cisimler için doğrudan ayarlanabilir.
    // Dinamik cisimler için kuvvet veya impuls kullanılmalı.
    // Örneğin: ((FChaosPhysicsActor*)body->chaos_actor_handle)->SetPosition(ConvertFromFeVec3(new_position));
    body->desc.initial_position = new_position; // Mock
    return FE_CHAOS_SUCCESS;
}

fe_chaos_error_t fe_chaos_world_set_rigid_body_rotation(fe_chaos_rigid_body_t* body, fe_quat new_rotation) {
    if (!g_chaos_world_state.is_initialized || !body) {
        return FE_CHAOS_INVALID_ARGUMENT;
    }
    // TODO: Chaos API'sinde rotasyonu ayarla.
    body->desc.initial_rotation = new_rotation; // Mock
    return FE_CHAOS_SUCCESS;
}

fe_chaos_error_t fe_chaos_world_apply_force(fe_chaos_rigid_body_t* body, fe_vec3 force) {
    if (!g_chaos_world_state.is_initialized || !body) {
        return FE_CHAOS_INVALID_ARGUMENT;
    }
    // TODO: Chaos API'ye kuvvet uygula.
    // Örneğin: ((FChaosPhysicsActor*)body->chaos_actor_handle)->AddForce(ConvertFromFeVec3(force));
    FE_LOG_DEBUG("Applied force {%.2f, %.2f, %.2f} to body %s.", force.x, force.y, force.z, body->id.data);
    return FE_CHAOS_SUCCESS;
}

fe_chaos_error_t fe_chaos_world_apply_impulse(fe_chaos_rigid_body_t* body, fe_vec3 impulse) {
    if (!g_chaos_world_state.is_initialized || !body) {
        return FE_CHAOS_INVALID_ARGUMENT;
    }
    // TODO: Chaos API'ye itme uygula.
    FE_LOG_DEBUG("Applied impulse {%.2f, %.2f, %.2f} to body %s.", impulse.x, impulse.y, impulse.z, body->id.data);
    return FE_CHAOS_SUCCESS;
}

fe_chaos_error_t fe_chaos_world_apply_torque(fe_chaos_rigid_body_t* body, fe_vec3 torque) {
    if (!g_chaos_world_state.is_initialized || !body) {
        return FE_CHAOS_INVALID_ARGUMENT;
    }
    // TODO: Chaos API'ye tork uygula.
    FE_LOG_DEBUG("Applied torque {%.2f, %.2f, %.2f} to body %s.", torque.x, torque.y, torque.z, body->id.data);
    return FE_CHAOS_SUCCESS;
}

fe_chaos_error_t fe_chaos_world_apply_angular_impulse(fe_chaos_rigid_body_t* body, fe_vec3 angular_impulse) {
    if (!g_chaos_world_state.is_initialized || !body) {
        return FE_CHAOS_INVALID_ARGUMENT;
    }
    // TODO: Chaos API'ye açısal itme uygula.
    FE_LOG_DEBUG("Applied angular impulse {%.2f, %.2f, %.2f} to body %s.", angular_impulse.x, angular_impulse.y, angular_impulse.z, body->id.data);
    return FE_CHAOS_SUCCESS;
}

fe_chaos_rigid_body_t* fe_chaos_world_get_rigid_body(const char* body_id) {
    if (!g_chaos_world_state.is_initialized || !body_id) {
        return NULL;
    }
    return fe_hash_map_get(g_chaos_world_state.rigid_bodies_map, body_id);
}
