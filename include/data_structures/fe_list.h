#ifndef FE_LIST_H
#define FE_LIST_H

#include "core/utils/fe_types.h"        // Temel tipler (size_t, bool, uint64_t vb.)
#include "core/memory/fe_memory_manager.h" // Bellek yönetimi (FE_MALLOC, FE_FREE)
#include "core/ds/fe_ds_types.h"       // fe_data_free_func, fe_compare_func, fe_data_copy_func için

// --- Bağlantılı Liste Düğümü Yapısı ---
/**
 * @brief Tek yönlü bağlantılı liste düğümü.
 * Her düğüm, bir veri parçasını ve listedeki bir sonraki düğüme bir işaretçiyi içerir.
 */
typedef struct fe_list_node {
    void* data;       // Düğümün depoladığı veri
    size_t               data_size;  // Verinin bayt cinsinden boyutu
    struct fe_list_node* next;       // Listedeki bir sonraki düğüme işaretçi
} fe_list_node_t;

// --- Bağlantılı Liste Yapısı ---
/**
 * @brief Tek yönlü bağlantılı liste veri yapısı.
 * Listenin başını ve geçerli eleman sayısını yönetir.
 */
typedef struct fe_list {
    fe_list_node_t* head;          // Listenin ilk düğümüne işaretçi
    size_t              size;          // Listedeki eleman sayısı
    fe_data_free_func   data_free_cb;  // Düğümler silindiğinde veriyi serbest bırakmak için geri arama fonksiyonu (callback)
    fe_compare_func     data_compare_cb; // Verileri karşılaştırmak için geri arama fonksiyonu (opsiyonel)
    fe_data_copy_func   data_copy_cb;  // Verileri kopyalamak için geri arama fonksiyonu (opsiyonel)
} fe_list_t;

// --- Liste Fonksiyonları ---

/**
 * @brief Yeni bir boş bağlantılı liste başlatır.
 *
 * @param list Başlatılacak liste işaretçisi.
 * @param data_free_callback İsteğe bağlı: Her düğümün verisi için bir serbest bırakma
 * geri arama fonksiyonu. NULL ise veri serbest bırakılmaz.
 * @param data_compare_callback İsteğe bağlı: Arama ve sıralama gibi işlemler için
 * veri karşılaştırma geri arama fonksiyonu.
 * @param data_copy_callback İsteğe bağlı: Veriyi düğümlere kopyalamak için geri arama
 * fonksiyonu. NULL ise memcpy kullanılır.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_list_init(fe_list_t* list,
                  fe_data_free_func data_free_callback,
                  fe_compare_func data_compare_callback,
                  fe_data_copy_func data_copy_callback);

/**
 * @brief Bir bağlantılı listeyi kapatır ve tüm düğümlerini ve varsa verilerini serbest bırakır.
 *
 * @param list Kapatılacak liste işaretçisi.
 */
void fe_list_shutdown(fe_list_t* list);

/**
 * @brief Listenin başına yeni bir öğe ekler. O(1) karmaşıklık.
 *
 * @param list Öğenin ekleneceği liste.
 * @param data Eklenecek öğenin verisi.
 * @param data_size Verinin bayt cinsinden boyutu.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_list_prepend(fe_list_t* list, const void* data, size_t data_size);

/**
 * @brief Listenin sonuna yeni bir öğe ekler. O(n) karmaşıklık.
 * Çift yönlü listelerde O(1) olabilir.
 *
 * @param list Öğenin ekleneceği liste.
 * @param data Eklenecek öğenin verisi.
 * @param data_size Verinin bayt cinsinden boyutu.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_list_append(fe_list_t* list, const void* data, size_t data_size);

/**
 * @brief Belirli bir indekse yeni bir öğe ekler.
 * İndeks 0 ise fe_list_prepend ile aynıdır.
 * İndeks listenin boyutu ise fe_list_append ile aynıdır.
 *
 * @param list Öğenin ekleneceği liste.
 * @param index Öğenin ekleneceği konum (0 tabanlı).
 * @param data Eklenecek öğenin verisi.
 * @param data_size Verinin bayt cinsinden boyutu.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_list_insert_at(fe_list_t* list, size_t index, const void* data, size_t data_size);

/**
 * @brief Listenin başındaki öğeyi kaldırır. O(1) karmaşıklık.
 *
 * @param list Öğenin kaldırılacağı liste.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_list_remove_head(fe_list_t* list);

/**
 * @brief Listenin sonundaki öğeyi kaldırır. O(n) karmaşıklık.
 *
 * @param list Öğenin kaldırılacağı liste.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_list_remove_tail(fe_list_t* list);

/**
 * @brief Belirli bir indeksteki öğeyi kaldırır.
 *
 * @param list Öğenin kaldırılacağı liste.
 * @param index Kaldırılacak öğenin konumu (0 tabanlı).
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_list_remove_at(fe_list_t* list, size_t index);

/**
 * @brief Listedeki ilk eşleşen öğeyi (veri içeriğine göre) kaldırır.
 * data_compare_cb fonksiyonu liste başlatılırken verilmiş olmalıdır.
 *
 * @param list Öğenin kaldırılacağı liste.
 * @param data Kaldırılacak öğenin verisi (karşılaştırma için kullanılır).
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_list_remove(fe_list_t* list, const void* data);

/**
 * @brief Listenin başındaki öğenin verisine bir işaretçi döndürür.
 *
 * @param list İşlem yapılacak liste.
 * @return void* Baş düğümün verisine işaretçi, liste boşsa NULL.
 */
void* fe_list_peek_head(const fe_list_t* list);

/**
 * @brief Listenin sonundaki öğenin verisine bir işaretçi döndürür.
 *
 * @param list İşlem yapılacak liste.
 * @return void* Son düğümün verisine işaretçi, liste boşsa NULL.
 */
void* fe_list_peek_tail(const fe_list_t* list);

/**
 * @brief Belirli bir indeksteki öğenin verisine bir işaretçi döndürür.
 *
 * @param list İşlem yapılacak liste.
 * @param index Getirilecek öğenin konumu (0 tabanlı).
 * @return void* İlgili düğümün verisine işaretçi, indeks geçersizse NULL.
 */
void* fe_list_get_at(const fe_list_t* list, size_t index);

/**
 * @brief Listedeki bir öğenin varlığını kontrol eder.
 * data_compare_cb fonksiyonu liste başlatılırken verilmiş olmalıdır.
 *
 * @param list İşlem yapılacak liste.
 * @param data Aranacak öğenin verisi.
 * @return bool Öğeyi bulunursa true, aksi takdirde false.
 */
bool fe_list_contains(const fe_list_t* list, const void* data);

/**
 * @brief Listenin boş olup olmadığını kontrol eder.
 *
 * @param list İşlem yapılacak liste.
 * @return bool Liste boşsa true, aksi takdirde false.
 */
bool fe_list_is_empty(const fe_list_t* list);

/**
 * @brief Listedeki eleman sayısını döndürür.
 *
 * @param list İşlem yapılacak liste.
 * @return size_t Listedeki eleman sayısı.
 */
size_t fe_list_size(const fe_list_t* list);

/**
 * @brief Listeyi boşaltır, tüm düğümleri ve varsa verilerini serbest bırakır,
 * ancak listeyi kapatmaz (kullanıma hazır bırakır).
 *
 * @param list Boşaltılacak liste işaretçisi.
 */
void fe_list_clear(fe_list_t* list);

/**
 * @brief Liste üzerinde iterasyon yapmak için başlangıç düğümünü döndürür.
 *
 * @param list İterasyon yapılacak liste.
 * @return fe_list_node_t* Listenin ilk düğümü, liste boşsa NULL.
 */
fe_list_node_t* fe_list_get_iterator(const fe_list_t* list);

/**
 * @brief Listenin tüm elemanlarını verilen bir fonksiyona uygular.
 *
 * @param list İşlem yapılacak liste.
 * @param callback Her eleman için çağrılacak fonksiyon (parametre olarak void* data alır).
 */
typedef void (*fe_list_iter_callback)(void* data);
void fe_list_for_each(const fe_list_t* list, fe_list_iter_callback callback);

#endif // FE_LIST_H
