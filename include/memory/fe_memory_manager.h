#ifndef FE_MEMORY_MANAGER_H
#define FE_MEMORY_MANAGER_H

#include <stddef.h> // size_t için
#include <stdbool.h> // bool için

// Bellek hata kodları
typedef enum fe_memory_error {
    FE_MEMORY_SUCCESS = 0,
    FE_MEMORY_ALLOC_FAILED,
    FE_MEMORY_FREE_FAILED,
    FE_MEMORY_INVALID_POINTER,
    FE_MEMORY_DOUBLE_FREE,
    FE_MEMORY_NOT_OWNED
} fe_memory_error_t;

// Bellek tahsis tipi (örneğin, motor iç bileşenleri, oyun verisi, geçici)
typedef enum fe_memory_allocation_type {
    FE_MEM_TYPE_GENERAL = 0,
    FE_MEM_TYPE_GRAPHICS,
    FE_MEM_TYPE_PHYSICS,
    FE_MEM_TYPE_AUDIO,
    FE_MEM_TYPE_AI,
    FE_MEM_TYPE_EDITOR,
    FE_MEM_TYPE_TEMP,
    FE_MEM_TYPE_COUNT // Toplam tahsis tipi sayısı
} fe_memory_allocation_type_t;

// --- Temel Bellek Yöneticisi Fonksiyonları ---

/**
 * @brief Bellek yöneticisini başlatır.
 * Bu fonksiyon motorun başlangıcında çağrılmalıdır.
 * @return fe_memory_error_t Hata kodu (FE_MEMORY_SUCCESS eğer başarılıysa).
 */
fe_memory_error_t fe_memory_manager_init();

/**
 * @brief Bellek yöneticisini kapatır ve tüm tahsis edilmiş belleği serbest bırakır.
 * Bu fonksiyon motorun kapanışında çağrılmalıdır.
 * @return fe_memory_error_t Hata kodu.
 */
fe_memory_error_t fe_memory_manager_shutdown();

/**
 * @brief Belirtilen boyutta bellek tahsis eder ve referans sayısını 1 olarak ayarlar.
 * Bu bellek bloğunun "sahibi" bu tahsisi yapan fonksiyondur.
 * @param size Tahsis edilecek bayt boyutu.
 * @param type Tahsisin tipi (örneğin, FE_MEM_TYPE_GRAPHICS).
 * @param file Belleğin tahsis edildiği kaynak dosya (hata ayıklama için).
 * @param line Belleğin tahsis edildiği kaynak satır (hata ayıklama için).
 * @return void* Tahsis edilmiş bellek bloğunun işaretçisi, başarısız olursa NULL.
 */
void* fe_malloc_owned(size_t size, fe_memory_allocation_type_t type, const char* file, int line);

/**
 * @brief Belirtilen bir bellek bloğunun sahipliğini devralır (referans sayısını artırır).
 * Bu fonksiyon bir "ödünç alma" (borrowing) veya "paylaşma" (sharing) durumu oluşturur.
 * @param ptr Sahipliği artırılacak bellek bloğunun işaretçisi.
 * @param file İşlemi yapan kaynak dosya.
 * @param line İşlemi yapan kaynak satır.
 * @return fe_memory_error_t Hata kodu.
 */
fe_memory_error_t fe_memory_acquire(void* ptr, const char* file, int line);

/**
 * @brief Bir bellek bloğunun referans sayısını azaltır.
 * Referans sayısı sıfıra ulaştığında bellek otomatik olarak serbest bırakılır.
 * Bu fonksiyon bir "sahipliği bırakma" (releasing ownership) durumu oluşturur.
 * @param ptr Serbest bırakılacak veya referans sayısı azaltılacak bellek bloğunun işaretçisi.
 * @param file İşlemi yapan kaynak dosya.
 * @param line İşlemi yapan kaynak satır.
 * @return fe_memory_error_t Hata kodu.
 */
fe_memory_error_t fe_free_owned(void* ptr, const char* file, int line);

// --- Hata ayıklama ve raporlama fonksiyonları ---

/**
 * @brief Mevcut bellek kullanımını konsola veya log dosyasına yazdırır.
 */
void fe_memory_print_usage();

/**
 * @brief Belirtilen bellek adresinin geçerli olup olmadığını kontrol eder.
 * @param ptr Kontrol edilecek işaretçi.
 * @return bool İşaretçi geçerliyse true, aksi takdirde false.
 */
bool fe_memory_is_valid_ptr(const void* ptr);

#endif // FE_MEMORY_MANAGER_H
