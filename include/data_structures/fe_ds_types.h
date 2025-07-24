#ifndef FE_DS_TYPES_H
#define FE_DS_TYPES_H

#include "core/utils/fe_types.h" // Motorun genel temel tipleri (uint32_t, bool, size_t vb.)

// --- Genel Veri Yapısı Tanımları ve Typedef'ler ---

/**
 * @brief Genel bir karşılaştırma fonksiyonu typedef'i.
 * Bu fonksiyon, iki öğeyi karşılaştırır ve şunları döndürür:
 * - Negatif bir değer: Eğer a < b ise
 * - Pozitif bir değer: Eğer a > b ise
 * - Sıfır: Eğer a == b ise
 * @param a Karşılaştırılacak ilk öğe işaretçisi.
 * @param b Karşılaştırılacak ikinci öğe işaretçisi.
 * @return int Karşılaştırma sonucu.
 */
typedef int (*fe_compare_func)(const void* a, const void* b);

/**
 * @brief Genel bir hash fonksiyonu typedef'i.
 * Belirtilen veri için bir hash değeri üretir.
 * @param data Hashlenecek veri işaretçisi.
 * @param size Hashlenecek verinin boyutu.
 * @return uint64_t Oluşturulan hash değeri.
 */
typedef uint64_t (*fe_hash_func)(const void* data, size_t size);

/**
 * @brief Genel bir bellek serbest bırakma (free) fonksiyonu typedef'i.
 * Belirli bir veri öğesi için tahsis edilmiş belleği serbest bırakır.
 * Konteynerler bir öğeyi kaldırdığında veya kapatıldığında bu kullanılabilir.
 * @param data Serbest bırakılacak veri işaretçisi.
 */
typedef void (*fe_data_free_func)(void* data);

/**
 * @brief Genel bir veri kopyalama (copy) fonksiyonu typedef'i.
 * Bir öğenin derin kopyasını oluşturur. Yüksek performanslı konteynerler için
 * kopyalama genellikle gereklidir, özellikle dinamik bellek içeren öğeler için.
 * @param dest Kopyalanacak verinin hedef işaretçisi.
 * @param src Kopyalanacak verinin kaynak işaretçisi.
 * @param size Kopyalanacak verinin boyutu.
 * @return bool Kopyalama başarılı ise true, aksi takdirde false.
 */
typedef bool (*fe_data_copy_func)(void* dest, const void* src, size_t size);

// --- Temel Hash Fonksiyonları (genel kullanıma açık) ---
// Bunlar fe_hash_func typedef'ine uyan fonksiyon implementasyonları olacaktır.

/**
 * @brief Basit bir FNV-1a 64-bit hash fonksiyonu implementasyonu.
 * Metin tabanlı anahtarlar veya ham bayt dizileri için kullanılabilir.
 * @param data Hashlenecek veri işaretçisi.
 * @param size Hashlenecek verinin boyutu.
 * @return uint64_t Oluşturulan hash değeri.
 */
uint64_t fe_ds_hash_fnv1a(const void* data, size_t size);

/**
 * @brief Bir string için hash değeri üreten yardımcı fonksiyon.
 * Aslında fe_ds_hash_fnv1a'yı strlen ile çağırır.
 * @param str Hashlenecek string.
 * @return uint64_t Oluşturulan hash değeri.
 */
uint64_t fe_ds_hash_string(const char* str);


// --- Temel Karşılaştırma Fonksiyonları (genel kullanıma açık) ---
// Bunlar fe_compare_func typedef'ine uyan fonksiyon implementasyonları olacaktır.

/**
 * @brief İki integer'ı karşılaştıran fonksiyon.
 * @param a Karşılaştırılacak ilk int işaretçisi.
 * @param b Karşılaştırılacak ikinci int işaretçisi.
 * @return int Karşılaştırma sonucu.
 */
int fe_ds_compare_int(const void* a, const void* b);

/**
 * @brief İki float'ı karşılaştıran fonksiyon.
 * Hassasiyet sorunları nedeniyle float karşılaştırmaları dikkatli olmalıdır.
 * @param a Karşılaştırılacak ilk float işaretçisi.
 * @param b Karşılaştırılacak ikinci float işaretçisi.
 * @return int Karşılaştırma sonucu.
 */
int fe_ds_compare_float(const void* a, const void* b);

/**
 * @brief İki string'i sözlük sırasına göre karşılaştıran fonksiyon.
 * @param a Karşılaştırılacak ilk string işaretçisi.
 * @param b Karşılaştırılacak ikinci string işaretçisi.
 * @return int Karşılaştırma sonucu (strcmp'in döndürdüğü gibi).
 */
int fe_ds_compare_string(const void* a, const void* b);

// --- Konteynerlere Özel Typedef'ler ---
// Bu typedef'ler, genellikle diğer veri yapılarının kendilerine ait
// .h dosyalarında tanımlanır ancak burada genel olarak bir fikir vermek için listelenmiştir.

// Dinamik Dizi (Dynamic Array)
// #define FE_DYNAMIC_ARRAY_DEFINE(type) struct { type* data; size_t size; size_t capacity; }
// typedef struct fe_dynamic_array_meta { /* meta bilgiler */ } fe_dynamic_array_meta_t;

// Bağlantılı Liste (Linked List)
// typedef struct fe_list_node { void* data; struct fe_list_node* next; } fe_list_node_t;
// typedef struct fe_linked_list { fe_list_node_t* head; size_t size; } fe_linked_list_t;

// Hash Haritası (Hash Map)
// typedef struct fe_hash_map_entry { uint64_t key_hash; void* key_ptr; void* value_ptr; } fe_hash_map_entry_t;
// typedef struct fe_hash_map { /* ... */ } fe_hash_map_t;

// İkili Arama Ağacı (Binary Search Tree - BST)
// typedef struct fe_bst_node { void* key; void* value; struct fe_bst_node* left; struct fe_bst_node* right; } fe_bst_node_t;
// typedef struct fe_bst_tree { fe_bst_node_t* root; size_t size; fe_compare_func compare_func; } fe_bst_tree_t;


#endif // FE_DS_TYPES_H
