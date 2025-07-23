#include "include/memory/fe_allocator_pool.h"
#include "include/memory/fe_memory_manager.h" // fe_malloc_owned, fe_free_owned için
#include "include/utils/fe_logger.h"          // Loglama için
#include <string.h>                       // memset için
#include <stdio.h>                        // printf için (kullanım istatistikleri)

// İleriye dönük bildirim (free list düğümleri için)
typedef struct free_node {
    struct free_node* next;
} free_node_t;

fe_memory_error_t fe_allocator_pool_create(fe_allocator_pool_t* pool,
                                           size_t element_size,
                                           size_t capacity,
                                           fe_memory_allocation_type_t type) {
    if (pool == NULL || element_size == 0 || capacity == 0) {
        FE_LOG_ERROR("Invalid parameters for fe_allocator_pool_create: pool=%p, element_size=%zu, capacity=%zu",
                     (void*)pool, element_size, capacity);
        return FE_MEMORY_INVALID_POINTER; // Veya daha uygun bir hata kodu
    }

    // Bellek hizalamasını sağlamak için eleman boyutunu yuvarla
    // Her bir elemanın boyutu, bir sonraki boş liste işaretçisinin (free_node*) sığabileceği kadar büyük olmalı
    // ve aynı zamanda FE_MEMORY_ALIGNMENT'a hizalanmış olmalı.
    size_t actual_element_size = element_size;
    if (actual_element_size < sizeof(free_node_t)) {
        actual_element_size = sizeof(free_node_t);
    }
    actual_element_size = (actual_element_size + (FE_MEMORY_ALIGNMENT - 1)) & ~(FE_MEMORY_ALIGNMENT - 1);

    size_t total_pool_size = actual_element_size * capacity;

    // Ana bellek yöneticisinden bellek tahsis et
    pool->memory_block = fe_malloc_owned(total_pool_size, type, __FILE__, __LINE__);
    if (pool->memory_block == NULL) {
        FE_LOG_ERROR("Failed to allocate %zu bytes for memory pool (element_size: %zu, capacity: %zu)",
                     total_pool_size, element_size, capacity);
        return FE_MEMORY_ALLOC_FAILED;
    }

    pool->element_size = actual_element_size;
    pool->capacity = capacity;
    pool->allocated_count = 0;
    pool->free_list_head = NULL;

    // Boş listeyi oluştur
    // Tüm bellek bloklarını işaretleyip bağlı listeye ekle
    for (size_t i = 0; i < capacity; ++i) {
        void* current_block = (char*)pool->memory_block + (i * pool->element_size);
        free_node_t* node = (free_node_t*)current_block;
        node->next = (free_node_t*)pool->free_list_head; // Önceki head'i next olarak ayarla
        pool->free_list_head = node;                      // Yeni head bu düğüm olsun
    }

    FE_LOG_INFO("Memory pool created: element_size=%zu (actual: %zu), capacity=%zu, total_size=%zu bytes",
                element_size, actual_element_size, capacity, total_pool_size);

    return FE_MEMORY_SUCCESS;
}

fe_memory_error_t fe_allocator_pool_destroy(fe_allocator_pool_t* pool) {
    if (pool == NULL) {
        FE_LOG_WARN("Attempted to destroy NULL memory pool.");
        return FE_MEMORY_INVALID_POINTER;
    }
    if (pool->memory_block == NULL) {
        FE_LOG_WARN("Memory pool %p already destroyed or not initialized.", (void*)pool);
        return FE_MEMORY_SUCCESS;
    }

    // Tüm tahsis edilmiş elemanların serbest bırakıldığından emin olunması tavsiye edilir.
    // Ancak havuzu yok ederken, alttaki ana bloğu serbest bırakmak yeterlidir.
    if (pool->allocated_count > 0) {
        FE_LOG_WARN("Memory pool destroyed with %zu elements still allocated!", pool->allocated_count);
    }

    fe_memory_error_t err = fe_free_owned(pool->memory_block, __FILE__, __LINE__);
    if (err != FE_MEMORY_SUCCESS) {
        FE_LOG_ERROR("Failed to free memory block for pool %p, error: %d", (void*)pool, err);
        return err;
    }

    // Yapıyı sıfırla
    memset(pool, 0, sizeof(fe_allocator_pool_t));
    FE_LOG_INFO("Memory pool destroyed.");

    return FE_MEMORY_SUCCESS;
}

void* fe_allocator_pool_allocate(fe_allocator_pool_t* pool) {
    if (pool == NULL || pool->memory_block == NULL) {
        FE_LOG_ERROR("Attempted to allocate from an uninitialized or destroyed pool.");
        return NULL;
    }

    if (pool->free_list_head == NULL) {
        // Havuz dolu
        FE_LOG_WARN("Memory pool is full (capacity: %zu, allocated: %zu). Cannot allocate.",
                    pool->capacity, pool->allocated_count);
        return NULL;
    }

    // Boş listeden bir blok al
    void* allocated_block = (void*)pool->free_list_head;
    pool->free_list_head = ((free_node_t*)pool->free_list_head)->next; // Listeyi ilerlet

    pool->allocated_count++;
    // Opsiyonel: Tahsis edilen belleği sıfırla (güvenlik için, performans maliyetli)
     memset(allocated_block, 0, pool->element_size);

    return allocated_block;
}

fe_memory_error_t fe_allocator_pool_free(fe_allocator_pool_t* pool, void* ptr) {
    if (pool == NULL || pool->memory_block == NULL) {
        FE_LOG_ERROR("Attempted to free to an uninitialized or destroyed pool.");
        return FE_MEMORY_INVALID_POINTER;
    }
    if (ptr == NULL) {
        FE_LOG_WARN("Attempted to free NULL pointer to memory pool.");
        return FE_MEMORY_INVALID_POINTER;
    }

    // İşaretçinin havuz içinde olup olmadığını kontrol et (güvenlik kontrolü)
    if (ptr < pool->memory_block || (char*)ptr >= ((char*)pool->memory_block + (pool->element_size * pool->capacity))) {
        FE_LOG_ERROR("Attempted to free memory %p not belonging to this pool!", ptr);
        return FE_MEMORY_NOT_OWNED; // Başka bir hata kodu da olabilir
    }

    // Doğru hizalamada olup olmadığını kontrol et (parçalanma kontrolü)
    if (((uintptr_t)ptr - (uintptr_t)pool->memory_block) % pool->element_size != 0) {
        FE_LOG_ERROR("Attempted to free misaligned memory %p to pool (element_size: %zu)!", ptr, pool->element_size);
        return FE_MEMORY_INVALID_POINTER;
    }

    // Serbest bırakılan bloğu boş listesinin başına ekle
    free_node_t* node = (free_node_t*)ptr;
    node->next = (free_node_t*)pool->free_list_head;
    pool->free_list_head = node;

    pool->allocated_count--;
    return FE_MEMORY_SUCCESS;
}

void fe_allocator_pool_print_stats(const fe_allocator_pool_t* pool) {
    if (pool == NULL) {
        FE_LOG_WARN("Cannot print stats for NULL memory pool.");
        return;
    }
    FE_LOG_INFO("--- Memory Pool Stats ---");
    FE_LOG_INFO("  Element Size (actual): %zu bytes", pool->element_size);
    FE_LOG_INFO("  Capacity: %zu elements", pool->capacity);
    FE_LOG_INFO("  Allocated: %zu elements", pool->allocated_count);
    FE_LOG_INFO("  Free: %zu elements", pool->capacity - pool->allocated_count);
    FE_LOG_INFO("  Total Pool Memory: %zu bytes", pool->element_size * pool->capacity);
    FE_LOG_INFO("-------------------------");
}
