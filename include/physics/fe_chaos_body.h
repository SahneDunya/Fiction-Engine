#ifndef FE_CHAOS_BODY_H
#define FE_CHAOS_BODY_H

#include "core/utils/fe_types.h" // fe_vec3, fe_quat, fe_mat4 için
#include "core/containers/fe_string.h" // fe_string için

// Bu başlık fe_chaos_world.h'deki enum ve yapıları içerir.
// Böylece bu dosyayı kullananların ayrıca fe_chaos_world.h'yi dahil etmesine gerek kalmaz.
#include "physics/fe_chaos_world.h"

// --- Fonksiyon Deklarasyonları ---

/**
 * @brief Yeni bir rijit cisim tanımlayıcısı (fe_rigid_body_desc_t) oluşturur ve varsayılan değerlerle başlatır.
 *
 * @return fe_rigid_body_desc_t Yeni bir tanımlayıcı.
 */
fe_rigid_body_desc_t fe_chaos_body_create_desc();

/**
 * @brief Bir rijit cisim tanımlayıcısının içerdiği dinamik belleği (özellikle isim ve şekiller) temizler.
 *
 * @param desc Temizlenecek tanımlayıcı.
 */
void fe_chaos_body_destroy_desc(fe_rigid_body_desc_t* desc);

/**
 * @brief Rijit cisim tanımlayıcısına yeni bir çarpışma şekli ekler.
 * Bu fonksiyon, desc'in internal collision_shapes dizisini dinamik olarak büyütür.
 *
 * @param desc Eklenecek şeklin ait olduğu rijit cisim tanımlayıcısı.
 * @param shape Eklenecek çarpışma şekli. Bu şekil kendi içinde kopyalanacaktır.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_body_add_collision_shape(fe_rigid_body_desc_t* desc, const fe_collision_shape_t* shape);

/**
 * @brief Yeni bir rijit cisim oluşturur ve onu fizik dünyasına (Chaos World) ekler.
 *
 * @param desc Rijit cismin özellikleri. desc içindeki veriler kopyalanır.
 * @return fe_chaos_rigid_body_t* Oluşturulan rijit cisim tutacına işaretçi, başarısız olursa NULL.
 */
fe_chaos_rigid_body_t* fe_chaos_body_create(const fe_rigid_body_desc_t* desc);

/**
 * @brief Bir rijit cismi fizik dünyasından kaldırır ve tüm kaynaklarını serbest bırakır.
 *
 * @param body Yok edilecek rijit cisim tutacı.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_body_destroy(fe_chaos_rigid_body_t* body);

/**
 * @brief Bir rijit cismin ID'sini döndürür.
 *
 * @param body Rijit cisim tutacı.
 * @return const char* Rijit cismin benzersiz ID'si.
 */
const char* fe_chaos_body_get_id(const fe_chaos_rigid_body_t* body);

/**
 * @brief Bir rijit cismin türünü (statik, dinamik, kinematik) döndürür.
 *
 * @param body Rijit cisim tutacı.
 * @return fe_physics_body_type_t Rijit cismin türü.
 */
fe_physics_body_type_t fe_chaos_body_get_type(const fe_chaos_rigid_body_t* body);

/**
 * @brief Bir rijit cismin kütlesini döndürür.
 *
 * @param body Rijit cisim tutacı.
 * @return float Rijit cismin kütlesi.
 */
float fe_chaos_body_get_mass(const fe_chaos_rigid_body_t* body);

/**
 * @brief Bir rijit cismin kütlesini ayarlar. Sadece dinamik cisimler için geçerlidir.
 *
 * @param body Rijit cisim tutacı.
 * @param mass Ayarlanacak yeni kütle.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_body_set_mass(fe_chaos_rigid_body_t* body, float mass);

/**
 * @brief Bir rijit cismin sürtünme katsayılarını ayarlar.
 *
 * @param body Rijit cisim tutacı.
 * @param static_friction Statik sürtünme katsayısı.
 * @param dynamic_friction Dinamik sürtünme katsayısı.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_body_set_friction(fe_chaos_rigid_body_t* body, float static_friction, float dynamic_friction);

/**
 * @brief Bir rijit cismin geri sekme katsayısını ayarlar.
 *
 * @param body Rijit cisim tutacı.
 * @param restitution Geri sekme katsayısı.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_body_set_restitution(fe_chaos_rigid_body_t* body, float restitution);


// Konum ve rotasyon fonksiyonları fe_chaos_world'den doğrudan çağrılabilir.
// Ancak, fe_chaos_body üzerinden de erişim kolaylığı sağlamak için burada tekrarlıyoruz.
// Bunlar aslında fe_chaos_world'deki fonksiyonlara birer wrapper olacaktır.

/**
 * @brief Bir rijit cismin mevcut konumunu alır.
 *
 * @param body Rijit cisim tutacı.
 * @param out_position Konumun yazılacağı fe_vec3 işaretçisi.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_body_get_position(const fe_chaos_rigid_body_t* body, fe_vec3* out_position);

/**
 * @brief Bir rijit cismin mevcut rotasyonunu alır.
 *
 * @param body Rijit cisim tutacı.
 * @param out_rotation Rotasyonun yazılacağı fe_quat işaretçisi.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_body_get_rotation(const fe_chaos_rigid_body_t* body, fe_quat* out_rotation);

/**
 * @brief Bir rijit cismin mevcut dönüşüm matrisini (konum ve rotasyon) alır.
 *
 * @param body Rijit cisim tutacı.
 * @param out_transform Dönüşümün yazılacağı fe_mat4 işaretçisi.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_body_get_transform(const fe_chaos_rigid_body_t* body, fe_mat4* out_transform);

/**
 * @brief Bir rijit cismin konumunu ayarlar (yalnızca kinematik veya dinamik cisimler için).
 *
 * @param body Rijit cisim tutacı.
 * @param new_position Yeni konum.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_body_set_position(fe_chaos_rigid_body_t* body, fe_vec3 new_position);

/**
 * @brief Bir rijit cismin rotasyonunu ayarlar (yalnızca kinematik veya dinamik cisimler için).
 *
 * @param body Rijit cisim tutacı.
 * @param new_rotation Yeni rotasyon.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_body_set_rotation(fe_chaos_rigid_body_t* body, fe_quat new_rotation);

/**
 * @brief Bir rijit cisme doğrusal bir kuvvet uygular.
 *
 * @param body Rijit cisim tutacı.
 * @param force Uygulanacak kuvvet vektörü.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_body_apply_force(fe_chaos_rigid_body_t* body, fe_vec3 force);

/**
 * @brief Bir rijit cisme tek seferlik bir doğrusal itme uygular.
 *
 * @param body Rijit cisim tutacı.
 * @param impulse Uygulanacak itme vektörü.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_body_apply_impulse(fe_chaos_rigid_body_t* body, fe_vec3 impulse);

/**
 * @brief Bir rijit cisme açısal bir tork uygular.
 *
 * @param body Rijit cisim tutacı.
 * @param torque Uygulanacak tork vektörü.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_body_apply_torque(fe_chaos_rigid_body_t* body, fe_vec3 torque);

/**
 * @brief Bir rijit cisme tek seferlik bir açısal itme uygular.
 *
 * @param body Rijit cisim tutacı.
 * @param angular_impulse Uygulanacak açısal itme vektörü.
 * @return fe_chaos_error_t Başarı durumunu döner.
 */
fe_chaos_error_t fe_chaos_body_apply_angular_impulse(fe_chaos_rigid_body_t* body, fe_vec3 angular_impulse);

#endif // FE_CHAOS_BODY_H
