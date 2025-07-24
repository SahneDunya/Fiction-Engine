#ifndef FE_BODY_H
#define FE_BODY_H

#include <cglm/cglm.h> // GLM kütüphanesini kullanıyoruz, vektör ve matris işlemleri için
#include <stdbool.h>   // bool tipi için

// --- Yapı Tanımlamaları ---

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
// Bir rijit cisme (FeRigidbody) bağlıdır ve fiziksel geometrisini tanımlar.
typedef struct FeCollider {
    FeColliderType type;       // Çarpışma tipi (küre, kutu vb.)
    FePhysicsMaterial material; // Çarpışma materyali (sürtünme, esneklik)
    union { // Çarpıştırıcı tipine göre farklı şekil verileri
        struct {
            float radius;
        } sphere;
        struct {
            vec3 halfExtents; // Yarım boyutlar (x, y, z)
        } box;
        struct {
            float radius;
            float height; // Toplam yükseklik (silindirik kısım + yarım küreler)
        } capsule;
    } shape;
    vec3 localOffset;   // Rijit cisme göre göreceli konum
    versor localRotation; // Rijit cisme göre göreceli dönüş (Quaternion)
} FeCollider;

// Rijit Cisim (Rigidbody)
// Dinamik fiziksel etkileşimler için temel nesne.
typedef struct FeRigidbody {
    unsigned int id;           // Benzersiz ID
    bool isActive;             // Aktif mi? (Simülasyona dahil mi?)
    bool isStatic;             // Statik mi? (Hareket etmez, diğer nesnelerle çarpışır)
    bool useGravity;           // Yerçekimi etkilensin mi?

    vec3 position;             // Dünya uzayındaki konum
    versor rotation;           // Dünya uzayındaki dönüş (Quaternion)

    vec3 linearVelocity;       // Doğrusal hız
    vec3 angularVelocity;      // Açısal hız (rad/s)

    float mass;                // Kütle
    float inverseMass;         // Ters kütle (performans için: 1/mass)

    // Eylemsizlik tensörü (objenin dönüş hareketine karşı direnci)
    // Dünya uzayında her karede yeniden hesaplanmalı veya dönüşümle güncellenmeli.
    mat4 inverseInertiaTensor; // Dünya uzayındaki ters eylemsizlik tensörü

    vec3 forceAccumulator;     // Bu adımda biriken kuvvetler
    vec3 torqueAccumulator;    // Bu adımda biriken torklar

    FeCollider collider;       // Bu cisme ait çarpışma nesnesi

    void* userData;            // Genel amaçlı kullanıcı verisi (örn: oyun objesi pointer'ı)
} FeRigidbody;

// --- Fonksiyon Deklarasyonları ---

// Yeni bir Rijit Cisim oluşturur ve gerekli başlangıç değerlerini atar.
// collider: Cismin fiziksel şekli
// position: Başlangıç dünya konumu
// rotation: Başlangıç dünya dönüşü (Quaternion)
// mass: Cismin kütlesi (0 veya çok büyük değerler statik nesneler için kullanılabilir)
// isStatic: Cismin statik olup olmadığı (hareket etmez)
// useGravity: Cismin yerçekiminden etkilenip etkilenmediği
// Dönüş: Oluşturulan ve başlatılan FeRigidbody pointer'ı. Bellek sizin sorumluluğunuzdadur.
FeRigidbody* feRigidbodyCreate(FeCollider collider, vec3 position, versor rotation, float mass, bool isStatic, bool useGravity);

// Bir Rijit Cismin belleğini serbest bırakır.
void feRigidbodyDestroy(FeRigidbody* rb);

// Rijit Cisme Kuvvet Uygular (cismin merkezinden).
void feRigidbodyApplyForce(FeRigidbody* rb, vec3 force);

// Rijit Cisme Belirli Bir Noktadan Kuvvet Uygular (tork oluşturabilir).
// point: Kuvvetin uygulandığı dünya uzayı koordinatındaki nokta
void feRigidbodyApplyForceAtPoint(FeRigidbody* rb, vec3 force, vec3 point);

// Rijit Cisme Tork Uygular.
void feRigidbodyApplyTorque(FeRigidbody* rb, vec3 torque);

// Rijit Cismin Eylemsizlik Tensörünü Hesaplar.
// Bu fonksiyon genellikle cisim oluşturulduğunda ve kütle/şekil değiştiğinde çağrılır.
// Dinamik cisimler için her fizik adımında güncellenmesi gerekir.
void feRigidbodyCalculateInertiaTensor(FeRigidbody* rb);

// Rijit Cismin Transformasyon Matrisini (konum ve dönüşü birleştiren) hesaplar ve döndürür.
// outMatrix: Sonuç matrisinin yazılacağı mat4 pointer'ı
void feRigidbodyGetTransformMatrix(const FeRigidbody* rb, mat4 outMatrix);

// Rijit cismin dünya uzayındaki çarpıştırıcısının merkezini döndürür.
// outCenter: Sonuç vektörünün yazılacağı vec3 pointer'ı
void feRigidbodyGetColliderWorldCenter(const FeRigidbody* rb, vec3 outCenter);

#endif // FE_BODY_H
