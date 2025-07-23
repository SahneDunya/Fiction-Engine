#include "include/memory/fe_memory_manager.h"
#include "include/utils/fe_logger.h" // Loglama için
#include <stdlib.h> // malloc, free için
#include <stdio.h>  // printf, snprintf için
#include <string.h> // memset için

// --- Dahili Yapılar ve Değişkenler ---

// Her tahsis edilmiş blok için meta veri
typedef struct fe_memory_block_header {
    size_t size;                       // Bloğun boyutu
    unsigned int ref_count;            // Referans sayısı
    fe_memory_allocation_type_t type;  // Tahsis tipi
    const char* file;                  // Tahsis edilen dosya
    int line;                          // Tahsis edilen satır
    struct fe_memory_block_header* next; // Bağlı liste için sonraki
    struct fe_memory_block_header* prev; // Bağlı liste için önceki
    // Ek hata ayıklama verileri eklenebilir (örneğin, magic number, çağrı yığını)
} fe_memory_block_header_t;

// Bellek yöneticisinin durumu
static struct {
    size_t total_allocated_bytes;
    size_t allocated_bytes_by_type[FE_MEM_TYPE_COUNT];
    unsigned int total_allocations;
    unsigned int active_allocations;
    fe_memory_block_header_t* head; // Tüm aktif tahsislerin bağlı listesi
    bool is_initialized;
} fe_memory_manager_state;

// Header'ın kendisinin boyutu (işaretçi aritmetiği için)
#define FE_MEMORY_HEADER_SIZE sizeof(fe_memory_block_header_t)

// İşaretçiden header'a dönüşüm
#define GET_HEADER_FROM_PTR(ptr) \
    ((fe_memory_block_header_t*)((char*)ptr - FE_MEMORY_HEADER_SIZE))

// Header'dan kullanıcı işaretçisine dönüşüm
#define GET_PTR_FROM_HEADER(header) \
    ((void*)((char*)header + FE_MEMORY_HEADER_SIZE))

// --- Dahili Yardımcı Fonksiyonlar ---

static void fe_memory_add_to_list(fe_memory_block_header_t* header) {
    if (fe_memory_manager_state.head == NULL) {
        fe_memory_manager_state.head = header;
        header->next = NULL;
        header->prev = NULL;
    } else {
        header->next = fe_memory_manager_state.head;
        fe_memory_manager_state.head->prev = header;
        header->prev = NULL;
        fe_memory_manager_state.head = header;
    }
    fe_memory_manager_state.active_allocations++;
}

static void fe_memory_remove_from_list(fe_memory_block_header_t* header) {
    if (header->prev) {
        header->prev->next = header->next;
    } else {
        fe_memory_manager_state.head = header->next;
    }
    if (header->next) {
        header->next->prev = header->prev;
    }
    fe_memory_manager_state.active_allocations--;
}

// --- Bellek Yöneticisi Fonksiyonları Uygulaması ---

fe_memory_error_t fe_memory_manager_init() {
    if (fe_memory_manager_state.is_initialized) {
        FE_LOG_WARN("Memory manager already initialized.");
        return FE_MEMORY_SUCCESS;
    }

    memset(&fe_memory_manager_state, 0, sizeof(fe_memory_manager_state));
    fe_memory_manager_state.is_initialized = true;
    FE_LOG_INFO("Memory manager initialized.");
    return FE_MEMORY_SUCCESS;
}

fe_memory_error_t fe_memory_manager_shutdown() {
    if (!fe_memory_manager_state.is_initialized) {
        FE_LOG_WARN("Memory manager not initialized, cannot shut down.");
        return FE_MEMORY_SUCCESS;
    }

    // Kapatmadan önce sızan bellek olup olmadığını kontrol et
    if (fe_memory_manager_state.total_allocated_bytes > 0) {
        FE_LOG_ERROR("Memory manager shutting down with %zu bytes (%u allocations) still active!",
                     fe_memory_manager_state.total_allocated_bytes,
                     fe_memory_manager_state.active_allocations);
        fe_memory_print_usage(); // Detaylı kullanımı yazdır
        // Tüm kalan tahsisleri serbest bırak (sızıntıları önlemek için)
        fe_memory_block_header_t* current = fe_memory_manager_state.head;
        while (current != NULL) {
            fe_memory_block_header_t* next = current->next;
            FE_LOG_WARN("Forcibly freeing leaked memory: %zu bytes (Type: %d) at %s:%d",
                        current->size, current->type, current->file, current->line);
            free(current); // Direkt free kullan
            current = next;
        }
    }

    memset(&fe_memory_manager_state, 0, sizeof(fe_memory_manager_state));
    fe_memory_manager_state.is_initialized = false;
    FE_LOG_INFO("Memory manager shut down.");
    return FE_MEMORY_SUCCESS;
}

void* fe_malloc_owned(size_t size, fe_memory_allocation_type_t type, const char* file, int line) {
    if (!fe_memory_manager_state.is_initialized) {
        FE_LOG_ERROR("Memory manager not initialized! Allocating raw memory for %zu bytes at %s:%d", size, file, line);
        return malloc(size); // Acil durum için raw malloc
    }

    // Header ve kullanıcı verisi için toplam boyut
    size_t total_size = size + FE_MEMORY_HEADER_SIZE;
    fe_memory_block_header_t* header = (fe_memory_block_header_t*)malloc(total_size);

    if (header == NULL) {
        FE_LOG_ERROR("Failed to allocate %zu bytes (Type: %d) at %s:%d", size, type, file, line);
        return NULL;
    }

    // Header'ı doldur
    header->size = size;
    header->ref_count = 1; // Başlangıçta 1 referans (sahip)
    header->type = type;
    header->file = file;
    header->line = line;
    // Bağlı listeye ekle
    fe_memory_add_to_list(header);

    // İstatistikleri güncelle
    fe_memory_manager_state.total_allocated_bytes += size;
    if (type < FE_MEM_TYPE_COUNT) {
        fe_memory_manager_state.allocated_bytes_by_type[type] += size;
    }
    fe_memory_manager_state.total_allocations++;

    FE_LOG_DEBUG("Allocated %zu bytes (Type: %d, RC: %u) at %s:%d",
                 size, type, header->ref_count, file, line);

    // Kullanıcıya veri alanının işaretçisini döndür
    return GET_PTR_FROM_HEADER(header);
}

fe_memory_error_t fe_memory_acquire(void* ptr, const char* file, int line) {
    if (!fe_memory_manager_state.is_initialized) {
        FE_LOG_ERROR("Memory manager not initialized! Cannot acquire memory for %p at %s:%d", ptr, file, line);
        return FE_MEMORY_ALLOC_FAILED; // Uygun bir hata kodu
    }
    if (ptr == NULL) {
        FE_LOG_WARN("Attempted to acquire NULL pointer at %s:%d", file, line);
        return FE_MEMORY_INVALID_POINTER;
    }

    fe_memory_block_header_t* header = GET_HEADER_FROM_PTR(ptr);

    // TODO: header'ın gerçekten geçerli bir tahsis bloğuna ait olup olmadığını kontrol et
    // Bu, bağlı listede arama yaparak veya daha sofistike bir yöntemle yapılabilir.
    // Şimdilik sadece basit bir kontrol yapıyoruz.
    if (header->size == 0 || header->ref_count == 0) { // Çok basit bir kontrol
        FE_LOG_ERROR("Invalid memory block header for acquire operation at %s:%d. Pointer: %p", file, line, ptr);
        return FE_MEMORY_INVALID_POINTER;
    }

    header->ref_count++;
    FE_LOG_DEBUG("Acquired memory %p (Type: %d, RC: %u) at %s:%d",
                 ptr, header->type, header->ref_count, file, line);
    return FE_MEMORY_SUCCESS;
}

fe_memory_error_t fe_free_owned(void* ptr, const char* file, int line) {
    if (!fe_memory_manager_state.is_initialized) {
        FE_LOG_ERROR("Memory manager not initialized! Cannot free memory for %p at %s:%d", ptr, file, line);
        free(ptr); // Acil durum için raw free
        return FE_MEMORY_ALLOC_FAILED; // Uygun bir hata kodu
    }
    if (ptr == NULL) {
        FE_LOG_WARN("Attempted to free NULL pointer at %s:%d", file, line);
        return FE_MEMORY_INVALID_POINTER;
    }

    fe_memory_block_header_t* header = GET_HEADER_FROM_PTR(ptr);

    // TODO: header'ın gerçekten geçerli bir tahsis bloğuna ait olup olmadığını kontrol et
    // Bu, bağlı listede arama yaparak veya daha sofistike bir yöntemle yapılabilir.
    // Şimdilik sadece basit bir kontrol yapıyoruz.
    if (header->size == 0 || header->ref_count == 0) { // Çok basit bir kontrol
        FE_LOG_ERROR("Invalid memory block header for free operation at %s:%d. Pointer: %p", file, line, ptr);
        return FE_MEMORY_INVALID_POINTER;
    }

    header->ref_count--;
    FE_LOG_DEBUG("Released memory %p (Type: %d, RC: %u) at %s:%d",
                 ptr, header->type, header->ref_count, file, line);

    if (header->ref_count == 0) {
        // Referans sayısı sıfıra düştü, belleği serbest bırak
        fe_memory_remove_from_list(header);

        fe_memory_manager_state.total_allocated_bytes -= header->size;
        if (header->type < FE_MEM_TYPE_COUNT) {
            fe_memory_manager_state.allocated_bytes_by_type[header->type] -= header->size;
        }
        free(header); // Belleği fiziksel olarak serbest bırak
        FE_LOG_DEBUG("Memory block %p (size: %zu) freed by %s:%d", ptr, header->size, file, line);
        return FE_MEMORY_SUCCESS;
    } else if (header->ref_count < 0) { // Should not happen with unsigned int
         FE_LOG_ERROR("Ref count went below zero for %p at %s:%d", ptr, file, line);
         return FE_MEMORY_DOUBLE_FREE; // Veya başka bir uygun hata
    } else {
        // Bellek hala başka sahipler tarafından kullanılıyor, serbest bırakma
        return FE_MEMORY_SUCCESS;
    }
}

void fe_memory_print_usage() {
    if (!fe_memory_manager_state.is_initialized) {
        FE_LOG_WARN("Memory manager not initialized, cannot print usage.");
        return;
    }

    FE_LOG_INFO("--- Fiction Engine Memory Usage ---");
    FE_LOG_INFO("Total Allocated Bytes: %zu", fe_memory_manager_state.total_allocated_bytes);
    FE_LOG_INFO("Total Allocations Made: %u", fe_memory_manager_state.total_allocations);
    FE_LOG_INFO("Active Allocations: %u", fe_memory_manager_state.active_allocations);

    const char* type_names[] = {
        "General", "Graphics", "Physics", "Audio", "AI", "Editor", "Temp"
    };

    for (int i = 0; i < FE_MEM_TYPE_COUNT; ++i) {
        if (fe_memory_manager_state.allocated_bytes_by_type[i] > 0) {
            FE_LOG_INFO("  - %s: %zu bytes", type_names[i], fe_memory_manager_state.allocated_bytes_by_type[i]);
        }
    }

    // Aktif tahsislerin detaylarını listele (hata ayıklama için faydalı)
    if (fe_memory_manager_state.active_allocations > 0) {
        FE_LOG_INFO("--- Active Memory Blocks ---");
        fe_memory_block_header_t* current = fe_memory_manager_state.head;
        while (current != NULL) {
            FE_LOG_INFO("  %p | Size: %zu | RC: %u | Type: %s | %s:%d",
                        GET_PTR_FROM_HEADER(current), current->size, current->ref_count,
                        type_names[current->type], current->file, current->line);
            current = current->next;
        }
    }
    FE_LOG_INFO("------------------------------");
}

bool fe_memory_is_valid_ptr(const void* ptr) {
    if (!fe_memory_manager_state.is_initialized || ptr == NULL) {
        return false;
    }
    // Basit bir kontrol: işaretçinin tahsis edilen bir bloğun parçası olup olmadığını kontrol etmek
    // Tam bir doğrulama için, tüm bağlı listedeki header'ları kontrol etmek gerekir.
    // Bu işlem büyük tahsis listelerinde yavaş olabilir.
    // Daha performanslı bir çözüm için, bir hash tablosu veya özel bir veri yapısı kullanılabilir.
    fe_memory_block_header_t* current = fe_memory_manager_state.head;
    while (current != NULL) {
        void* user_ptr = GET_PTR_FROM_HEADER(current);
        if (user_ptr == ptr) {
            return true;
        }
        current = current->next;
    }
    return false;
}
