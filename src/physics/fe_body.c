#include "fe_body.h"
#include <stdio.h>    // printf için
#include <stdlib.h>   // malloc, free için
#include <string.h>   // memset için
#include <math.h>     // fmaxf, fminf vb. için

// --- Dahili Yardımcı Fonksiyonlar ---
// (İleride burada fiziksel özellik hesaplamaları veya yardımcı matris dönüşümleri olabilir)

// --- API Fonksiyon Uygulamaları ---

FeRigidbody* feRigidbodyCreate(FeCollider collider, vec3 position, versor rotation, float mass, bool isStatic, bool useGravity) {
    FeRigidbody* newRb = (FeRigidbody*)malloc(sizeof(FeRigidbody));
    if (!newRb) {
        fprintf(stderr, "Hata: Rijit cisim için bellek ayrılamadı.\n");
        return NULL;
    }

    memset(newRb, 0, sizeof(FeRigidbody)); // Belleği sıfırla

    // Benzersiz bir ID atama (PhysicsManager'dan bağımsız, basit bir örnek)
    // Gerçek bir motor, ID atamasını merkezi bir sistemden yapmalıdır.
    static unsigned int globalRigidbodyIdCounter = 0;
    newRb->id = globalRigidbodyIdCounter++;

    newRb->isActive = true;
    newRb->isStatic = isStatic;
    newRb->useGravity = useGravity;

    glm_vec3_copy(position, newRb->position);
    glm_quat_copy(rotation, newRb->rotation);

    glm_vec3_zero(newRb->linearVelocity);
    glm_vec3_zero(newRb->angularVelocity);

    newRb->mass = isStatic ? 0.0f : mass; // Statik cisimlerin kütlesi sonsuzdur (ters kütle 0)
    newRb->inverseMass = isStatic ? 0.0f : (1.0f / mass);
    if (newRb->mass < 0.0001f && !isStatic) { // Kütle çok küçükse veya 0 ise ve statik değilse, uyarı ver veya statik yap
        fprintf(stderr, "Uyarı: Rijit cisim kütlesi çok küçük veya sıfır. Statik olarak kabul edilebilir.\n");
        newRb->inverseMass = 0.0f;
    }


    // Collider'ı kopyala
    newRb->collider = collider;

    // Eylemsizlik tensörünü hesapla (objenin kendi lokal uzayında)
    feRigidbodyCalculateInertiaTensor(newRb);

    glm_vec3_zero(newRb->forceAccumulator);
    glm_vec3_zero(newRb->torqueAccumulator);

    newRb->userData = NULL; // Varsayılan olarak NULL

    printf("Rijit cisim %d oluşturuldu (Kütle: %.2f, Statik: %s, Çarpıştırıcı Tipi: %d)\n",
           newRb->id, newRb->mass, newRb->isStatic ? "Evet" : "Hayır", newRb->collider.type);

    return newRb;
}

void feRigidbodyDestroy(FeRigidbody* rb) {
    if (rb) {
        printf("Rijit cisim %d yok edildi.\n", rb->id);
        free(rb);
    }
}

void feRigidbodyApplyForce(FeRigidbody* rb, vec3 force) {
    if (!rb || rb->isStatic) return;
    glm_vec3_add(rb->forceAccumulator, force, rb->forceAccumulator);
}

void feRigidbodyApplyForceAtPoint(FeRigidbody* rb, vec3 force, vec3 point) {
    if (!rb || rb->isStatic) return;

    feRigidbodyApplyForce(rb, force); // Doğrusal kuvvet

    // Tork hesapla: r x F
    vec3 r; // Cismin merkezinden kuvvetin uygulama noktasına vektör
    glm_vec3_sub(point, rb->position, r);
    vec3 torque;
    glm_vec3_cross(r, force, torque);
    feRigidbodyApplyTorque(rb, torque);
}

void feRigidbodyApplyTorque(FeRigidbody* rb, vec3 torque) {
    if (!rb || rb->isStatic) return;
    glm_vec3_add(rb->torqueAccumulator, torque, rb->torqueAccumulator);
}

void feRigidbodyCalculateInertiaTensor(FeRigidbody* rb) {
    // Statik cisimlerin eylemsizlik tensörü sonsuzdur (tersi 0)
    if (rb->isStatic) {
        glm_mat4_zero(rb->inverseInertiaTensor);
        return;
    }

    mat4 inertiaTensor; // Cismin lokal uzayındaki eylemsizlik tensörü
    glm_mat4_zero(inertiaTensor); // Başlangıçta sıfırla
    inertiaTensor[3][3] = 1.0f; // Homojen koordinatlar için

    switch (rb->collider.type) {
        case FE_COLLIDER_SPHERE: {
            float I = (2.0f / 5.0f) * rb->mass * (rb->collider.shape.sphere.radius * rb->collider.shape.sphere.radius);
            inertiaTensor[0][0] = I;
            inertiaTensor[1][1] = I;
            inertiaTensor[2][2] = I;
            break;
        }
        case FE_COLLIDER_BOX: {
            vec3 halfExtents = rb->collider.shape.box.halfExtents;
            float x = 2.0f * halfExtents[0];
            float y = 2.0f * halfExtents[1];
            float z = 2.0f * halfExtents[2];

            inertiaTensor[0][0] = (1.0f / 12.0f) * rb->mass * (y*y + z*z);
            inertiaTensor[1][1] = (1.0f / 12.0f) * rb->mass * (x*x + z*z);
            inertiaTensor[2][2] = (1.0f / 12.0f) * rb->mass * (x*x + y*y);
            // Köşegen dışı elemanlar sıfır (aligned box)
            break;
        }
        case FE_COLLIDER_CAPSULE: {
            // Kapsül için eylemsizlik tensörü daha karmaşıktır.
            // Bu sadece bir basitleştirme veya yer tutucu.
            // Kapsülün silindir ve yarım küre parçalarının birleşimi hesaplanmalıdır.
            // Genellikle en basit yaklaşım, kapsülü bir silindir veya küre gibi ele almaktır.
            // Y ekseni boyunca bir kapsül varsayalım.
            float r = rb->collider.shape.capsule.radius;
            float h = rb->collider.shape.capsule.height; // Silindirik kısımın yüksekliği
            float m = rb->mass;

            // Bu formüller silindir ve yarım kürelerin eylemsizliklerini birleştirir.
            // Detaylı bir kaynak için fizik motoru dokümanlarına bakılmalı.
            // Basitlik adına, burada approximate bir değer kullanalım veya daha sonra iyileştirelim.
            float I_x = (0.25f * m * r*r) + (m * h*h / 12.0f); // Silindir için
            float I_y = (0.5f * m * r*r); // Silindir için
            float I_z = I_x; // Silindir için

            // Yarım kürelerin katkısı da eklenmeli. Bu kısım atlanmıştır.
            // Şimdilik sadece yaklaşık bir değer atayalım.
            float approximateI = (m * r*r / 2.0f) + (m * h*h / 12.0f); // Çok basit bir yaklaşım
            inertiaTensor[0][0] = approximateI;
            inertiaTensor[1][1] = approximateI;
            inertiaTensor[2][2] = approximateI;

            break;
        }
        default:
            fprintf(stderr, "Uyarı: Bilinmeyen çarpıştırıcı tipi için eylemsizlik tensörü hesaplanamıyor. Varsayılan kullanılıyor.\n");
            glm_mat4_identity(inertiaTensor); // Varsayılan olarak birim matris
            break;
    }

    // Dünya uzayındaki ters eylemsizlik tensörü = R * I_body_inverse * R^T
    // Bu işlem her frame'de rijit cismin dönüşü değiştikçe yapılmalıdır.
    // Ancak, başlangıçta sadece lokal uzaydaki eylemsizlik tensörünün tersini alıyoruz.
    // PhysicsManager'da entegrasyon sırasında bu tensör güncellenmelidir.
    // Şimdilik sadece lokal tensörün tersini alıyoruz:
    glm_mat4_inv(inertiaTensor, rb->inverseInertiaTensor);
}

void feRigidbodyGetTransformMatrix(const FeRigidbody* rb, mat4 outMatrix) {
    mat4 rotationMatrix;
    glm_quat_mat4(rb->rotation, rotationMatrix); // Quaternion'dan dönüşüm matrisine
    glm_mat4_copy(rotationMatrix, outMatrix);
    glm_mat4_translate(outMatrix, rb->position); // Konumu matrise uygula
}

void feRigidbodyGetColliderWorldCenter(const FeRigidbody* rb, vec3 outCenter) {
    vec3 localOffsetWorld;
    // Yerel ofseti dünya uzayına dönüştür
    mat4 rotationMat;
    glm_quat_mat4(rb->rotation, rotationMat);
    glm_mat4_mulv3(rotationMat, rb->collider.localOffset, 0.0f, localOffsetWorld);

    // Rijit cismin konumu + dönüştürülmüş yerel ofset
    glm_vec3_add(rb->position, localOffsetWorld, outCenter);
}
