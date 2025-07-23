#ifndef FE_AI_MANAGER_H
#define FE_AI_MANAGER_H

#include "core/utils/fe_types.h"
#include "core/containers/fe_array.h" // fe_array_t için
#include "core/memory/fe_memory_manager.h" // Bellek yönetimi için

// Gerekli diğer AI ve yardımcı modüllerin başlık dosyaları
#include "ai/fe_perception_system.h"
#include "navigation/fe_pathfinder.h"
// Varsayımsal olarak AI ajanlarının temel verilerini içeren bir yapı
#include "ai/fe_ai_agent.h" // Daha önce tanımlanmış veya yeni tanımlanacak bir yapı

// --- AI Manager Ana Yapısı ---
typedef struct fe_ai_manager {
    fe_array_t ai_agents; // fe_ai_agent_t*: Kayıtlı tüm AI ajanları

    // AI alt sistemlerine referanslar
    fe_perception_system_t* perception_system; // Algılama sistemi işaretçisi
    fe_pathfinder_t* pathfinder_system;     // Yol bulma sistemi işaretçisi
    // struct fe_target_selection_system* target_selection_system; // Hedef seçimi gibi diğer sistemler

    uint32_t current_game_time_ms; // Oyunun mevcut zamanı
} fe_ai_manager_t;

// --- AI Manager Fonksiyonları ---

/**
 * @brief AI Yönetici sistemini başlatır.
 * @param manager Başlatılacak AI yöneticisi yapısının işaretçisi.
 * @param initial_agent_capacity Başlangıç AI ajanı kapasitesi.
 * @param perception_system_ptr Önceden başlatılmış algılama sisteminin işaretçisi.
 * @param pathfinder_system_ptr Önceden başlatılmış yol bulma sisteminin işaretçisi.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_ai_manager_init(fe_ai_manager_t* manager,
                        size_t initial_agent_capacity,
                        fe_perception_system_t* perception_system_ptr,
                        fe_pathfinder_t* pathfinder_system_ptr);

/**
 * @brief AI Yönetici sistemini ve ilişkili tüm kaynakları serbest bırakır.
 * Kayıtlı AI ajanlarını da serbest bırakır. Alt sistemlerin (perception, pathfinder)
 * kendileri tarafından serbest bırakılması gerektiğini varsayar.
 * @param manager Serbest bırakılacak AI yöneticisi yapısının işaretçisi.
 */
void fe_ai_manager_destroy(fe_ai_manager_t* manager);

/**
 * @brief Yeni bir AI ajanını sisteme kaydeder.
 * Ajanın içsel verilerini (örn. davranış ağacı, durum makinesi) başlatır.
 * @param manager AI yöneticisi işaretçisi.
 * @param entity_id Bu AI ajanının kontrol edeceği varlığın ID'si.
 * @param initial_pos Ajanın başlangıç pozisyonu.
 * @param initial_forward_dir Ajanın başlangıç ileri yönü.
 * @return fe_ai_agent_t* Eklenen AI ajanı yapısının işaretçisi veya hata durumunda NULL.
 */
fe_ai_agent_t* fe_ai_manager_register_agent(fe_ai_manager_t* manager,
                                            uint32_t entity_id,
                                            fe_vec3_t initial_pos,
                                            fe_vec3_t initial_forward_dir);

/**
 * @brief Kayıtlı bir AI ajanını sistemden kaldırır.
 * Ajanın içsel verilerini ve belleğini serbest bırakır.
 * @param manager AI yöneticisi işaretçisi.
 * @param agent_id Kaldırılacak ajanın varlık ID'si.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_ai_manager_unregister_agent(fe_ai_manager_t* manager, uint32_t agent_id);

/**
 * @brief Tüm AI ajanlarını günceller (her oyun tick'inde çağrılır).
 * Her ajanın davranış mantığını yürütür ve alt sistemlerle etkileşimlerini yönetir.
 * @param manager AI yöneticisi işaretçisi.
 * @param delta_time_ms Son güncellemeden bu yana geçen süre (milisaniye).
 * @param current_game_time_ms Oyunun mevcut genel zamanı (milisaniye).
 */
void fe_ai_manager_update(fe_ai_manager_t* manager, uint32_t delta_time_ms, uint32_t current_game_time_ms);

/**
 * @brief Belirli bir varlık ID'sine sahip AI ajanını bulur.
 * @param manager AI yöneticisi işaretçisi.
 * @param entity_id Aranacak ajanın varlık ID'si.
 * @return fe_ai_agent_t* Bulunan ajanın işaretçisi veya bulunamazsa NULL.
 */
fe_ai_agent_t* fe_ai_manager_get_agent(const fe_ai_manager_t* manager, uint32_t entity_id);

#endif // FE_AI_MANAGER_H
