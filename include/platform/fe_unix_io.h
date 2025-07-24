#ifndef FE_UNIX_IO_H
#define FE_UNIX_IO_H

#include "core/utils/fe_types.h" // Temel tipler (bool, uint32_t vb.) için

#include <fcntl.h>   // open, O_RDONLY, O_WRONLY vb. için
#include <unistd.h>  // read, write, close, lseek için
#include <sys/stat.h> // stat, fstat için
#include <sys/mman.h> // mmap, munmap için

// --- Dosya Açma Modları ---
typedef enum fe_file_access_mode {
    FE_FILE_ACCESS_READ_ONLY,    // Sadece okuma
    FE_FILE_ACCESS_WRITE_ONLY,   // Sadece yazma (dosya yoksa oluşturulur, varsa içeriği silinir)
    FE_FILE_ACCESS_READ_WRITE,   // Okuma ve yazma (dosya yoksa oluşturulur, varsa içeriği korunur)
    FE_FILE_ACCESS_APPEND,       // Sadece yazma, sona ekle (dosya yoksa oluşturulur)
} fe_file_access_mode_t;

// Unix'te sharing mode genellikle dosyayı açarken izin bitleri ile kontrol edilir.
// Ancak buradaki tanım, Windows'taki gibi explicit bir "share mode" değil,
// daha çok dosya erişim hakları (file permissions) ile ilgilidir.
// Basitlik için burada doğrudan kullanılmıyor, ancak gelecekte eklenebilir.
typedef enum fe_file_share_mode {
    FE_FILE_SHARE_NONE = 0x0,
    FE_FILE_SHARE_READ = 0x1,
    FE_FILE_SHARE_WRITE = 0x2,
} fe_file_share_mode_t;

// --- Dosya Oluşturma/Açma Davranışları ---
typedef enum fe_file_creation_disp {
    FE_FILE_CREATE_NEW,          // Dosya varsa hata verir, yoksa oluşturur (O_EXCL | O_CREAT)
    FE_FILE_CREATE_ALWAYS,       // Her zaman yeni dosya oluşturur (varsa üzerine yazar) (O_TRUNC | O_CREAT)
    FE_FILE_OPEN_EXISTING,       // Sadece var olan dosyayı açar, yoksa hata verir (varsayılan)
    FE_FILE_OPEN_ALWAYS,         // Dosya varsa açar, yoksa oluşturur (O_CREAT)
    FE_FILE_TRUNCATE_EXISTING    // Var olan dosyayı açar ve boyutunu sıfıra indirir, yoksa hata verir (O_TRUNC)
} fe_file_creation_disp_t;

// --- Dosya Yapısı (Dosya Tanımlayıcı tabanlı) ---
typedef struct fe_unix_file {
    int fd;                      // Unix dosya tanımlayıcısı (file descriptor)
    fe_file_access_mode_t access_mode; // Açılışta belirtilen erişim modu
    uint64_t size;               // Dosya boyutu (bayt cinsinden)
} fe_unix_file_t;

// --- Bellek Eşlemeli Dosya Yapısı ---
typedef struct fe_unix_memory_mapped_file {
    int fd;                      // Dosya tanımlayıcısı
    void* view_ptr;              // Eşlenmiş verinin başlangıç adresi
    uint64_t size;               // Eşlenmiş alanın boyutu
} fe_unix_memory_mapped_file_t;

// --- Fonksiyon Deklarasyonları ---

/**
 * @brief Unix sistem çağrılarını kullanarak bir dosyayı açar veya oluşturur.
 * @param file fe_unix_file_t yapısının işaretçisi.
 * @param file_path Dosyanın yolu (UTF-8 char*).
 * @param access_mode Dosya erişim modu (okuma, yazma, vb.).
 * @param creation_disp Dosya oluşturma/açma davranışı.
 * @param mode Yeni dosya için izin bitleri (örn. S_IRUSR | S_IWUSR).
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_unixio_open_file(fe_unix_file_t* file, const char* file_path,
                         fe_file_access_mode_t access_mode, fe_file_creation_disp_t creation_disp,
                         mode_t mode); // mode_t for file permissions (e.g., 0644)

/**
 * @brief Açık bir dosyadan veri okur.
 * @param file Okunacak dosya yapısının işaretçisi.
 * @param buffer Okunan verinin yazılacağı tampon.
 * @param bytes_to_read Okunacak bayt sayısı.
 * @param bytes_read Okunan gerçek bayt sayısını tutacak adres.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_unixio_read_file(fe_unix_file_t* file, void* buffer, uint32_t bytes_to_read, uint32_t* bytes_read);

/**
 * @brief Açık bir dosyaya veri yazar.
 * @param file Yazılacak dosya yapısının işaretçisi.
 * @param buffer Yazılacak verinin bulunduğu tampon.
 * @param bytes_to_write Yazılacak bayt sayısı.
 * @param bytes_written Yazılan gerçek bayt sayısını tutacak adres.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_unixio_write_file(fe_unix_file_t* file, const void* buffer, uint32_t bytes_to_write, uint32_t* bytes_written);

/**
 * @brief Açık bir dosya içindeki okuma/yazma işaretçisini ayarlar.
 * @param file İşaretçisi ayarlanacak dosya yapısının işaretçisi.
 * @param offset Ofset değeri.
 * @param origin Ofset başlangıç noktası (FE_FILE_SEEK_BEGIN, FE_FILE_SEEK_CURRENT, FE_FILE_SEEK_END).
 * @param new_position İşaretçinin yeni pozisyonunu döndürecek adres (isteğe bağlı, NULL olabilir).
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_unixio_seek_file(fe_unix_file_t* file, int64_t offset, int origin, uint64_t* new_position);

// Seek origin tanımlamaları (Unix API'deki karşılıkları)
#define FE_FILE_SEEK_BEGIN   SEEK_SET
#define FE_FILE_SEEK_CURRENT SEEK_CUR
#define FE_FILE_SEEK_END     SEEK_END

/**
 * @brief Açık bir dosyanın boyutunu alır.
 * @param file Boyutu alınacak dosya yapısının işaretçisi.
 * @return uint64_t Dosyanın boyutu bayt cinsinden. Hata durumunda 0.
 */
uint64_t fe_unixio_get_file_size(fe_unix_file_t* file);

/**
 * @brief Açık bir dosyayı kapatır.
 * @param file Kapatılacak dosya yapısının işaretçisi.
 */
void fe_unixio_close_file(fe_unix_file_t* file);

/**
 * @brief Bir dosyanın var olup olmadığını kontrol eder.
 * @param file_path Kontrol edilecek dosyanın yolu (UTF-8 char*).
 * @return bool Dosya varsa true, yoksa false.
 */
bool fe_unixio_file_exists(const char* file_path);

/**
 * @brief Bir dosyayı siler.
 * @param file_path Silinecek dosyanın yolu (UTF-8 char*).
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_unixio_delete_file(const char* file_path);

/**
 * @brief Bir dosyayı yeniden adlandırır veya taşır.
 * @param old_path Eski dosya yolu (UTF-8 char*).
 * @param new_path Yeni dosya yolu (UTF-8 char*).
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_unixio_rename_file(const char* old_path, const char* new_path);


// --- Bellek Eşlemeli Dosya Fonksiyonları ---

/**
 * @brief Bir dosyayı belleğe eşler (memory-map).
 * @param mapped_file fe_unix_memory_mapped_file_t yapısının işaretçisi.
 * @param file_path Belleğe eşlenecek dosyanın yolu (UTF-8 char*).
 * @param access_mode FE_FILE_ACCESS_READ_ONLY veya FE_FILE_ACCESS_READ_WRITE olmalı.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_unixio_map_file(fe_unix_memory_mapped_file_t* mapped_file, const char* file_path, fe_file_access_mode_t access_mode);

/**
 * @brief Belleğe eşlenmiş bir dosyanın eşlemesini kaldırır ve kaynakları serbest bırakır.
 * @param mapped_file Eşlemesi kaldırılacak dosya yapısının işaretçisi.
 */
void fe_unixio_unmap_file(fe_unix_memory_mapped_file_t* mapped_file);

#endif // FE_UNIX_IO_H
