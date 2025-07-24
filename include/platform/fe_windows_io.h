#ifndef FE_WINDOWS_IO_H
#define FE_WINDOWS_IO_H

#include "core/utils/fe_types.h" // Temel tipler (bool, uint32_t vb.) için
#include <windows.h>             // Windows API'leri için ana başlık

// --- Dosya Açma Modları ---
typedef enum fe_file_access_mode {
    FE_FILE_ACCESS_READ_ONLY,    // Sadece okuma
    FE_FILE_ACCESS_WRITE_ONLY,   // Sadece yazma (dosya yoksa oluşturulur, varsa içeriği silinir)
    FE_FILE_ACCESS_READ_WRITE,   // Okuma ve yazma (dosya yoksa oluşturulur, varsa içeriği korunur)
    FE_FILE_ACCESS_APPEND,       // Sadece yazma, sona ekle (dosya yoksa oluşturulur)
} fe_file_access_mode_t;

// --- Dosya Paylaşım Modları ---
typedef enum fe_file_share_mode {
    FE_FILE_SHARE_NONE = 0x0,     // Dosya kilitli, başka süreçler açamaz
    FE_FILE_SHARE_READ = 0x1,     // Başka süreçler okuma için açabilir
    FE_FILE_SHARE_WRITE = 0x2,    // Başka süreçler yazma için açabilir
    FE_FILE_SHARE_DELETE = 0x4    // Başka süreçler silme için açabilir (nadiren kullanılır)
} fe_file_share_mode_t; // Windows'daki dwShareMode ile eşleşir

// --- Dosya Oluşturma/Açma Davranışları ---
typedef enum fe_file_creation_disp {
    FE_FILE_CREATE_NEW,          // Dosya varsa hata verir, yoksa oluşturur
    FE_FILE_CREATE_ALWAYS,       // Her zaman yeni dosya oluşturur (varsa üzerine yazar)
    FE_FILE_OPEN_EXISTING,       // Sadece var olan dosyayı açar, yoksa hata verir
    FE_FILE_OPEN_ALWAYS,         // Dosya varsa açar, yoksa oluşturur
    FE_FILE_TRUNCATE_EXISTING    // Var olan dosyayı açar ve boyutunu sıfıra indirir, yoksa hata verir
} fe_file_creation_disp_t;

// --- Dosya Yapısı (Handle tabanlı) ---
typedef struct fe_windows_file {
    HANDLE handle;               // Windows dosya handle'ı
    fe_file_access_mode_t access_mode; // Açılışta belirtilen erişim modu
    uint64_t size;               // Dosya boyutu (bayt cinsinden)
} fe_windows_file_t;

// --- Bellek Eşlemeli Dosya Yapısı ---
typedef struct fe_windows_memory_mapped_file {
    HANDLE file_handle;          // Dosya handle'ı
    HANDLE mapping_handle;       // Dosya eşleme handle'ı
    void* view_ptr;             // Eşlenmiş verinin başlangıç adresi
    uint64_t size;               // Eşlenmiş alanın boyutu
} fe_windows_memory_mapped_file_t;

// --- Fonksiyon Deklarasyonları ---

/**
 * @brief Windows API kullanarak bir dosyayı açar veya oluşturur.
 * @param file fe_windows_file_t yapısının işaretçisi.
 * @param file_path Dosyanın yolu (UTF-16 L-string).
 * @param access_mode Dosya erişim modu (okuma, yazma, vb.).
 * @param share_mode Dosya paylaşım modu.
 * @param creation_disp Dosya oluşturma/açma davranışı.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_winio_open_file(fe_windows_file_t* file, const wchar_t* file_path,
                        fe_file_access_mode_t access_mode, fe_file_share_mode_t share_mode,
                        fe_file_creation_disp_t creation_disp);

/**
 * @brief Açık bir dosyadan veri okur.
 * @param file Okunacak dosya yapısının işaretçisi.
 * @param buffer Okunan verinin yazılacağı tampon.
 * @param bytes_to_read Okunacak bayt sayısı.
 * @param bytes_read Okunan gerçek bayt sayısını tutacak adres.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_winio_read_file(fe_windows_file_t* file, void* buffer, uint32_t bytes_to_read, uint32_t* bytes_read);

/**
 * @brief Açık bir dosyaya veri yazar.
 * @param file Yazılacak dosya yapısının işaretçisi.
 * @param buffer Yazılacak verinin bulunduğu tampon.
 * @param bytes_to_write Yazılacak bayt sayısı.
 * @param bytes_written Yazılan gerçek bayt sayısını tutacak adres.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_winio_write_file(fe_windows_file_t* file, const void* buffer, uint32_t bytes_to_write, uint32_t* bytes_written);

/**
 * @brief Açık bir dosya içindeki okuma/yazma işaretçisini ayarlar.
 * @param file İşaretçisi ayarlanacak dosya yapısının işaretçisi.
 * @param offset Ofset değeri.
 * @param origin Ofset başlangıç noktası (FE_FILE_SEEK_BEGIN, FE_FILE_SEEK_CURRENT, FE_FILE_SEEK_END).
 * @param new_position İşaretçinin yeni pozisyonunu döndürecek adres (isteğe bağlı, NULL olabilir).
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_winio_seek_file(fe_windows_file_t* file, int64_t offset, uint32_t origin, uint64_t* new_position);

// Seek origin tanımlamaları (Windows API'deki karşılıkları)
#define FE_FILE_SEEK_BEGIN   FILE_BEGIN
#define FE_FILE_SEEK_CURRENT FILE_CURRENT
#define FE_FILE_SEEK_END     FILE_END

/**
 * @brief Açık bir dosyanın boyutunu alır.
 * @param file Boyutu alınacak dosya yapısının işaretçisi.
 * @return uint64_t Dosyanın boyutu bayt cinsinden. Hata durumunda 0.
 */
uint64_t fe_winio_get_file_size(fe_windows_file_t* file);

/**
 * @brief Açık bir dosyayı kapatır.
 * @param file Kapatılacak dosya yapısının işaretçisi.
 */
void fe_winio_close_file(fe_windows_file_t* file);

/**
 * @brief Bir dosyanın var olup olmadığını kontrol eder.
 * @param file_path Kontrol edilecek dosyanın yolu (UTF-16 L-string).
 * @return bool Dosya varsa true, yoksa false.
 */
bool fe_winio_file_exists(const wchar_t* file_path);

/**
 * @brief Bir dosyayı siler.
 * @param file_path Silinecek dosyanın yolu (UTF-16 L-string).
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_winio_delete_file(const wchar_t* file_path);

/**
 * @brief Bir dosyayı yeniden adlandırır veya taşır.
 * @param old_path Eski dosya yolu (UTF-16 L-string).
 * @param new_path Yeni dosya yolu (UTF-16 L-string).
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_winio_rename_file(const wchar_t* old_path, const wchar_t* new_path);


// --- Bellek Eşlemeli Dosya Fonksiyonları ---

/**
 * @brief Bir dosyayı belleğe eşler (memory-map).
 * @param mapped_file fe_windows_memory_mapped_file_t yapısının işaretçisi.
 * @param file_path Belleğe eşlenecek dosyanın yolu (UTF-16 L-string).
 * @param access_mode FE_FILE_ACCESS_READ_ONLY veya FE_FILE_ACCESS_READ_WRITE olmalı.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_winio_map_file(fe_windows_memory_mapped_file_t* mapped_file, const wchar_t* file_path, fe_file_access_mode_t access_mode);

/**
 * @brief Belleğe eşlenmiş bir dosyanın eşlemesini kaldırır ve kaynakları serbest bırakır.
 * @param mapped_file Eşlemesi kaldırılacak dosya yapısının işaretçisi.
 */
void fe_winio_unmap_file(fe_windows_memory_mapped_file_t* mapped_file);

#endif // FE_WINDOWS_IO_H
