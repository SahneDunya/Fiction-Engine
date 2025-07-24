#ifndef FE_PHYSICS_MANAGER_H
#define FE_PHYSICS_MANAGER_H

#include <cglm/cglm.h> // GLM kütüphanesini kullanıyoruz, vektör ve matris işlemleri için
#include <stdbool.h>   // bool tipi için

// --- Yapı Tanımlamaları ---

// İleri bildirimler (Circular Dependency'yi önlemek için)
typedef struct FeRigidbody FeRigidbody;
typedef struct FeCollider FeCollider;

// Fiziksel Materyal (Sürtünme, Esneklik vb.)
typedef struct FePhysicsMaterial {
    float staticFriction;  // Statik sürtünme katsayısı
    float dynamicFriction; // Dinamik sürtünme katsayısı
    float restitution;     // Esneklik katsayısı (zıplama)
} FePhysicsMaterial;

// Çarpışma Şekli Tipi
typedef enum FeColliderType {
    FE_COLLIDER_SPHERE,
    FE_COLLIDER_BOX,
    FE_COLLIDER_CAPSULE
    // FE_COLLIDER_MESH // İleride eklenebilir
} FeColliderType;

// Çarpışma Nesnesi (Collider)
struct FeCollider {
    FeColliderType type; // Çarpışma tipi
    FePhysicsMaterial material; // Çarpışma materyali
    union {
        // Küre Çarpıştırıcısı
        struct {
            float radius;
        } sphere;
        // Kutu Çarpıştırıcısı
        struct {
            vec3 halfExtents; // Yarım boyutlar (x, y, z)
        } box;
        // Kapsül Çarpıştırıcısı
        struct {
            float radius;
            float height; // Toplam yükseklik (silindirik kısım + yarım küreler)
        } capsule;
    } shape;
    // Göreceli konum ve dönüş (Rigidbody'ye göre)
    vec3 localOffset;
    versor localRotation; // Quaternion
};

// Rijit Cisim (Rigidbody)
// Dinamik fiziksel etkileşimler için
struct FeRigidbody {
    unsigned int id;           // Benzersiz ID
    bool isActive;             // Aktif mi? (Simülasyona dahil mi?)
    bool isStatic;             // Statik mi? (Hareket etmez, diğer nesnelerle çarpışır)
    bool useGravity;           // Yerçekimi etkilensin mi?

    vec3 position;             // Konum
    versor rotation;           // Dönüş (Quaternion)

    vec3 linearVelocity;       // Doğrusal hız
    vec3 angularVelocity;      // Açısal hız (rad/s)

    float mass;                // Kütle
    float inverseMass;         // Ters kütle (performans için)
    mat4 inverseInertiaTensor; // Ters eylemsizlik tensörü (dönüş için)

    vec3 forceAccumulator;     // Bu adımda biriken kuvvetler
    vec3 torqueAccumulator;    // Bu adımda biriken torklar

    FeCollider collider;       // Bu cisme ait çarpışma nesnesi

    // Bağlantılar (Gerektiğinde başka bileşenlere referanslar eklenebilir, örn: rendering component ID)
    void* userData; // Genel amaçlı kullanıcı verisi
};

// Çarpışma Bilgisi
// İki nesne arasındaki çarpışma detaylarını içerir
typedef struct FeCollisionInfo {
    FeRigidbody* rbA;             // Çarpan ilk rijit cisim
    FeRigidbody* rbB;             // Çarpan ikinci rijit cisim
    vec3 normal;                  // Çarpışma normali (rbA'dan rbB'ye doğru)
    vec3 contactPointA;           // Çarpışma noktası (rbA yüzeyinde)
    vec3 contactPointB;           // Çarpışma noktası (rbB yüzeyinde)
    float penetrationDepth;       // İç içe geçme miktarı
    //float impulse;                // Uygulanan impuls (çözüm sonrası)
} FeCollisionInfo;

// --- Fonksiyon Deklarasyonları ---

// Fizik Yöneticisini Başlatır
// collisionPairsCapacity: Çarpışma testlerinin yapıldığı maksimum çift sayısı
// maxRigidbodies: Yöneticinin yönetebileceği maksimum rijit cisim sayısı
void fePhysicsManagerInit(unsigned int collisionPairsCapacity, unsigned int maxRigidbodies);

// Fizik Yöneticisini Temizler ve Belleği Serbest Bırakır
void fePhysicsManagerShutdown();

// Fizik Dünyası Parametrelerini Ayarlar
void fePhysicsManagerSetGravity(vec3 gravity);

// Yeni bir Rijit Cisim Oluşturur ve Motor'a Ekler
// Dönüş: Oluşturulan Rijit Cismin pointer'ı (NULL ise hata)
FeRigidbody* fePhysicsManagerCreateRigidbody(vec3 position, versor rotation, float mass, FeCollider collider, bool isStatic, bool useGravity);

// Mevcut bir Rijit Cismi Fizik Motorundan Kaldırır
// rb: Kaldırılacak rijit cisim
void fePhysicsManagerRemoveRigidbody(FeRigidbody* rb);

// Fizik Motorunu Belirtilen Zaman Adımı Kadar İlerletir
// deltaTime: Geçen süre (saniye cinsinden)
void fePhysicsManagerUpdate(float deltaTime);

// Rijit Cisme Kuvvet Uygular (merkezden)
void feRigidbodyApplyForce(FeRigidbody* rb, vec3 force);

// Rijit Cisme Belirli Bir Noktadan Kuvvet Uygular (tork oluşturabilir)
void feRigidbodyApplyForceAtPoint(FeRigidbody* rb, vec3 force, vec3 point);

// Rijit Cisme Tork Uygular
void feRigidbodyApplyTorque(FeRigidbody* rb, vec3 torque);

// Rijit Cismin Eylemsizlik Tensörünü Hesaplar (basit şekiller için)
void feRigidbodyCalculateInertiaTensor(FeRigidbody* rb);

// Rijit Cismin Transformasyon Matrisini Alır (konum ve dönüşü birleştirir)
void feRigidbodyGetTransformMatrix(const FeRigidbody* rb, mat4 outMatrix);

// Çarpışma Kontrol Fonksiyonları (internal, dışarıdan çağrılmamalı)
// Bu fonksiyonlar fePhysicsManagerUpdate içinde kullanılacaktır
bool feDetectCollision_SphereSphere(const FeRigidbody* rbA, const FeRigidbody* rbB, FeCollisionInfo* outInfo);
bool feDetectCollision_BoxBox(const FeRigidbody* rbA, const FeRigidbody* rbB, FeCollisionInfo* outInfo);
bool feDetectCollision_SphereBox(const FeRigidbody* rbA, const FeRigidbody* rbB, FeCollisionInfo* outInfo);
// ... Diğer kombinasyonlar eklenecek ...

// Çarpışma Çözümleme Fonksiyonu (internal)
void feResolveCollision(FeCollisionInfo* collision);

#endif // FE_PHYSICS_MANAGER_H
