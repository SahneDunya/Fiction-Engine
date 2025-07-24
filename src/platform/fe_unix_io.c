#include "platform/unix/fe_unix_io.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h" // FE_MALLOC, FE_FREE için

#include <string.h>  // memset için
#include <errno.h>   // errno için
#include <stdio.h>   // perror için (geçici olarak, daha iyi loglama için)

// --- Yardımcı Fonksiyonlar ---
// Hata kodunu loglamak için
static void fe_unixio_log_last_error(const char* func_name, const char* file_path) {
    FE_LOG_ERROR("UNIXIO Error in %s for file '%s': %s (Errno: %d)",
                 func_name, file_path ? file_path : "", strerror(errno), errno);
}

// fe_file_access_mode_t'yi Unix'in open() flags değerine dönüştürür
static int fe_access_mode_to_unix_flags(fe_file_access_mode_t mode) {
    switch (mode) {
        case FE_FILE_ACCESS_READ_ONLY:  return O_RDONLY;
        case FE_FILE_ACCESS_WRITE_ONLY: return O_WRONLY;
        case FE_FILE_ACCESS_READ_WRITE: return O_RDWR;
        case FE_FILE_ACCESS_APPEND:     return O_WRONLY | O_APPEND; // O_APPEND ile her yazımda sona ekle
        default: return 0;
    }
}

// fe_file_creation_disp_t'yi Unix'in open() flags değerine dönüştürür
static int fe_creation_disp_to_unix_flags(fe_file_creation_disp_t disp) {
    switch (disp) {
        case FE_FILE_CREATE_NEW:          return O_CREAT | O_EXCL; // Oluştur ve varsa hata ver
        case FE_FILE_CREATE_ALWAYS:       return O_CREAT | O_TRUNC; // Oluştur ve varsa içeriği sil
        case FE_FILE_OPEN_EXISTING:       return 0; // Sadece var olanı aç, ek bayrak yok
        case FE_FILE_OPEN_ALWAYS:         return O_CREAT; // Varsa aç, yoksa oluştur
        case FE_FILE_TRUNCATE_EXISTING:   return O_TRUNC; // Var olanı aç ve içeriği sil
        default:                          return 0;
    }
}

// --- Dosya Fonksiyonları Implementasyonları ---

bool fe_unixio_open_file(fe_unix_file_t* file, const char* file_path,
                         fe_file_access_mode_t access_mode, fe_file_creation_disp_t creation_disp,
                         mode_t mode) {
    if (!file || !file_path) {
        FE_LOG_ERROR("fe_unixio_open_file: Invalid parameters (NULL file or file_path).");
        return false;
    }

    memset(file, 0, sizeof(fe_unix_file_t));
    file->fd = -1; // Başlangıçta geçersiz olarak işaretle

    int oflags = fe_access_mode_to_unix_flags(access_mode) | fe_creation_disp_to_unix_flags(creation_disp);

    // O_APPEND bayrağı zaten fe_access_mode_to_unix_flags içinde ele alınıyor.
    // O_TRUNC bayrağı fe_creation_disp_to_unix_flags içinde ele alınıyor.

    file->fd = open(file_path, oflags, mode);

    if (file->fd == -1) {
        fe_unixio_log_last_error("open", file_path);
        return false;
    }

    file->access_mode = access_mode;
    file->size = fe_unixio_get_file_size(file); // Açılışta boyutu al

    FE_LOG_DEBUG("File opened successfully: '%s' (fd: %d)", file_path, file->fd);
    return true;
}

bool fe_unixio_read_file(fe_unix_file_t* file, void* buffer, uint32_t bytes_to_read, uint32_t* bytes_read) {
    if (!file || file->fd == -1 || !buffer || bytes_to_read == 0) {
        FE_LOG_ERROR("fe_unixio_read_file: Invalid parameters (NULL file/buffer or invalid fd/size).");
        if (bytes_read) *bytes_read = 0;
        return false;
    }

    ssize_t actual_bytes_read = read(file->fd, buffer, bytes_to_read);

    if (actual_bytes_read == -1) {
        fe_unixio_log_last_error("read", ""); // Dosya yolu burada mevcut değil, genel hata logla
        if (bytes_read) *bytes_read = 0;
        return false;
    }

    if (bytes_read) *bytes_read = (uint32_t)actual_bytes_read;
    return true;
}

bool fe_unixio_write_file(fe_unix_file_t* file, const void* buffer, uint32_t bytes_to_write, uint32_t* bytes_written) {
    if (!file || file->fd == -1 || !buffer || bytes_to_write == 0) {
        FE_LOG_ERROR("fe_unixio_write_file: Invalid parameters (NULL file/buffer or invalid fd/size).");
        if (bytes_written) *bytes_written = 0;
        return false;
    }

    ssize_t actual_bytes_written = write(file->fd, buffer, bytes_to_write);

    if (actual_bytes_written == -1) {
        fe_unixio_log_last_error("write", "");
        if (bytes_written) *bytes_written = 0;
        return false;
    }

    if (bytes_written) *bytes_written = (uint32_t)actual_bytes_written;
    file->size = fe_unixio_get_file_size(file); // Boyutu güncelle
    return true;
}

bool fe_unixio_seek_file(fe_unix_file_t* file, int64_t offset, int origin, uint64_t* new_position) {
    if (!file || file->fd == -1) {
        FE_LOG_ERROR("fe_unixio_seek_file: Invalid file descriptor.");
        if (new_position) *new_position = 0;
        return false;
    }

    off_t new_pos = lseek(file->fd, offset, origin);

    if (new_pos == (off_t)-1) {
        fe_unixio_log_last_error("lseek", "");
        if (new_position) *new_position = 0;
        return false;
    }

    if (new_position) *new_position = (uint64_t)new_pos;
    return true;
}

uint64_t fe_unixio_get_file_size(fe_unix_file_t* file) {
    if (!file || file->fd == -1) {
        FE_LOG_ERROR("fe_unixio_get_file_size: Invalid file descriptor.");
        return 0;
    }
    struct stat st;
    if (fstat(file->fd, &st) == -1) {
        fe_unixio_log_last_error("fstat", "");
        return 0;
    }
    return (uint64_t)st.st_size;
}

void fe_unixio_close_file(fe_unix_file_t* file) {
    if (!file || file->fd == -1) {
        return;
    }
    if (close(file->fd) == -1) {
        fe_unixio_log_last_error("close", "");
    }
    file->fd = -1; // Geçersiz olarak işaretle
    file->size = 0;
    FE_LOG_DEBUG("File descriptor closed.");
}

bool fe_unixio_file_exists(const char* file_path) {
    if (!file_path) {
        return false;
    }
    // stat fonksiyonu dosya veya dizin bilgilerini alır.
    // Başarılı olursa 0 döner, hata olursa -1.
    struct stat st;
    if (stat(file_path, &st) == 0) {
        // Dosyanın bir dizin olmadığını da kontrol et (isteğe bağlı ama iyi bir pratik)
        return S_ISREG(st.st_mode);
    }
    // errno değeri ENOENT (No such file or directory) ise dosya yok demektir.
    // Diğer hatalar da olabilir (örn. izinler).
    return false;
}

bool fe_unixio_delete_file(const char* file_path) {
    if (!file_path) {
        FE_LOG_ERROR("fe_unixio_delete_file: Invalid file_path (NULL).");
        return false;
    }
    if (unlink(file_path) == -1) {
        fe_unixio_log_last_error("unlink", file_path);
        return false;
    }
    FE_LOG_INFO("File deleted: '%s'", file_path);
    return true;
}

bool fe_unixio_rename_file(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) {
        FE_LOG_ERROR("fe_unixio_rename_file: Invalid paths (NULL).");
        return false;
    }
    // rename fonksiyonu hedef dosya varsa üzerine yazar.
    if (rename(old_path, new_path) == -1) {
        fe_unixio_log_last_error("rename", old_path);
        return false;
    }
    FE_LOG_INFO("File renamed/moved from '%s' to '%s'", old_path, new_path);
    return true;
}

// --- Bellek Eşlemeli Dosya Fonksiyonları Implementasyonları ---

bool fe_unixio_map_file(fe_unix_memory_mapped_file_t* mapped_file, const char* file_path, fe_file_access_mode_t access_mode) {
    if (!mapped_file || !file_path) {
        FE_LOG_ERROR("fe_unixio_map_file: Invalid parameters (NULL mapped_file or file_path).");
        return false;
    }
    if (access_mode != FE_FILE_ACCESS_READ_ONLY && access_mode != FE_FILE_ACCESS_READ_WRITE) {
        FE_LOG_ERROR("fe_unixio_map_file: Invalid access mode for memory mapping. Only READ_ONLY or READ_WRITE allowed.");
        return false;
    }

    memset(mapped_file, 0, sizeof(fe_unix_memory_mapped_file_t));
    mapped_file->fd = -1;
    mapped_file->view_ptr = NULL;
    mapped_file->size = 0;

    int open_flags = (access_mode == FE_FILE_ACCESS_READ_ONLY) ? O_RDONLY : O_RDWR;
    mapped_file->fd = open(file_path, open_flags);

    if (mapped_file->fd == -1) {
        fe_unixio_log_last_error("open (for mmap)", file_path);
        return false;
    }

    // Dosya boyutunu al
    struct stat st;
    if (fstat(mapped_file->fd, &st) == -1) {
        fe_unixio_log_last_error("fstat (for mmap)", file_path);
        close(mapped_file->fd);
        return false;
    }
    mapped_file->size = (uint64_t)st.st_size;

    if (mapped_file->size == 0) {
        FE_LOG_WARN("fe_unixio_map_file: Attempted to map an empty file: '%s'", file_path);
        close(mapped_file->fd);
        return false;
    }

    int prot_flags = (access_mode == FE_FILE_ACCESS_READ_ONLY) ? PROT_READ : (PROT_READ | PROT_WRITE);
    int map_flags = MAP_SHARED; // Değişiklikler temel dosyaya yansır

    mapped_file->view_ptr = mmap(NULL,           // Sistem bir adres seçsin
                                mapped_file->size, // Eşlenecek bayt sayısı
                                prot_flags,   // Erişim koruması (okuma/yazma)
                                map_flags,    // Paylaşım tipi (dosya ile paylaşım)
                                mapped_file->fd,   // Dosya tanımlayıcısı
                                0);           // Ofset (0 = dosyanın başından itibaren)

    if (mapped_file->view_ptr == MAP_FAILED) {
        fe_unixio_log_last_error("mmap", file_path);
        close(mapped_file->fd);
        mapped_file->fd = -1;
        return false;
    }

    FE_LOG_INFO("File '%s' successfully memory-mapped (size: %llu bytes).", file_path, mapped_file->size);
    return true;
}

void fe_unixio_unmap_file(fe_unix_memory_mapped_file_t* mapped_file) {
    if (!mapped_file || mapped_file->view_ptr == NULL) {
        return;
    }

    if (munmap(mapped_file->view_ptr, mapped_file->size) == -1) {
        fe_unixio_log_last_error("munmap", "");
    }
    mapped_file->view_ptr = NULL;

    if (mapped_file->fd != -1) {
        if (close(mapped_file->fd) == -1) {
            fe_unixio_log_last_error("close (mmap file fd)", "");
        }
        mapped_file->fd = -1;
    }
    mapped_file->size = 0;
    FE_LOG_DEBUG("Memory-mapped file unmapped and file descriptor closed.");
}
