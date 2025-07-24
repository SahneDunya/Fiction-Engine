#ifndef FE_MAP_H
#define FE_MAP_H

#include "core/utils/fe_types.h"            // Temel tipler (size_t, bool, uint64_t vb.)
#include "core/memory/fe_memory_manager.h"  // Bellek yönetimi (FE_MALLOC, FE_FREE)
#include "core/ds/fe_ds_types.h"           // fe_hash_func, fe_compare_func, fe_data_free_func, fe_data_copy_func için
#include "core/containers/fe_list.h"       // Kovalar için bağlantılı liste

// --- Hash Haritası Giriş (Entry) Yapısı ---
/**
 * @brief Hash haritasında depolanan tek bir anahtar-değer çiftini temsil eder.
 */
typedef struct fe_map_entry {
    void* key;         // Anahtarın kendisi (bellek tahsis edilmiş olabilir)
    size_t        key_size;    // Anahtarın bayt cinsinden boyutu
    void* value;       // Değerin kendisi (bellek tahsis edilmiş olabilir)
    size_t        value_size;  // Değerin bayt cinsinden boyutu
    uint64_t      key_hash;    // Anahtarın önceden hesaplanmış hash değeri
} fe_map_entry_t;

// --- Hash Haritası Yapısı ---
/**
 * @brief Fiction Engine için jenerik bir hash haritası (hash map) veri yapısı.
 * Çakışmaları zincirleme (chaining) ile yönetir.
 */
typedef struct fe_map {
    fe_list_t* buckets;            // Kovaları temsil eden bağlantılı listeler dizisi
    size_t                  capacity;           // Toplam kova sayısı (hash tablosunun boyutu)
    size_t                  size;               // Haritadaki eleman sayısı (anahtar-değer çiftleri)
    float                   load_factor_threshold; // Yeniden boyutlandırma için yük faktörü eşiği
    
    fe_hash_func        hash_key_cb;        // Anahtarları hashlemek için geri arama fonksiyonu
    fe_compare_func     compare_key_cb;     // Anahtarları karşılaştırmak için geri arama fonksiyonu
    fe_data_free_func   free_key_cb;        // Anahtar verisini serbest bırakmak için geri arama (isteğe bağlı)
    fe_data_free_func   free_value_cb;      // Değer verisini serbest bırakmak için geri arama (isteğe bağlı)
    fe_data_copy_func   copy_key_cb;        // Anahtar verisini kopyalamak için geri arama (isteğe bağlı)
    fe_data_copy_func   copy_value_cb;      // Değer verisini kopyalamak için geri arama (isteğe bağlı)
} fe_map_t;

// --- Hash Haritası Fonksiyonları ---

/**
 * @brief Yeni bir boş hash haritası başlatır.
 *
 * @param map Başlatılacak hash haritası işaretçisi.
 * @param initial_capacity Başlangıç kova sayısı. İdeal olarak bir asal sayı.
 * @param hash_key_callback Anahtarları hashlemek için kullanılacak geri arama fonksiyonu. ZORUNLU.
 * @param compare_key_callback Anahtarları karşılaştırmak için kullanılacak geri arama fonksiyonu. ZORUNLU.
 * @param free_key_callback İsteğe bağlı: Her anahtar için bir serbest bırakma geri arama fonksiyonu.
 * NULL ise anahtar serbest bırakılmaz (sadece fe_map_entry_t.key serbest bırakılır).
 * @param free_value_callback İsteğe bağlı: Her değer için bir serbest bırakma geri arama fonksiyonu.
 * NULL ise değer serbest bırakılmaz (sadece fe_map_entry_t.value serbest bırakılır).
 * @param copy_key_callback İsteğe bağlı: Anahtarı kopyalamak için geri arama fonksiyonu. NULL ise memcpy kullanılır.
 * @param copy_value_callback İsteğe bağlı: Değeri kopyalamak için geri arama fonksiyonu. NULL ise memcpy kullanılır.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_map_init(fe_map_t* map,
                 size_t initial_capacity,
                 fe_hash_func hash_key_callback,
                 fe_compare_func compare_key_callback,
                 fe_data_free_func free_key_callback,
                 fe_data_free_func free_value_callback,
                 fe_data_copy_func copy_key_callback,
                 fe_data_copy_func copy_value_callback);

/**
 * @brief Bir hash haritasını kapatır ve tüm elemanlarını ve varsa verilerini serbest bırakır.
 *
 * @param map Kapatılacak hash haritası işaretçisi.
 */
void fe_map_shutdown(fe_map_t* map);

/**
 * @brief Haritaya yeni bir anahtar-değer çifti ekler veya mevcut bir anahtarın değerini günceller.
 *
 * @param map İşlem yapılacak hash haritası.
 * @param key Eklenecek/güncellenecek anahtar.
 * @param key_size Anahtarın bayt cinsinden boyutu.
 * @param value Eklenecek/güncellenecek değer.
 * @param value_size Değerin bayt cinsinden boyutu.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_map_set(fe_map_t* map, const void* key, size_t key_size, const void* value, size_t value_size);

/**
 * @brief Belirli bir anahtara karşılık gelen değeri alır.
 *
 * @param map İşlem yapılacak hash haritası.
 * @param key Aranacak anahtar.
 * @param key_size Anahtarın bayt cinsinden boyutu.
 * @param out_value Bulunan değerin kopyasının depolanacağı adres. Eğer NULL ise sadece varlık kontrolü yapılır.
 * @param out_value_size Bulunan değerin bayt cinsinden boyutunu döndürür (NULL değilse).
 * @return bool Anahtar bulunursa true, aksi takdirde false.
 */
bool fe_map_get(const fe_map_t* map, const void* key, size_t key_size, void** out_value, size_t* out_value_size);

/**
 * @brief Haritadan belirli bir anahtar-değer çiftini kaldırır.
 *
 * @param map İşlem yapılacak hash haritası.
 * @param key Kaldırılacak anahtar.
 * @param key_size Anahtarın bayt cinsinden boyutu.
 * @return bool Başarılı ise true (anahtar bulundu ve kaldırıldı), aksi takdirde false.
 */
bool fe_map_remove(fe_map_t* map, const void* key, size_t key_size);

/**
 * @brief Haritanın belirli bir anahtar içerip içermediğini kontrol eder.
 *
 * @param map İşlem yapılacak hash haritası.
 * @param key Aranacak anahtar.
 * @param key_size Anahtarın bayt cinsinden boyutu.
 * @return bool Anahtar bulunursa true, aksi takdirde false.
 */
bool fe_map_contains(const fe_map_t* map, const void* key, size_t key_size);

/**
 * @brief Haritadaki eleman sayısını döndürür.
 *
 * @param map İşlem yapılacak hash haritası.
 * @return size_t Haritadaki eleman sayısı.
 */
size_t fe_map_size(const fe_map_t* map);

/**
 * @brief Haritanın boş olup olmadığını kontrol eder.
 *
 * @param map İşlem yapılacak hash haritası.
 * @return bool Harita boşsa true, aksi takdirde false.
 */
bool fe_map_is_empty(const fe_map_t* map);

/**
 * @brief Haritayı boşaltır, tüm elemanlarını ve varsa verilerini serbest bırakır,
 * ancak haritanın yapısını korur (kullanıma hazır bırakır).
 *
 * @param map Boşaltılacak hash haritası işaretçisi.
 */
void fe_map_clear(fe_map_t* map);

/**
 * @brief Harita üzerinde iterasyon yapmak için her eleman için verilen callback'i çağırır.
 * İterasyon sırası garanti edilmez.
 *
 * @param map İterasyon yapılacak harita.
 * @param callback Her eleman için çağrılacak fonksiyon (key, key_size, value, value_size alır).
 */
typedef void (*fe_map_iter_callback)(const void* key, size_t key_size, void* value, size_t value_size);
void fe_map_for_each(const fe_map_t* map, fe_map_iter_callback callback);

#endif // FE_MAP_H
