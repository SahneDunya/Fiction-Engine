#ifndef FE_BT_NODE_H
#define FE_BT_NODE_H

#include "core/utils/fe_types.h"
#include "core/memory/fe_memory_manager.h" // For FE_MALLOC, FE_FREE, etc.
#include "core/containers/fe_array.h"      // For managing child nodes

// --- Davranış Ağacı Düğüm Durumları ---
// Bir düğümün 'tick' işlemi sonucunda döndürebileceği durumlar.
typedef enum fe_bt_state {
    FE_BT_STATE_RUNNING,   // Düğüm hala çalışıyor ve tamamlanmadı. Bir sonraki tick'te devam edecek.
    FE_BT_STATE_SUCCESS,   // Düğüm başarıyla tamamlandı.
    FE_BT_STATE_FAILURE    // Düğüm başarısız oldu.
} fe_bt_state_t;

// --- Davranış Ağacı Düğüm Türleri ---
// Farklı düğüm davranışlarını temsil eder.
typedef enum fe_bt_node_type {
    FE_BT_NODE_TYPE_BASE,      // Soyut temel düğüm tipi (doğrudan kullanılmaz)
    FE_BT_NODE_TYPE_SEQUENCE,  // Çocukları sırayla yürütür, biri başarısız olursa durur.
    FE_BT_NODE_TYPE_SELECTOR,  // Çocukları sırayla yürütür, biri başarılı olursa durur.
    FE_BT_NODE_TYPE_LEAF,      // Eylem veya koşul gibi bitiş düğümleri. Çocukları olmaz.
    // Gelecekte eklenebilecek diğer tipler: Decorator, Parallel, Conditional vb.
} fe_bt_node_type_t;

// --- Fonksiyon İşaretçileri (Callback'ler) ---
// Her düğüm tipinin kendi davranışını tanımlamasını sağlar.

// Düğümün 'tick' edildiğinde çağrılacak fonksiyon.
// Parametre olarak düğümün kendisini ve isteğe bağlı 'context' verisini alır.
// fe_bt_state_t döndürür: RUNNING, SUCCESS, FAILURE.
typedef fe_bt_state_t (*PFN_fe_bt_node_tick)(struct fe_bt_node* node, void* context);

// Düğümün başlatıldığında (ilk tick'ten önce veya resetlendiğinde) çağrılacak fonksiyon.
// Kaynak tahsisi veya durum sıfırlama için kullanılabilir.
typedef void (*PFN_fe_bt_node_init_func)(struct fe_bt_node* node);

// Düğümün temizlenip yok edildiğinde çağrılacak fonksiyon.
// Tahsis edilen kaynakları serbest bırakmak için kullanılır.
typedef void (*PFN_fe_bt_node_destroy_func)(struct fe_bt_node* node);

// --- Davranış Ağacı Soyut Düğüm Yapısı ---
// Tüm davranış ağacı düğümleri bu temel yapıdan türetilir.
// Bu yapı doğrudan belleğe tahsis edilmez, genellikle diğer düğüm türlerinin içinde yer alır.
typedef struct fe_bt_node {
    fe_bt_node_type_t type;         // Düğümün türü
    char name[64];                  // Düğümün insan okunabilir adı (debugging için)
    fe_bt_state_t current_state;    // Düğümün mevcut yürütme durumu
    bool is_initialized;            // Düğümün başlatılıp başlatılmadığı

    // Çocuk düğümler (sadece kompozit düğümler için kullanılır)
    fe_array_t children;            // fe_bt_node* türünde işaretçileri tutan dinamik dizi

    void* user_data;                // Bu düğüme özel isteğe bağlı kullanıcı verisi (pointer)
    void* internal_state;           // Düğümün dahili durumunu tutan işaretçi (örn. sequence/selector'ın mevcut çocuk indeksi)

    PFN_fe_bt_node_tick tick_func;             // Düğümün 'tick' işlevini uygulayan fonksiyon işaretçisi
    PFN_fe_bt_node_init_func init_func;        // Düğümün başlatma işlevini uygulayan fonksiyon işaretçisi
    PFN_fe_bt_node_destroy_func destroy_func;  // Düğümün yok etme işlevini uygulayan fonksiyon işaretçisi
} fe_bt_node_t;

/**
 * @brief Bir davranış ağacı düğümünü başlatır. Bu fonksiyon genellikle diğer düğüm tipleri tarafından çağrılır.
 * @param node Başlatılacak düğümün işaretçisi.
 * @param type Düğümün türü.
 * @param name Düğümün adı (kopyalanır, null sonlandırıcılı olmalı).
 * @param tick_func Düğümün tick fonksiyon işaretçisi.
 * @param init_func Düğümün init fonksiyon işaretçisi (NULL olabilir).
 * @param destroy_func Düğümün destroy fonksiyon işaretçisi (NULL olabilir).
 * @param initial_child_capacity Eğer kompozit düğümse, başlangıç çocuk kapasitesi.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_bt_node_base_init(fe_bt_node_t* node, fe_bt_node_type_t type, const char* name,
                           PFN_fe_bt_node_tick tick_func, PFN_fe_bt_node_init_func init_func,
                           PFN_fe_bt_node_destroy_func destroy_func, size_t initial_child_capacity);

/**
 * @brief Bir davranış ağacı düğümünü ve ilişkili tüm kaynakları (çocuklar dahil) temizler.
 * @param node Temizlenecek düğümün işaretçisi.
 */
void fe_bt_node_destroy(fe_bt_node_t* node);

/**
 * @brief Düğümü 'tick' eder (günceller) ve durumunu döndürür.
 * @param node 'Tick' edilecek düğümün işaretçisi.
 * @param context AI sisteminin bağlamı veya durumu (örn. oyun dünyası, AI karakteri).
 * @return fe_bt_state_t Düğümün 'tick' sonrası durumu.
 */
fe_bt_state_t fe_bt_node_tick(fe_bt_node_t* node, void* context);

/**
 * @brief Bir kompozit düğüme çocuk düğüm ekler. Sadece kompozit düğümler için geçerlidir.
 * @param parent Üst düğümün işaretçisi.
 * @param child Eklenecek çocuk düğümün işaretçisi.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_bt_node_add_child(fe_bt_node_t* parent, fe_bt_node_t* child);

/**
 * @brief Bir düğümü başlangıç durumuna döndürür ve eğer varsa init_func'ını çağırır.
 * Özellikle bir düğüm tekrar yürütülecekse kullanılır.
 * @param node Sıfırlanacak düğümün işaretçisi.
 */
void fe_bt_node_reset(fe_bt_node_t* node);

/**
 * @brief Düğümün kullanıcı verisini ayarlar.
 * @param node Düğümün işaretçisi.
 * @param user_data Atanacak kullanıcı verisi işaretçisi.
 */
void fe_bt_node_set_user_data(fe_bt_node_t* node, void* user_data);

/**
 * @brief Düğümün kullanıcı verisini döndürür.
 * @param node Düğümün işaretçisi.
 * @return void* Kullanıcı verisi işaretçisi.
 */
void* fe_bt_node_get_user_data(const fe_bt_node_t* node);

// --- Davranış Ağacı Düğüm Tipleri için Yapıcı Fonksiyonlar ---
// Bu fonksiyonlar, belirli düğüm tiplerini kolayca oluşturmak için kullanılır.

/**
 * @brief Yeni bir Sequence (Sıralı) düğümü oluşturur.
 * @param name Düğümün adı.
 * @return fe_bt_node_t* Başlatılmış Sequence düğümünün işaretçisi, hata durumunda NULL.
 */
fe_bt_node_t* fe_bt_node_create_sequence(const char* name);

/**
 * @brief Yeni bir Selector (Seçici) düğümü oluşturur.
 * @param name Düğümün adı.
 * @return fe_bt_node_t* Başlatılmış Selector düğümünün işaretçisi, hata durumunda NULL.
 */
fe_bt_node_t* fe_bt_node_create_selector(const char* name);

/**
 * @brief Yeni bir Leaf (Yaprak) düğümü oluşturur. Bu düğümler genellikle eylemleri veya koşulları temsil eder.
 * @param name Düğümün adı.
 * @param tick_func Bu yaprak düğüm için özel 'tick' fonksiyonu. NULL olamaz.
 * @param init_func Bu yaprak düğüm için özel 'init' fonksiyonu (NULL olabilir).
 * @param destroy_func Bu yaprak düğüm için özel 'destroy' fonksiyonu (NULL olabilir).
 * @return fe_bt_node_t* Başlatılmış Leaf düğümünün işaretçisi, hata durumunda NULL.
 */
fe_bt_node_t* fe_bt_node_create_leaf(const char* name, PFN_fe_bt_node_tick tick_func,
                                     PFN_fe_bt_node_init_func init_func, PFN_fe_bt_node_destroy_func destroy_func);

#endif // FE_BT_NODE_H
