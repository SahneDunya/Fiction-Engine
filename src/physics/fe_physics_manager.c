#include "fe_physics_manager.h"
#include <stdio.h>    // printf için
#include <stdlib.h>   // malloc, free için
#include <string.h>   // memset için
#include <math.h>     // sqrt, fabsf vb. için

// --- Dahili Global Değişkenler ---
// Bu değişkenler, fizik yöneticisinin durumunu tutar
static struct {
    vec3 gravity;                  // Yerçekimi vektörü
    FeRigidbody** rigidbodies;     // Tüm rijit cisimlerin listesi (dynamik dizi)
    unsigned int numRigidbodies;   // Mevcut rijit cisim sayısı
    unsigned int maxRigidbodies;   // Maksimum rijit cisim sayısı
    unsigned int nextRigidbodyId;  // Bir sonraki rijit cisim ID'si

    // Çarpışma bilgi depolaması
    FeCollisionInfo* collisionInfos; // Bu adımdaki tüm çarpışmaların listesi
    unsigned int numCollisionInfos;  // Mevcut çarpışma sayısı
    unsigned int maxCollisionInfos;  // Maksimum çarpışma sayısı (çift sayısı)

    bool initialized;              // Yöneticinin başlatılıp başlatılmadığı
} g_PhysicsManager;

// --- Dahili Yardımcı Fonksiyonlar ---

// Rijit cisimlerin güncellenmesi (konum, hız, ivme)
static void feIntegrateRigidbodies(float deltaTime) {
    // Verle entegrasyonu (daha stabil)
    for (unsigned int i = 0; i < g_PhysicsManager.numRigidbodies; ++i) {
        FeRigidbody* rb = g_PhysicsManager.rigidbodies[i];
        if (!rb->isActive || rb->isStatic) continue;

        // Doğrusal ivme hesaplama
        vec3 linearAcceleration;
        glm_vec3_scale(g_PhysicsManager.gravity, rb->useGravity ? 1.0f : 0.0f, linearAcceleration); // Yerçekimi
        glm_vec3_muladds(rb->forceAccumulator, rb->inverseMass, linearAcceleration); // Uygulanan kuvvetler

        // Açısal ivme hesaplama
        vec3 angularAcceleration;
        glm_mat4_mulv3(rb->inverseInertiaTensor, rb->torqueAccumulator, 1.0f, angularAcceleration);

        // Hız güncelleme (v += a * dt)
        glm_vec3_muladds(linearAcceleration, deltaTime, rb->linearVelocity);
        glm_vec3_muladds(angularAcceleration, deltaTime, rb->angularVelocity);

        // Sürtünme veya sönümleme (damping) eklemek isteyebilirsiniz
        // glm_vec3_scale(rb->linearVelocity, powf(0.99, deltaTime), rb->linearVelocity);
        // glm_vec3_scale(rb->angularVelocity, powf(0.98, deltaTime), rb->angularVelocity);

        // Konum güncelleme (p += v * dt)
        glm_vec3_muladds(rb->linearVelocity, deltaTime, rb->position);

        // Dönüş güncelleme (quaternion ile)
        versor q_delta;
        // q_delta = 0.5 * angularVelocity * rotation
        glm_quat_init(q_delta, 0.5f * rb->angularVelocity[0], 0.5f * rb->angularVelocity[1], 0.5f * rb->angularVelocity[2], 0.0f);
        glm_quat_mul(q_delta, rb->rotation, q_delta); // q_delta = q_delta * rb->rotation
        glm_quat_muladds(q_delta, deltaTime, rb->rotation); // rb->rotation += q_delta * deltaTime
        glm_quat_normalize(rb->rotation); // Normalleştirme önemli!

        // Akümülatörleri sıfırla
        glm_vec3_zero(rb->forceAccumulator);
        glm_vec3_zero(rb->torqueAccumulator);
    }
}

// Tüm olası çarpışma çiftlerini test eder
static void feBroadPhase() {
    g_PhysicsManager.numCollisionInfos = 0; // Bir önceki adımdaki çarpışmaları temizle

    for (unsigned int i = 0; i < g_PhysicsManager.numRigidbodies; ++i) {
        for (unsigned int j = i + 1; j < g_PhysicsManager.numRigidbodies; ++j) {
            FeRigidbody* rbA = g_PhysicsManager.rigidbodies[i];
            FeRigidbody* rbB = g_PhysicsManager.rigidbodies[j];

            // Her iki cisim de aktif ve statik değilse veya biri statik diğeri aktifse
            if ((!rbA->isActive || !rbB->isActive) || (rbA->isStatic && rbB->isStatic)) {
                continue;
            }

            // Çarpışma türüne göre uygun tespit fonksiyonunu çağır
            bool collisionDetected = false;
            FeCollisionInfo currentCollision;
            currentCollision.rbA = rbA;
            currentCollision.rbB = rbB;

            // TODO: Daha genel bir çarpışma tespit sistemi kurulmalı (Dispatch Table veya switch/case)
            // Şimdilik sadece küre-küre çarpışmasını örnekleyelim
            if (rbA->collider.type == FE_COLLIDER_SPHERE && rbB->collider.type == FE_COLLIDER_SPHERE) {
                collisionDetected = feDetectCollision_SphereSphere(rbA, rbB, &currentCollision);
            } else if (rbA->collider.type == FE_COLLIDER_BOX && rbB->collider.type == FE_COLLIDER_BOX) {
                collisionDetected = feDetectCollision_BoxBox(rbA, rbB, &currentCollision);
            } else if ((rbA->collider.type == FE_COLLIDER_SPHERE && rbB->collider.type == FE_COLLIDER_BOX) ||
                       (rbA->collider.type == FE_COLLIDER_BOX && rbB->collider.type == FE_COLLIDER_SPHERE)) {
                // Sphere-Box çarpışması için sırayı standardize et
                if (rbA->collider.type == FE_COLLIDER_BOX) {
                    FeRigidbody* temp = rbA;
                    rbA = rbB;
                    rbB = temp;
                    // temp_rb_A ve temp_rb_B'yi kullanarak info'yu doldururken dikkat edin.
                    currentCollision.rbA = rbA; // Güncellenmiş rbA (sphere)
                    currentCollision.rbB = rbB; // Güncellenmiş rbB (box)
                }
                collisionDetected = feDetectCollision_SphereBox(rbA, rbB, &currentCollision);
            }
            // Diğer çarpışma kombinasyonları buraya eklenecek

            if (collisionDetected) {
                if (g_PhysicsManager.numCollisionInfos < g_PhysicsManager.maxCollisionInfos) {
                    g_PhysicsManager.collisionInfos[g_PhysicsManager.numCollisionInfos++] = currentCollision;
                } else {
                    fprintf(stderr, "Uyarı: Maksimum çarpışma sayısı aşıldı! Bazı çarpışmalar göz ardı edilebilir.\n");
                }
            }
        }
    }
}

// Çarpışmaları çözümler (penetrasyon ve impuls)
static void feNarrowPhaseAndResolution() {
    for (unsigned int i = 0; i < g_PhysicsManager.numCollisionInfos; ++i) {
        feResolveCollision(&g_PhysicsManager.collisionInfos[i]);
    }
}


// --- API Fonksiyon Uygulamaları ---

void fePhysicsManagerInit(unsigned int collisionPairsCapacity, unsigned int maxRigidbodies) {
    if (g_PhysicsManager.initialized) {
        fprintf(stderr, "Fizik yöneticisi zaten başlatıldı.\n");
        return;
    }

    g_PhysicsManager.gravity[0] = 0.0f;
    g_PhysicsManager.gravity[1] = -9.81f; // Varsayılan yerçekimi (Y ekseni aşağı)
    g_PhysicsManager.gravity[2] = 0.0f;

    g_PhysicsManager.maxRigidbodies = maxRigidbodies;
    g_PhysicsManager.rigidbodies = (FeRigidbody**)malloc(sizeof(FeRigidbody*) * maxRigidbodies);
    if (!g_PhysicsManager.rigidbodies) {
        fprintf(stderr, "Hata: Rijit cisimler için bellek ayrılamadı.\n");
        exit(EXIT_FAILURE);
    }
    g_PhysicsManager.numRigidbodies = 0;
    g_PhysicsManager.nextRigidbodyId = 0;

    g_PhysicsManager.maxCollisionInfos = collisionPairsCapacity;
    g_PhysicsManager.collisionInfos = (FeCollisionInfo*)malloc(sizeof(FeCollisionInfo) * collisionPairsCapacity);
    if (!g_PhysicsManager.collisionInfos) {
        fprintf(stderr, "Hata: Çarpışma bilgileri için bellek ayrılamadı.\n");
        free(g_PhysicsManager.rigidbodies);
        exit(EXIT_FAILURE);
    }
    g_PhysicsManager.numCollisionInfos = 0;

    g_PhysicsManager.initialized = true;
    printf("Fizik Yöneticisi başarıyla başlatıldı.\n");
}

void fePhysicsManagerShutdown() {
    if (!g_PhysicsManager.initialized) {
        fprintf(stderr, "Fizik yöneticisi başlatılmadı.\n");
        return;
    }

    // Tüm rijit cisimleri serbest bırak
    for (unsigned int i = 0; i < g_PhysicsManager.numRigidbodies; ++i) {
        free(g_PhysicsManager.rigidbodies[i]);
    }
    free(g_PhysicsManager.rigidbodies);
    g_PhysicsManager.rigidbodies = NULL;
    g_PhysicsManager.numRigidbodies = 0;

    free(g_PhysicsManager.collisionInfos);
    g_PhysicsManager.collisionInfos = NULL;
    g_PhysicsManager.numCollisionInfos = 0;
    g_PhysicsManager.maxCollisionInfos = 0;

    g_PhysicsManager.initialized = false;
    printf("Fizik Yöneticisi kapatıldı.\n");
}

void fePhysicsManagerSetGravity(vec3 gravity) {
    if (!g_PhysicsManager.initialized) {
        fprintf(stderr, "Fizik yöneticisi başlatılmadı. Yerçekimi ayarlanamaz.\n");
        return;
    }
    glm_vec3_copy(gravity, g_PhysicsManager.gravity);
}

FeRigidbody* fePhysicsManagerCreateRigidbody(vec3 position, versor rotation, float mass, FeCollider collider, bool isStatic, bool useGravity) {
    if (!g_PhysicsManager.initialized) {
        fprintf(stderr, "Hata: Fizik yöneticisi başlatılmadı. Rijit cisim oluşturulamıyor.\n");
        return NULL;
    }
    if (g_PhysicsManager.numRigidbodies >= g_PhysicsManager.maxRigidbodies) {
        fprintf(stderr, "Hata: Maksimum rijit cisim sayısına ulaşıldı (%d).\n", g_PhysicsManager.maxRigidbodies);
        return NULL;
    }

    FeRigidbody* newRb = (FeRigidbody*)malloc(sizeof(FeRigidbody));
    if (!newRb) {
        fprintf(stderr, "Hata: Rijit cisim için bellek ayrılamadı.\n");
        return NULL;
    }

    memset(newRb, 0, sizeof(FeRigidbody)); // Belleği sıfırla

    newRb->id = g_PhysicsManager.nextRigidbodyId++;
    newRb->isActive = true;
    newRb->isStatic = isStatic;
    newRb->useGravity = useGravity;

    glm_vec3_copy(position, newRb->position);
    glm_quat_copy(rotation, newRb->rotation);

    glm_vec3_zero(newRb->linearVelocity);
    glm_vec3_zero(newRb->angularVelocity);

    newRb->mass = isStatic ? 0.0f : mass; // Statik cisimlerin kütlesi sonsuzdur (veya 0 ters kütle)
    newRb->inverseMass = isStatic ? 0.0f : (1.0f / mass);

    // Collider'ı kopyala
    newRb->collider = collider;

    // Eylemsizlik tensörünü hesapla (şimdilik basit bir varsayılan)
    feRigidbodyCalculateInertiaTensor(newRb);

    glm_vec3_zero(newRb->forceAccumulator);
    glm_vec3_zero(newRb->torqueAccumulator);

    g_PhysicsManager.rigidbodies[g_PhysicsManager.numRigidbodies++] = newRb;
    printf("Rijit cisim %d oluşturuldu (Kütle: %.2f, Statik: %s)\n", newRb->id, newRb->mass, newRb->isStatic ? "Evet" : "Hayır");
    return newRb;
}

void fePhysicsManagerRemoveRigidbody(FeRigidbody* rb) {
    if (!g_PhysicsManager.initialized) {
        fprintf(stderr, "Fizik yöneticisi başlatılmadı.\n");
        return;
    }
    if (!rb) return;

    // Rijit cismi listeden bul ve kaldır
    for (unsigned int i = 0; i < g_PhysicsManager.numRigidbodies; ++i) {
        if (g_PhysicsManager.rigidbodies[i]->id == rb->id) {
            free(g_PhysicsManager.rigidbodies[i]); // Belleği serbest bırak

            // Diziyi kaydır
            for (unsigned int j = i; j < g_PhysicsManager.numRigidbodies - 1; ++j) {
                g_PhysicsManager.rigidbodies[j] = g_PhysicsManager.rigidbodies[j + 1];
            }
            g_PhysicsManager.numRigidbodies--;
            printf("Rijit cisim %d kaldırıldı.\n", rb->id);
            return;
        }
    }
    fprintf(stderr, "Uyarı: Rijit cisim %d bulunamadı veya zaten kaldırılmış.\n", rb->id);
}

void fePhysicsManagerUpdate(float deltaTime) {
    if (!g_PhysicsManager.initialized) {
        fprintf(stderr, "Hata: Fizik yöneticisi başlatılmadı. Güncelleme yapılamıyor.\n");
        return;
    }
    if (deltaTime <= 0.0f) return;

    // 1. Rijit cisimleri entegre et (konum ve hızları güncelle)
    feIntegrateRigidbodies(deltaTime);

    // 2. Geniş Faz Çarpışma Tespiti (Broad-Phase) - Olası çarpışma çiftlerini bul
    feBroadPhase();

    // 3. Dar Faz Çarpışma Tespiti ve Çözümlemesi (Narrow-Phase & Resolution) - Çarpışma bilgilerini oluştur ve çöz
    // Penetrasyonu giderme (Position correction)
    // İteratif bir yaklaşım, daha kararlı sonuçlar verir.
    // Projelerde yaygın olarak birkaç iterasyon kullanılır (örn: 5-10)
    const int positionCorrectionIterations = 5;
    for (int iter = 0; iter < positionCorrectionIterations; ++iter) {
        // Sadece penetrasyonu düzeltmek için (çarpışma impulsu uygulanmaz)
        for (unsigned int i = 0; i < g_PhysicsManager.numCollisionInfos; ++i) {
             FeCollisionInfo* collision = &g_PhysicsManager.collisionInfos[i];
             // Penetrasyon giderme
             const float slop = 0.01f; // Küçük bir boşluk bırakarak titremeyi engeller
             const float percent = 0.4f; // Düzeltmenin yüzde kaçı uygulansın
             vec3 correction;
             glm_vec3_scale(collision->normal, fmaxf(collision->penetrationDepth - slop, 0.0f) * percent, correction);

             if (!collision->rbA->isStatic) {
                 glm_vec3_muladds(correction, -0.5f, collision->rbA->position);
             }
             if (!collision->rbB->isStatic) {
                 glm_vec3_muladds(correction, 0.5f, collision->rbB->position);
             }
        }
    }


    // Impuls tabanlı çarpışma çözümlemesi (hızları güncelleme)
    const int velocityIterations = 10; // Genellikle daha fazla iterasyon gerektirir
    for (int iter = 0; iter < velocityIterations; ++iter) {
         for (unsigned int i = 0; i < g_PhysicsManager.numCollisionInfos; ++i) {
            feResolveCollision(&g_PhysicsManager.collisionInfos[i]);
         }
    }


    // TODO: Bir çarpışma dinleyicisi veya geri çağırma (callback) mekanizması ekleyebilirsiniz
    // fePhysicsManagerOnCollision(collisionInfo);
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

    // Basit geometriler için eylemsizlik tensörü hesaplaması
    mat4 inertiaTensor; // Eylemsizlik tensörü

    switch (rb->collider.type) {
        case FE_COLLIDER_SPHERE: {
            float I = (2.0f / 5.0f) * rb->mass * (rb->collider.shape.sphere.radius * rb->collider.shape.sphere.radius);
            glm_mat4_identity(inertiaTensor);
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
            inertiaTensor[0][1] = inertiaTensor[0][2] = 0.0f;
            inertiaTensor[1][0] = inertiaTensor[1][2] = 0.0f;
            inertiaTensor[2][0] = inertiaTensor[2][1] = 0.0f;
            inertiaTensor[3][3] = 1.0f; // Homojen koordinatlar için
            break;
        }
        case FE_COLLIDER_CAPSULE: {
            // Kapsül için eylemsizlik tensörü daha karmaşıktır.
            // Silindir ve iki yarım kürenin birleşimi olarak düşünülebilir.
            // Basitlik adına, şimdilik bir küre veya kutu gibi davranmasını sağlayabiliriz.
            // Veya daha gelişmiş bir hesaplama yapabilirsiniz.
            // Şimdilik basitleştirilmiş bir yaklaşımla sadece küresel bir varsayım yapalım
            float effectiveRadius = rb->collider.shape.capsule.radius;
            float effectiveHeight = rb->collider.shape.capsule.height;
            // Bu sadece bir placeholder, gerçek hesaplama daha karmaşıktır.
            float I = (2.0f / 5.0f) * rb->mass * (effectiveRadius * effectiveRadius);
            glm_mat4_identity(inertiaTensor);
            inertiaTensor[0][0] = I;
            inertiaTensor[1][1] = I;
            inertiaTensor[2][2] = I;
            break;
        }
        default:
            fprintf(stderr, "Uyarı: Bilinmeyen çarpıştırıcı tipi için eylemsizlik tensörü hesaplanamıyor.\n");
            glm_mat4_identity(inertiaTensor); // Varsayılan olarak birim matris
            break;
    }

    // Tersini al
    glm_mat4_inv(inertiaTensor, rb->inverseInertiaTensor);
}

void feRigidbodyGetTransformMatrix(const FeRigidbody* rb, mat4 outMatrix) {
    mat4 rotationMatrix;
    glm_quat_mat4(rb->rotation, rotationMatrix); // Quaternion'dan dönüşüm matrisine
    glm_mat4_copy(rotationMatrix, outMatrix);
    glm_mat4_translate(outMatrix, rb->position); // Konumu matrise uygula
}


// --- Çarpışma Tespit Fonksiyonları ---
// Bunlar, Broad-Phase'de çağrılan dar faz fonksiyonlarıdır.

bool feDetectCollision_SphereSphere(const FeRigidbody* rbA, const FeRigidbody* rbB, FeCollisionInfo* outInfo) {
    // Dünya koordinatlarındaki merkez noktaları
    vec3 centerA, centerB;
    glm_vec3_add(rbA->position, rbA->collider.localOffset, centerA);
    glm_vec3_add(rbB->position, rbB->collider.localOffset, centerB);

    vec3 distanceVec;
    glm_vec3_sub(centerB, centerA, distanceVec);
    float distanceSq = glm_vec3_norm2(distanceVec); // Mesafenin karesi

    float radiusA = rbA->collider.shape.sphere.radius;
    float radiusB = rbB->collider.shape.sphere.radius;
    float radiiSum = radiusA + radiusB;
    float radiiSumSq = radiiSum * radiiSum;

    if (distanceSq >= radiiSumSq) {
        return false; // Çarpışma yok
    }

    // Çarpışma var, bilgileri doldur
    float distance = sqrtf(distanceSq);
    outInfo->rbA = (FeRigidbody*)rbA;
    outInfo->rbB = (FeRigidbody*)rbB;
    glm_vec3_normalize_to(distanceVec, outInfo->normal); // Normal A'dan B'ye
    outInfo->penetrationDepth = radiiSum - distance;

    // Temas noktalarını hesapla (basit haliyle)
    vec3 tempNormalScaled;
    glm_vec3_scale(outInfo->normal, radiusA, tempNormalScaled);
    glm_vec3_add(centerA, tempNormalScaled, outInfo->contactPointA); // rbA yüzeyindeki tahmini temas noktası

    glm_vec3_scale(outInfo->normal, -radiusB, tempNormalScaled);
    glm_vec3_add(centerB, tempNormalScaled, outInfo->contactPointB); // rbB yüzeyindeki tahmini temas noktası

    return true;
}

bool feDetectCollision_BoxBox(const FeRigidbody* rbA, const FeRigidbody* rbB, FeCollisionInfo* outInfo) {
    // OBB (Oriented Bounding Box) çarpışması AABB'den daha karmaşıktır.
    // Separating Axis Theorem (SAT) genellikle OBB çarpışmalarında kullanılır.
    // Bu fonksiyon sadece bir placeholder. Gerçek bir SAT implementasyonu gerektirir.
    // Şimdilik hep çarpışma yokmuş gibi davranalım.
    (void)rbA; // Kullanılmayan parametre uyarısını önlemek için
    (void)rbB; // Kullanılmayan parametre uyarısını önlemek için
    (void)outInfo; // Kullanılmayan parametre uyarısını önlemek için

    // TODO: Gerçek OBB-OBB SAT implementasyonu buraya gelecek.
    // Bu, oldukça karmaşık bir algoritmadır ve bu örnek kapsamına sığmayabilir.
    // Basit AABB (Axis-Aligned Bounding Box) çakışması daha kolaydır, ancak cisim döndüğünde yanlış sonuçlar verir.
    // Şimdilik sadece false döndürelim.
    return false;
}

bool feDetectCollision_SphereBox(const FeRigidbody* rbA, const FeRigidbody* rbB, FeCollisionInfo* outInfo) {
    // Sphere-Box (Küre-Kutu) çarpışması için temel bir algoritma:
    // 1. Kürenin merkezini kutunun yerel (local) uzayına dönüştür.
    // 2. Kürenin yerel uzaydaki en yakın noktasını kutunun sınırları içinde sıkıştır (clamp).
    // 3. Sıkıştırılmış nokta ile küre merkezi arasındaki mesafeyi kontrol et.

    // Not: rbA küre, rbB kutu olmalıdır (Broad-phase'de bu sağlanıyor)
    vec3 sphereCenterWorld;
    glm_vec3_add(rbA->position, rbA->collider.localOffset, sphereCenterWorld);

    // Kutunun dünya uzayındaki dönüş matrisini al
    mat4 boxRotationMat;
    glm_quat_mat4(rbB->rotation, boxRotationMat);

    // Kürenin merkezini kutunun yerel uzayına dönüştür
    vec3 sphereCenterLocal;
    vec3 tempVec;
    glm_vec3_sub(sphereCenterWorld, rbB->position, tempVec); // Kutunun orijinine göre küre merkezi
    mat4 boxRotationInvMat;
    glm_mat4_transpose(boxRotationMat, boxRotationInvMat); // Dönüş matrisinin tersi transpozesidir
    glm_mat4_mulv3(boxRotationInvMat, tempVec, 0.0f, sphereCenterLocal); // Sadece dönüşümü uygula


    vec3 closestPointInBox;
    vec3 boxHalfExtents = rbB->collider.shape.box.halfExtents;

    // Kürenin merkezini kutunun sınırları içine sıkıştır
    closestPointInBox[0] = fmaxf(-boxHalfExtents[0], fminf(sphereCenterLocal[0], boxHalfExtents[0]));
    closestPointInBox[1] = fmaxf(-boxHalfExtents[1], fminf(sphereCenterLocal[1], boxHalfExtents[1]));
    closestPointInBox[2] = fmaxf(-boxHalfExtents[2], fminf(sphereCenterLocal[2], boxHalfExtents[2]));

    // Sıkıştırılmış nokta ile küre merkezi arasındaki vektör
    vec3 distVecLocal;
    glm_vec3_sub(sphereCenterLocal, closestPointInBox, distVecLocal);
    float distanceSq = glm_vec3_norm2(distVecLocal);
    float sphereRadius = rbA->collider.shape.sphere.radius;
    float sphereRadiusSq = sphereRadius * sphereRadius;

    if (distanceSq >= sphereRadiusSq) {
        return false; // Çarpışma yok
    }

    // Çarpışma var, bilgileri doldur
    outInfo->rbA = (FeRigidbody*)rbA; // Sphere
    outInfo->rbB = (FeRigidbody*)rbB; // Box

    float distance = sqrtf(distanceSq);
    outInfo->penetrationDepth = sphereRadius - distance;

    // Çarpışma normalini ve temas noktalarını dünya uzayına dönüştür
    if (distance == 0.0f) { // Merkez kutunun tam içindeyse, varsayılan bir normal ata
        // En az penetrasyon eksenini bulmaya çalışabiliriz
        glm_vec3_copy((vec3){0.0f, 1.0f, 0.0f}, outInfo->normal); // Varsayılan olarak yukarı
    } else {
        vec3 normalLocal;
        glm_vec3_normalize_to(distVecLocal, normalLocal);
        glm_mat4_mulv3(boxRotationMat, normalLocal, 0.0f, outInfo->normal); // Kutunun dönüşünü uygula
    }

    // Temas noktalarını hesapla
    vec3 tempScaledNormal;
    glm_vec3_scale(outInfo->normal, sphereRadius - outInfo->penetrationDepth, tempScaledNormal); // Penetrasyon dışındaki kısım
    glm_vec3_add(sphereCenterWorld, tempScaledNormal, outInfo->contactPointA); // Küre yüzeyindeki temas noktası (tahmini)

    // Kutunun yüzeyindeki temas noktası (en yakın nokta)
    glm_mat4_mulv3(boxRotationMat, closestPointInBox, 0.0f, tempVec); // Local'den World'e
    glm_vec3_add(rbB->position, tempVec, outInfo->contactPointB);

    return true;
}


// --- Çarpışma Çözümleme Fonksiyonu ---

void feResolveCollision(FeCollisionInfo* collision) {
    FeRigidbody* rbA = collision->rbA;
    FeRigidbody* rbB = collision->rbB;

    // Statik nesneler çözülmez
    if (rbA->isStatic && rbB->isStatic) return;

    // Göreceli kapanma hızı (relative closing velocity) hesapla
    vec3 relativeVelocity;
    glm_vec3_sub(rbA->linearVelocity, rbB->linearVelocity, relativeVelocity);

    // Normal boyunca göreceli hız bileşeni
    float velAlongNormal = glm_vec3_dot(relativeVelocity, collision->normal);

    // Eğer nesneler birbirinden uzaklaşıyorsa, çözümlemeye gerek yok
    if (velAlongNormal > 0) {
        return;
    }

    // Esneklik katsayısı (restitution)
    // Materyallerin ortalaması alınabilir
    float e = fminf(rbA->collider.material.restitution, rbB->collider.material.restitution);

    // İmpuls hesaplaması
    float j = -(1.0f + e) * velAlongNormal;
    j /= (rbA->inverseMass + rbB->inverseMass); // Basit doğrusal impuls

    vec3 impulse;
    glm_vec3_scale(collision->normal, j, impulse);

    // Kuvvetleri uygula
    if (!rbA->isStatic) {
        glm_vec3_muladds(impulse, rbA->inverseMass, rbA->linearVelocity);
    }
    if (!rbB->isStatic) {
        glm_vec3_muladds(impulse, -rbB->inverseMass, rbB->linearVelocity);
    }

    // TODO: Açısal impuls ve sürtünme hesaplaması (daha karmaşık)
    // Bu, temas noktalarına olan uzaklık ve kütle ataleti ile ilgilidir.
    // collision->contactPointA ve collision->contactPointB kullanılarak torklar hesaplanabilir.
    // Tangential (teğetsel) hız bileşenleri ve sürtünme katsayıları gereklidir.
}
