#ifndef FE_ALLOCATOR_POOL_H
#define FE_ALLOCATOR_POOL_H

#include <stddef.h> // size_t için
#include <stdbool.h> // bool için
#include "core/memory/fe_memory_manager.h" // fe_memory_error_t için

// FE_MEMORY_ALIGNMENT: Bellek hizalama gereksinimi.
// Genellikle CPU ve GPU performans optimizasyonları için 16, 32 veya 64 bayt kullanılır.
#define FE_MEMORY_ALIGNMENT 16

/**
 * @brief Bellek havuzu yapısı.
 * Sabit boyutlu nesnelerin hızlı tahsisi ve serbest bırakılması için kullanılır.
 */
typedef struct fe_allocator_pool {
    size_t element_size;          // Havuzdaki her bir elemanın boyutu (bayt cinsinden)
    size_t capacity;              // Havuzun barındırabileceği toplam eleman sayısı
    size_t allocated_count;       // Şu anda tahsis edilmiş eleman sayısı

    void* memory_block;          // Havuzun tüm bellek bloğunun başlangıcı
    void* free_list_head;        // Serbest olan blokların bağlı listesinin başı (boşluk yönetimi)
} fe_allocator_pool_t;

/**
 * @brief Yeni bir bellek havuzu oluşturur ve başlatır.
 *
 * @param pool Havuz yapısının işaretçisi.
 * @param element_size Havuzdaki her bir elemanın bayt cinsinden boyutu.
 * @param capacity Havuzun barındırabileceği maksimum eleman sayısı.
 * @param type Bellek tahsis tipi (ana bellek yöneticisinden).
 * @return fe_memory_error_t Başarılı olursa FE_MEMORY_SUCCESS, aksi takdirde bir hata kodu.
 */
fe_memory_error_t fe_allocator_pool_create(fe_allocator_pool_t* pool,
                                           size_t element_size,
                                           size_t capacity,
                                           fe_memory_allocation_type_t type);

/**
 * @brief Bellek havuzunu yok eder ve tahsis edilmiş tüm belleği serbest bırakır.
 *
 * @param pool Havuz yapısının işaretçisi.
 * @return fe_memory_error_t Başarılı olursa FE_MEMORY_SUCCESS, aksi takdirde bir hata kodu.
 */
fe_memory_error_t fe_allocator_pool_destroy(fe_allocator_pool_t* pool);

/**
 * @brief Havuzdan yeni bir bellek bloğu tahsis eder.
 *
 * @param pool Havuz yapısının işaretçisi.
 * @return void* Tahsis edilmiş bellek bloğunun işaretçisi, havuz doluysa veya hata olursa NULL.
 */
void* fe_allocator_pool_allocate(fe_allocator_pool_t* pool);

/**
 * @brief Havuzdan tahsis edilmiş bir bellek bloğunu serbest bırakır.
 *
 * @param pool Havuz yapısının işaretçisi.
 * @param ptr Serbest bırakılacak bellek bloğunun işaretçisi.
 * @return fe_memory_error_t Başarılı olursa FE_MEMORY_SUCCESS, aksi takdirde bir hata kodu.
 */
fe_memory_error_t fe_allocator_pool_free(fe_allocator_pool_t* pool, void* ptr);

/**
 * @brief Havuzun mevcut kullanım istatistiklerini yazdırır.
 *
 * @param pool Havuz yapısının işaretçisi.
 */
void fe_allocator_pool_print_stats(const fe_allocator_pool_t* pool);

#endif // FE_ALLOCATOR_POOL_H
