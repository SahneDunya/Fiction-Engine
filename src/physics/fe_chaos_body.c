#include "physics/fe_chaos_body.h"
#include "physics/fe_chaos_world.h" // fe_chaos_world'deki dahili fonksiyonlara erişim için
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h"
#include "core/math/fe_math.h" // fe_vec3_zero, fe_quat_identity gibi helperlar için

#include <string.h> // memset, memcpy

// Sabit boyutta çarpışma şekli listesi için başlangıç kapasitesi
#define FE_CHAOS_BODY_DEFAULT_SHAPE_CAPACITY 4

// --- Dahili Yardımcı Fonksiyonlar (fe_chaos_world.c'den kopyalanmış veya çağrılmış) ---

// fe_chaos_world.c'deki static fonksiyonlara dışarıdan erişilemez, bu yüzden ya bunları
// public yapmak ya da burada sarıcı (wrapper) fonksiyonlar oluşturmak gerekir.
// Bu örnekte, fe_chaos_world.c'deki temel Chaos API fonksiyonlarını fe_chaos_body
// içinden çağırdığımızı varsayıyoruz.

// fe_chaos_world.c'deki fe_chaos_free_collision_shape fonksiyonuna erişebilmeliyiz.
// Eğer o static ise, burada kendi kopyasını veya benzerini yapmak gerekebilir.
// Bu örnek için, fe_chaos_world'den gelen fe_chaos_body_t yapısının zaten belleğini yönettiğini varsayalım.
// Dolayısıyla, fe_chaos_body_destroy sadece fe_chaos_world_destroy_rigid_body'yi çağırır.

// --- Fonksiyon Uygulamaları ---

fe_rigid_body_desc_t fe_chaos_body_create_desc() {
    fe_rigid_body_desc_t desc;
    fe_chaos_world_default_rigid_body_desc(&desc); // fe_chaos_world'deki varsayılanları kullan
    
    // Başlangıçta belirli bir kapasite tahsis et
    desc.collision_shapes = fe_malloc(FE_CHAOS_BODY_DEFAULT_SHAPE_CAPACITY * sizeof(fe_collision_shape_t), FE_MEM_TYPE_PHYSICS, __FILE__, __LINE__);
    if (!desc.collision_shapes) {
        FE_LOG_CRITICAL("Failed to allocate initial memory for collision shapes in desc.");
        desc.shape_capacity = 0; // Hata durumunda kapasiteyi sıfırla
    } else {
        desc.shape_capacity = FE_CHAOS_BODY_DEFAULT_SHAPE_CAPACITY;
    }
    desc.shape_count = 0; // Başlangıçta şekil yok
    
    fe_string_init(&desc.name, "DefaultRigidBody"); // Varsayılan isim
    return desc;
}

void fe_chaos_body_destroy_desc(fe_rigid_body_desc_t* desc) {
    if (!desc) return;

    fe_string_destroy(&desc->name); // İsim string'ini temizle

    if (desc->collision_shapes) {
        for (size_t i = 0; i < desc->shape_count; ++i) {
            // Mesh_id gibi dinamik olarak ayrılan string'leri temizle
            if (desc->collision_shapes[i].type == FE_COLLISION_SHAPE_MESH || 
                desc->collision_shapes[i].type == FE_COLLISION_SHAPE_CONVEX_HULL) {
                fe_string_destroy(&desc->collision_shapes[i].data.mesh.mesh_id);
            }
            // Not: Eğer Chaos_shape_handle burada doğrudan tutulup init ediliyorsa,
            // burada da serbest bırakılması gerekirdi. Ama şu anlık o fe_chaos_world'de yönetiliyor.
        }
        fe_free(desc->collision_shapes, FE_MEM_TYPE_PHYSICS, __FILE__, __LINE__);
        desc->collision_shapes = NULL;
    }
    desc->shape_count = 0;
    desc->shape_capacity = 0;
}

fe_chaos_error_t fe_chaos_body_add_collision_shape(fe_rigid_body_desc_t* desc, const fe_collision_shape_t* shape) {
    if (!desc || !shape) {
        FE_LOG_ERROR("Invalid arguments for adding collision shape.");
        return FE_CHAOS_INVALID_ARGUMENT;
    }

    // Kapasiteyi kontrol et ve gerekirse büyüt
    if (desc->shape_count >= desc->shape_capacity) {
        size_t new_capacity = desc->shape_capacity == 0 ? FE_CHAOS_BODY_DEFAULT_SHAPE_CAPACITY : desc->shape_capacity * 2;
        fe_collision_shape_t* new_shapes = fe_realloc(desc->collision_shapes,
                                                      new_capacity * sizeof(fe_collision_shape_t),
                                                      FE_MEM_TYPE_PHYSICS, __FILE__, __LINE__);
        if (!new_shapes) {
            FE_LOG_CRITICAL("Failed to reallocate memory for collision shapes.");
            return FE_CHAOS_OUT_OF_MEMORY;
        }
        desc->collision_shapes = new_shapes;
        desc->shape_capacity = new_capacity;
        FE_LOG_DEBUG("Resized rigid body desc shapes array to %zu capacity.", new_capacity);
    }

    // Şekli kopyala
    desc->collision_shapes[desc->shape_count] = *shape;
    // Eğer mesh_id varsa onu da kopyala
    if (shape->type == FE_COLLISION_SHAPE_MESH || shape->type == FE_COLLISION_SHAPE_CONVEX_HULL) {
        fe_string_init(&desc->collision_shapes[desc->shape_count].data.mesh.mesh_id, shape->data.mesh.mesh_id.data);
    }
    
    desc->shape_count++;
    return FE_CHAOS_SUCCESS;
}

fe_chaos_rigid_body_t* fe_chaos_body_create(const fe_rigid_body_desc_t* desc) {
    if (!desc) {
        FE_LOG_ERROR("Cannot create rigid body: description is NULL.");
        return NULL;
    }
    // fe_chaos_world modülü zaten rijit cismi oluşturma ve hash map'e ekleme işini yapıyor.
    // Burada sadece o fonksiyonu çağırıyoruz.
    return fe_chaos_world_create_rigid_body(desc);
}

fe_chaos_error_t fe_chaos_body_destroy(fe_chaos_rigid_body_t* body) {
    if (!body) {
        FE_LOG_ERROR("Cannot destroy rigid body: body is NULL.");
        return FE_CHAOS_INVALID_ARGUMENT;
    }
    // fe_chaos_world modülü zaten rijit cismi yok etme ve hash map'ten kaldırma işini yapıyor.
    // Burada sadece o fonksiyonu çağırıyoruz.
    return fe_chaos_world_destroy_rigid_body(body->id.data);
}


const char* fe_chaos_body_get_id(const fe_chaos_rigid_body_t* body) {
    if (!body) return NULL;
    return body->id.data;
}

fe_physics_body_type_t fe_chaos_body_get_type(const fe_chaos_rigid_body_t* body) {
    if (!body) return (fe_physics_body_type_t)0; // Hata değeri
    return body->desc.type;
}

float fe_chaos_body_get_mass(const fe_chaos_rigid_body_t* body) {
    if (!body) return 0.0f;
    return body->desc.mass;
}

fe_chaos_error_t fe_chaos_body_set_mass(fe_chaos_rigid_body_t* body, float mass) {
    if (!body) {
        FE_LOG_ERROR("Cannot set mass: body is NULL.");
        return FE_CHAOS_INVALID_ARGUMENT;
    }
    if (body->desc.type == FE_PHYSICS_BODY_STATIC) {
        FE_LOG_WARN("Attempted to set mass on a static body with ID '%s'. Mass will not have effect.", body->id.data);
        return FE_CHAOS_INVALID_ARGUMENT; // Statik cismin kütlesini ayarlamak anlamsız
    }
    body->desc.mass = mass;
    // TODO: Chaos API'sinde kütleyi güncelleme çağrısı (varsa).
    // FChaosPhysicsActor* actor = (FChaosPhysicsActor*)body->chaos_actor_handle;
    // actor->SetMass(mass);
    FE_LOG_DEBUG("Set mass of body '%s' to %.2f.", body->id.data, mass);
    return FE_CHAOS_SUCCESS;
}

fe_chaos_error_t fe_chaos_body_set_friction(fe_chaos_rigid_body_t* body, float static_friction, float dynamic_friction) {
    if (!body) {
        FE_LOG_ERROR("Cannot set friction: body is NULL.");
        return FE_CHAOS_INVALID_ARGUMENT;
    }
    body->desc.static_friction = static_friction;
    body->desc.dynamic_friction = dynamic_friction;
    // TODO: Chaos API'sinde sürtünme değerlerini güncelleme çağrısı.
    // Bu genellikle her bir çarpışma şekli için yapılır.
    FE_LOG_DEBUG("Set friction for body '%s': Static=%.2f, Dynamic=%.2f.", body->id.data, static_friction, dynamic_friction);
    return FE_CHAOS_SUCCESS;
}

fe_chaos_error_t fe_chaos_body_set_restitution(fe_chaos_rigid_body_t* body, float restitution) {
    if (!body) {
        FE_LOG_ERROR("Cannot set restitution: body is NULL.");
        return FE_CHAOS_INVALID_ARGUMENT;
    }
    body->desc.restitution = restitution;
    // TODO: Chaos API'sinde geri sekme değerini güncelleme çağrısı.
    // Bu da genellikle her bir çarpışma şekli için yapılır.
    FE_LOG_DEBUG("Set restitution for body '%s' to %.2f.", body->id.data, restitution);
    return FE_CHAOS_SUCCESS;
}

// Konum ve rotasyon fonksiyonları fe_chaos_world'deki karşılık gelen fonksiyonlara sarıcıdır.
fe_chaos_error_t fe_chaos_body_get_position(const fe_chaos_rigid_body_t* body, fe_vec3* out_position) {
    return fe_chaos_world_get_rigid_body_position(body, out_position);
}

fe_chaos_error_t fe_chaos_body_get_rotation(const fe_chaos_rigid_body_t* body, fe_quat* out_rotation) {
    return fe_chaos_world_get_rigid_body_rotation(body, out_rotation);
}

fe_chaos_error_t fe_chaos_body_get_transform(const fe_chaos_rigid_body_t* body, fe_mat4* out_transform) {
    return fe_chaos_world_get_rigid_body_transform(body, out_transform);
}

fe_chaos_error_t fe_chaos_body_set_position(fe_chaos_rigid_body_t* body, fe_vec3 new_position) {
    return fe_chaos_world_set_rigid_body_position(body, new_position);
}

fe_chaos_error_t fe_chaos_body_set_rotation(fe_chaos_rigid_body_t* body, fe_quat new_rotation) {
    return fe_chaos_world_set_rigid_body_rotation(body, new_rotation);
}

fe_chaos_error_t fe_chaos_body_apply_force(fe_chaos_rigid_body_t* body, fe_vec3 force) {
    return fe_chaos_world_apply_force(body, force);
}

fe_chaos_error_t fe_chaos_body_apply_impulse(fe_chaos_rigid_body_t* body, fe_vec3 impulse) {
    return fe_chaos_world_apply_impulse(body, impulse);
}

fe_chaos_error_t fe_chaos_body_apply_torque(fe_chaos_rigid_body_t* body, fe_vec3 torque) {
    return fe_chaos_world_apply_torque(body, torque);
}

fe_chaos_error_t fe_chaos_body_apply_angular_impulse(fe_chaos_rigid_body_t* body, fe_vec3 angular_impulse) {
    return fe_chaos_world_apply_angular_impulse(body, angular_impulse);
}
