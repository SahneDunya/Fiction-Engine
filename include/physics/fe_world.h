#ifndef FE_WORLD_H
#define FE_WORLD_H

#include "fe_physics_manager.h" // fe_physics_manager'ın yapılarını kullanacağız
#include <cglm/cglm.h>          // Vektör ve matris işlemleri için GLM

// --- Yapı Tanımlamaları ---

// Fizik Dünyası Yapısı
typedef struct FeWorld {
    vec3 gravity;                  // Dünya'nın yerçekimi vektörü
    float fixedDeltaTime;          // Fizik simülasyonu için sabit zaman adımı
    float accumulator;             // Sabit zaman adımı için biriktirici

    // Fizik yöneticisi tarafından yönetilen rigidbodies'ler, buraya doğrudan erişimden kaçınmalıyız.
    // PhysicsManager'ın iç yapısını dışarı sızdırmamak için doğrudan rigidbody listesini tutmuyoruz.
    // Ancak, dünya üzerindeki nesnelerle çalışmak için PhysicsManager'ı kullanacağız.
    // Kendi nesne yönetim mekanizmanızı isterseniz buraya ekleyebilirsiniz.
    // Örneğin: FeObject** objects; unsigned int numObjects;
} FeWorld;

// --- Fonksiyon Deklarasyonları ---

// Yeni bir Fizik Dünyası Oluşturur ve Başlatır
// collisionPairsCapacity: Çarpışma yöneticisi için maksimum çarpışma çifti kapasitesi
// maxRigidbodies: Çarpışma yöneticisi için maksimum rigidbody kapasitesi
// fixedDeltaTime: Fizik güncellemesi için sabit zaman adımı (örn: 1.0f / 60.0f)
// Dönüş: Oluşturulan FeWorld pointer'ı (NULL ise hata)
FeWorld* feWorldCreate(unsigned int collisionPairsCapacity, unsigned int maxRigidbodies, float fixedDeltaTime);

// Bir Fizik Dünyasını Temizler ve Belleği Serbest Bırakır
void feWorldDestroy(FeWorld* world);

// Fizik Dünyasının Yerçekimi Vektörünü Ayarlar
void feWorldSetGravity(FeWorld* world, vec3 gravity);

// Fizik Dünyasına Yeni Bir Rijit Cisim Ekler
// Konum, dönüş, kütle, çarpıştırıcı, statik mi, yerçekimi etkilensin mi?
// Dönüş: Oluşturulan FeRigidbody pointer'ı (NULL ise hata)
FeRigidbody* feWorldAddRigidbody(FeWorld* world, vec3 position, versor rotation, float mass, FeCollider collider, bool isStatic, bool useGravity);

// Fizik Dünyasından Bir Rijit Cismi Kaldırır
void feWorldRemoveRigidbody(FeWorld* world, FeRigidbody* rb);

// Fizik Dünyasını Günceller
// deltaTime: Gerçekte geçen zaman (oyun döngüsünden gelen)
void feWorldUpdate(FeWorld* world, float deltaTime);

#endif // FE_WORLD_H
