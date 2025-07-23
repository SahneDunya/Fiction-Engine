#ifndef FE_BT_ACTION_H
#define FE_BT_ACTION_H

#include "ai/fe_bt_node.h" // fe_bt_node_t ve fe_bt_state_t için
#include "core/utils/fe_types.h"
#include "core/memory/fe_memory_manager.h" // FE_MALLOC, FE_FREE için

// --- Yapılandırılmış Eylem Düğümü Verileri ---
// Her bir eylem tipinin kendi özel verilerini tutmak için kullanılabilecek yapılar tanımlanabilir.
// Bu, fe_bt_node_t'deki 'user_data' veya 'internal_state' ile ilişkilendirilebilir.

// Örnek: Bir "Hareket Et" eylemi için yapılandırma verisi
typedef struct fe_bt_action_move_data {
    float target_x;
    float target_y;
    float speed;
    float tolerance; // Hedefe ne kadar yakınsa başarılı sayılacak
} fe_bt_action_move_data_t;

// Örnek: Bir "Saldırı" eylemi için yapılandırma verisi
typedef struct fe_bt_action_attack_data {
    uint32_t ability_id;
    float attack_range;
    // Hedef varlık ID'si veya işaretçisi run-time context'ten alınır
} fe_bt_action_attack_data_t;

// --- Eylem Düğümü Fonksiyon İşaretçileri (İsteğe Bağlı) ---
// Belirli bir eylem türünü başlatmak veya bitirmek için özel callback'ler.
// Genellikle PFN_fe_bt_node_init_func ve PFN_fe_bt_node_destroy_func ile aynıdır.

// --- Eylem Düğümü Yapıcı Fonksiyonları ---
// Her bir özel eylem tipi için kolay oluşturma fonksiyonları.

/**
 * @brief Bir "Bekle" (Wait) eylemi düğümü oluşturur.
 * Belirli bir süre bekledikten sonra SUCCESS döndürür. Süre tamamlanana kadar RUNNING döndürür.
 *
 * @param name Düğümün adı (debugging için).
 * @param wait_duration_ms Beklenecek süre milisaniye cinsinden.
 * @return fe_bt_node_t* Yeni oluşturulan eylem düğümünün işaretçisi, hata durumunda NULL.
 */
fe_bt_node_t* fe_bt_action_create_wait(const char* name, uint32_t wait_duration_ms);

/**
 * @brief Bir "Hareket Et" (Move To) eylemi düğümü oluşturur.
 * Tanımlı bir hedefe hareket eder. Hedefe ulaştığında SUCCESS döndürür.
 * Hedefe ulaşana kadar RUNNING döndürür.
 *
 * @param name Düğümün adı.
 * @param data Hareket eylemine özgü yapılandırma verileri (target_x, target_y, speed, tolerance).
 * Bu veri FE_MALLOC ile kopyalanır, bu yüzden fonksiyona geçtikten sonra güvenle serbest bırakılabilir.
 * @return fe_bt_node_t* Yeni oluşturulan eylem düğümünün işaretçisi, hata durumunda NULL.
 */
fe_bt_node_t* fe_bt_action_create_move_to(const char* name, const fe_bt_action_move_data_t* data);

/**
 * @brief Bir "Saldır" (Attack) eylemi düğümü oluşturur.
 * Belirli bir yetenek ID'si ile hedefe saldırır. Saldırı tamamlandığında SUCCESS döndürür.
 * Saldırı animasyonu veya cooldown varsa RUNNING döndürebilir.
 *
 * @param name Düğümün adı.
 * @param data Saldırı eylemine özgü yapılandırma verileri (ability_id, attack_range).
 * @return fe_bt_node_t* Yeni oluşturulan eylem düğümünün işaretçisi, hata durumunda NULL.
 */
fe_bt_node_t* fe_bt_action_create_attack(const char* name, const fe_bt_action_attack_data_t* data);

// Diğer yaygın eylemler için benzer yapıcı fonksiyonlar eklenebilir:
// fe_bt_action_create_find_target()
// fe_bt_action_create_patrol()
// fe_bt_action_create_use_item()
// fe_bt_action_create_play_animation()
// ...

#endif // FE_BT_ACTION_H
