#include "fe_world.h"
#include <stdio.h>    // printf için
#include <stdlib.h>   // malloc, free için
#include <string.h>   // memset için

// --- API Fonksiyon Uygulamaları ---

FeWorld* feWorldCreate(unsigned int collisionPairsCapacity, unsigned int maxRigidbodies, float fixedDeltaTime) {
    FeWorld* newWorld = (FeWorld*)malloc(sizeof(FeWorld));
    if (!newWorld) {
        fprintf(stderr, "Hata: Fizik dünyası için bellek ayrılamadı.\n");
        return NULL;
    }

    memset(newWorld, 0, sizeof(FeWorld)); // Belleği sıfırla

    // Varsayılan değerleri ayarla
    glm_vec3_copy((vec3){0.0f, -9.81f, 0.0f}, newWorld->gravity); // Varsayılan yerçekimi
    newWorld->fixedDeltaTime = fixedDeltaTime;
    newWorld->accumulator = 0.0f;

    // Physics Manager'ı başlat (dünya oluşturulduğunda)
    fePhysicsManagerInit(collisionPairsCapacity, maxRigidbodies);
    fePhysicsManagerSetGravity(newWorld->gravity); // Physics Manager'a yerçekimini ayarla

    printf("Fizik Dünyası başarıyla oluşturuldu.\n");
    return newWorld;
}

void feWorldDestroy(FeWorld* world) {
    if (!world) return;

    // Physics Manager'ı kapat (dünya yok edildiğinde)
    fePhysicsManagerShutdown();

    free(world);
    printf("Fizik Dünyası yok edildi.\n");
}

void feWorldSetGravity(FeWorld* world, vec3 gravity) {
    if (!world) return;
    glm_vec3_copy(gravity, world->gravity);
    fePhysicsManagerSetGravity(world->gravity); // Physics Manager'a da bildir
}

FeRigidbody* feWorldAddRigidbody(FeWorld* world, vec3 position, versor rotation, float mass, FeCollider collider, bool isStatic, bool useGravity) {
    if (!world) {
        fprintf(stderr, "Hata: Geçersiz fizik dünyası. Rijit cisim eklenemiyor.\n");
        return NULL;
    }
    // Rijit cismi doğrudan Physics Manager'a ekle
    return fePhysicsManagerCreateRigidbody(position, rotation, mass, collider, isStatic, useGravity);
}

void feWorldRemoveRigidbody(FeWorld* world, FeRigidbody* rb) {
    if (!world) return;
    // Rijit cismi doğrudan Physics Manager'dan kaldır
    fePhysicsManagerRemoveRigidbody(rb);
}

void feWorldUpdate(FeWorld* world, float deltaTime) {
    if (!world) {
        fprintf(stderr, "Hata: Geçersiz fizik dünyası. Güncelleme yapılamıyor.\n");
        return;
    }

    world->accumulator += deltaTime; // Geçen gerçek zamanı biriktir

    // Sabit zaman adımı döngüsü
    // Bu, fizik simülasyonunun her zaman aynı zaman aralığında çalışmasını sağlar,
    // böylece farklı kare hızlarında (FPS) bile tutarlı fiziksel davranış elde edilir.
    while (world->accumulator >= world->fixedDeltaTime) {
        fePhysicsManagerUpdate(world->fixedDeltaTime);
        world->accumulator -= world->fixedDeltaTime;
    }

    // İsteğe bağlı: Kalan zamanı (accumulator) interpolate edebilirsiniz.
    // Bu, görsel güncellemelerin fizik güncellemeleri arasındaki boşlukları doldurmasına yardımcı olur.
    // Örneğin: float alpha = world->accumulator / world->fixedDeltaTime;
    // Nesnelerin görsel konumlarını (alpha * next_pos + (1-alpha) * current_pos) ile interpolasyon yapın.
}
