#include "core/ds/fe_ds_types.h"
#include "core/utils/fe_logger.h" // Loglama için
#include <string.h> // strlen, strcmp için

// --- FNV-1a 64-bit Hash Fonksiyonu Implementasyonu ---
// Kaynak: http://www.isthe.com/chongo/tech/comp/fnv/
#define FNV_PRIME_64 ((uint64_t)1099511628211ULL)
#define FNV_OFFSET_BASIS_64 ((uint64_t)14695981039346656037ULL)

uint64_t fe_ds_hash_fnv1a(const void* data, size_t size) {
    if (!data || size == 0) {
        return 0; // Geçersiz giriş için 0 döndür
    }

    uint64_t hash = FNV_OFFSET_BASIS_64;
    const unsigned char* bytes = (const unsigned char*)data;

    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= FNV_PRIME_64;
    }
    return hash;
}

uint64_t fe_ds_hash_string(const char* str) {
    if (!str) {
        return 0;
    }
    return fe_ds_hash_fnv1a(str, strlen(str));
}

// --- Karşılaştırma Fonksiyonları Implementasyonları ---

int fe_ds_compare_int(const void* a, const void* b) {
    if (!a || !b) {
        // Hata durumu veya null pointer'lar için uygun bir davranış tanımlayın
        // Genellikle null < non-null olarak kabul edilir
        if (!a && !b) return 0;
        if (!a) return -1;
        if (!b) return 1;
    }
    const int val_a = *(const int*)a;
    const int val_b = *(const int*)b;
    if (val_a < val_b) return -1;
    if (val_a > val_b) return 1;
    return 0;
}

int fe_ds_compare_float(const void* a, const void* b) {
    if (!a || !b) {
        if (!a && !b) return 0;
        if (!a) return -1;
        if (!b) return 1;
    }
    const float val_a = *(const float*)a;
    const float val_b = *(const float*)b;
    // Float karşılaştırmaları için küçük bir epsilon kullanmak daha güvenlidir.
    // Ancak basitlik için doğrudan karşılaştırma yapılıyor.
    if (val_a < val_b) return -1;
    if (val_a > val_b) return 1;
    return 0;
}

int fe_ds_compare_string(const void* a, const void* b) {
    if (!a || !b) {
        if (!a && !b) return 0;
        if (!a) return -1;
        if (!b) return 1;
    }
    const char* str_a = (const char*)a;
    const char* str_b = (const char*)b;
    return strcmp(str_a, str_b);
}

// TODO: fe_data_copy_func ve fe_data_free_func implementasyonları
// Bunlar genellikle genel amaçlı olmaz, veri yapısının içindeki öğe tipine göre özelleştirilir.
// Örneğin, bir string array'i için fe_data_free_func, her string'i free etmelidir.
// Ancak void* olarak tanımlandıkları için, konteyner bu fonksiyon işaretçilerini
// "bilerek" doğru tipi cast edip işlemelidir.
