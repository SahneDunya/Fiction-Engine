#ifndef FE_ASSET_MANAGER_H
#define FE_ASSET_MANAGER_H

#include "core/utils/fe_types.h"       // Temel tipler (uint32_t, bool vb.)
#include "core/containers/fe_hash_map.g.h" // Varlıkları saklamak için bir hash haritası
#include "core/memory/fe_memory_manager.h" // Bellek yönetimi için

// İleri bildirimler (gerçek varlık tipleri burada tanımlanmaz)
typedef struct fe_texture fe_texture_t;
typedef struct fe_model fe_model_t;
typedef struct fe_sound fe_sound_t;
// ... diğer varlık tipleri

// --- Genel Varlık Tipi Numaralandırması ---
typedef enum fe_asset_type {
    FE_ASSET_TYPE_UNKNOWN = 0,
    FE_ASSET_TYPE_TEXTURE,
    FE_ASSET_TYPE_MODEL,
    FE_ASSET_TYPE_SOUND,
    FE_ASSET_TYPE_SHADER,
    FE_ASSET_TYPE_LEVEL_DATA,
    // ... daha fazla varlık tipi eklenebilir
    FE_ASSET_TYPE_COUNT // Toplam varlık tipi sayısını tutar
} fe_asset_type_t;

// --- Temel Varlık Yapısı ---
// Tüm varlık tipleri bu yapıyı içermelidir (veya ilk üyesi olmalıdır)
// böylece genel bir fe_asset_t* olarak ele alınabilirler.
typedef struct fe_asset {
    uint64_t id;                 // Benzersiz varlık kimliği (path hash'i olabilir)
    char     path[256];          // Varlığın dosya yolu (mutlak veya göreceli)
    fe_asset_type_t type;        // Varlık tipi
    uint32_t ref_count;          // Referans sayacı: Kaç bileşenin bu varlığı kullandığını gösterir
    size_t   data_size;          // Varlık verisinin bellekteki boyutu (isteğe bağlı, izleme için)
    void* data_ptr;           // Varlık verisine işaretçi (örneğin, fe_texture_t*, fe_model_t*)
    // Ek meta veriler buraya eklenebilir (örneğin, yükleme bayrakları, son erişim zamanı)
} fe_asset_t;

// --- Varlık Yöneticisi Yapısı ---
// Varlıkların önbelleğini tutar.
typedef struct fe_asset_manager {
    FE_HASH_MAP_DEFINE(const char*, fe_asset_t*) assets_cache; // Varlık yolu -> Varlık pointer'ı
    uint64_t next_asset_id; // Yeni varlıklara benzersiz ID atamak için sayaç
} fe_asset_manager_t;

// --- Fonksiyon Deklarasyonları ---

/**
 * @brief Varlık yöneticisini başlatır.
 * @param manager fe_asset_manager_t yapısının işaretçisi.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_asset_manager_init(fe_asset_manager_t* manager);

/**
 * @brief Varlık yöneticisini kapatır ve tüm yüklü varlıkları bellekten atar.
 * @param manager fe_asset_manager_t yapısının işaretçisi.
 */
void fe_asset_manager_shutdown(fe_asset_manager_t* manager);

/**
 * @brief Belirtilen yoldan bir varlığı yükler veya önbellekten alır.
 * Referans sayacını artırır.
 * @param manager fe_asset_manager_t yapısının işaretçisi.
 * @param file_path Yüklenecek varlığın dosya yolu.
 * @param asset_type Varlığın tipi (FE_ASSET_TYPE_TEXTURE vb.).
 * @return fe_asset_t* Yüklenen/alınan varlığın işaretçisi, hata durumunda NULL.
 */
fe_asset_t* fe_asset_manager_load_asset(fe_asset_manager_t* manager, const char* file_path, fe_asset_type_t asset_type);

/**
 * @brief Bir varlığın referans sayacını azaltır.
 * Sayaç sıfıra ulaştığında varlığı bellekten atar.
 * @param manager fe_asset_manager_t yapısının işaretçisi.
 * @param asset Varlığın işaretçisi.
 */
void fe_asset_manager_release_asset(fe_asset_manager_t* manager, fe_asset_t* asset);

/**
 * @brief Belirtilen varlık tipindeki tüm varlıkları zorla bellekten atar.
 * Dikkat: Halihazırda kullanılan varlıklar için sorunlara yol açabilir.
 * Yalnızca belleği boşaltmak için veya kapanışta kullanılmalıdır.
 * @param manager fe_asset_manager_t yapısının işaretçisi.
 * @param asset_type Atılacak varlık tipi. FE_ASSET_TYPE_UNKNOWN tüm tipleri atar.
 */
void fe_asset_manager_unload_all_assets_of_type(fe_asset_manager_t* manager, fe_asset_type_t asset_type);

// --- Özel Yükleyici Fonksiyon Deklarasyonları (dahili veya harici olabilir) ---
// Bu fonksiyonlar, gerçek varlık verilerini yüklemekten sorumludur.
// Bunlar, asset manager'dan bağımsız bir varlık yükleme modülü olabilir.
// Şimdilik burada varsayılan olarak tanımlanmıştır.

/**
 * @brief Bir doku varlığını yükler.
 * @param file_path Doku dosyasının yolu.
 * @param asset_out Yüklenen doku varlık verisini tutacak işaretçi (fe_texture_t* olarak cast edilebilir).
 * @param data_size_out Yüklenen verinin boyutunu tutacak işaretçi.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
typedef bool (*fe_asset_loader_func)(const char* file_path, void** asset_out, size_t* data_size_out);

/**
 * @brief Bir doku varlığını boşaltır.
 * @param asset_data Boşaltılacak doku verisine işaretçi (fe_texture_t* olarak cast edilebilir).
 * @param data_size Varlık verisinin boyutu.
 */
typedef void (*fe_asset_unloader_func)(void* asset_data, size_t data_size);


// Harici yükleyiciler için register fonksiyonları
// Gerçek bir motor, farklı dosya formatları için farklı yükleyiciler kaydederdi.
// Örnek:
// void fe_asset_manager_register_loader(fe_asset_type_t type, fe_asset_loader_func loader_func, fe_asset_unloader_func unloader_func);


#endif // FE_ASSET_MANAGER_H
