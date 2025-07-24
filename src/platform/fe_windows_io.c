#include "platform/windows/fe_windows_io.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h" // FE_MALLOC, FE_FREE için

#include <string.h> // memset için

// --- Yardımcı Fonksiyonlar ---
// Hata kodunu loglamak için
static void fe_winio_log_last_error(const char* func_name, const wchar_t* file_path) {
    DWORD error_code = GetLastError();
    LPWSTR error_message = NULL;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&error_message,
        0,
        NULL
    );

    if (error_message) {
        FE_LOG_ERROR("WINIO Error in %s for file '%ls': (Code: %lu) %ls",
                     func_name, file_path ? file_path : L"", error_code, error_message);
        LocalFree(error_message);
    } else {
        FE_LOG_ERROR("WINIO Error in %s for file '%ls': Unknown error (Code: %lu)",
                     func_name, file_path ? file_path : L"", error_code);
    }
}

// fe_file_access_mode_t'yi Windows'un dwDesiredAccess değerine dönüştürür
static DWORD fe_access_mode_to_win_access(fe_file_access_mode_t mode) {
    switch (mode) {
        case FE_FILE_ACCESS_READ_ONLY:  return GENERIC_READ;
        case FE_FILE_ACCESS_WRITE_ONLY: return GENERIC_WRITE;
        case FE_FILE_ACCESS_READ_WRITE: return GENERIC_READ | GENERIC_WRITE;
        case FE_FILE_ACCESS_APPEND:     return GENERIC_WRITE; // Append için sadece yazma erişimi yeterli
        default: return 0;
    }
}

// fe_file_creation_disp_t'yi Windows'un dwCreationDisposition değerine dönüştürür
static DWORD fe_creation_disp_to_win_disp(fe_file_creation_disp_t disp, fe_file_access_mode_t access_mode) {
    switch (disp) {
        case FE_FILE_CREATE_NEW:          return CREATE_NEW;
        case FE_FILE_CREATE_ALWAYS:       return CREATE_ALWAYS;
        case FE_FILE_OPEN_EXISTING:       return OPEN_EXISTING;
        case FE_FILE_OPEN_ALWAYS:         return OPEN_ALWAYS;
        case FE_FILE_TRUNCATE_EXISTING:   return TRUNCATE_EXISTING;
        default:                          return OPEN_EXISTING; // Varsayılan güvenli davranış
    }
}

// --- Dosya Fonksiyonları Implementasyonları ---

bool fe_winio_open_file(fe_windows_file_t* file, const wchar_t* file_path,
                        fe_file_access_mode_t access_mode, fe_file_share_mode_t share_mode,
                        fe_file_creation_disp_t creation_disp) {
    if (!file || !file_path) {
        FE_LOG_ERROR("fe_winio_open_file: Invalid parameters (NULL file or file_path).");
        return false;
    }

    memset(file, 0, sizeof(fe_windows_file_t));
    file->handle = INVALID_HANDLE_VALUE; // Başlangıçta geçersiz olarak işaretle

    DWORD desired_access = fe_access_mode_to_win_access(access_mode);
    DWORD share_flags = (DWORD)share_mode;
    DWORD creation_disposition = fe_creation_disp_to_win_disp(creation_disp, access_mode);

    // Append modu için özel işlem:
    // Dosyayı OPEN_ALWAYS ile aç, sonra sona git
    if (access_mode == FE_FILE_ACCESS_APPEND) {
        // Append için dosya oluşturulur veya açılır
        file->handle = CreateFileW(file_path,
                                   GENERIC_WRITE, // Sadece yazma erişimi
                                   share_flags,
                                   NULL,          // Güvenlik özellikleri
                                   OPEN_ALWAYS,   // Dosya yoksa oluştur, varsa aç
                                   FILE_ATTRIBUTE_NORMAL,
                                   NULL);
        if (file->handle == INVALID_HANDLE_VALUE) {
            fe_winio_log_last_error("CreateFileW (Append)", file_path);
            return false;
        }
        // Dosya sonuna git
        if (SetFilePointerEx(file->handle, 0, NULL, FILE_END) == 0) {
            fe_winio_log_last_error("SetFilePointerEx (Append)", file_path);
            CloseHandle(file->handle);
            file->handle = INVALID_HANDLE_VALUE;
            return false;
        }
    } else {
        file->handle = CreateFileW(file_path,
                                   desired_access,
                                   share_flags,
                                   NULL,
                                   creation_disposition,
                                   FILE_ATTRIBUTE_NORMAL,
                                   NULL);
    }

    if (file->handle == INVALID_HANDLE_VALUE) {
        fe_winio_log_last_error("CreateFileW", file_path);
        return false;
    }

    file->access_mode = access_mode;
    file->size = fe_winio_get_file_size(file); // Açılışta boyutu al

    FE_LOG_DEBUG("File opened successfully: '%ls'", file_path);
    return true;
}

bool fe_winio_read_file(fe_windows_file_t* file, void* buffer, uint32_t bytes_to_read, uint32_t* bytes_read) {
    if (!file || file->handle == INVALID_HANDLE_VALUE || !buffer || bytes_to_read == 0) {
        FE_LOG_ERROR("fe_winio_read_file: Invalid parameters (NULL file/buffer or invalid handle/size).");
        if (bytes_read) *bytes_read = 0;
        return false;
    }

    DWORD actual_bytes_read = 0;
    if (!ReadFile(file->handle, buffer, bytes_to_read, &actual_bytes_read, NULL)) {
        fe_winio_log_last_error("ReadFile", L""); // Dosya yolu burada mevcut değil, genel hata logla
        if (bytes_read) *bytes_read = 0;
        return false;
    }

    if (bytes_read) *bytes_read = actual_bytes_read;
    return true;
}

bool fe_winio_write_file(fe_windows_file_t* file, const void* buffer, uint32_t bytes_to_write, uint32_t* bytes_written) {
    if (!file || file->handle == INVALID_HANDLE_VALUE || !buffer || bytes_to_write == 0) {
        FE_LOG_ERROR("fe_winio_write_file: Invalid parameters (NULL file/buffer or invalid handle/size).");
        if (bytes_written) *bytes_written = 0;
        return false;
    }

    DWORD actual_bytes_written = 0;
    if (!WriteFile(file->handle, buffer, bytes_to_write, &actual_bytes_written, NULL)) {
        fe_winio_log_last_error("WriteFile", L"");
        if (bytes_written) *bytes_written = 0;
        return false;
    }

    if (bytes_written) *bytes_written = actual_bytes_written;
    file->size = fe_winio_get_file_size(file); // Boyutu güncelle
    return true;
}

bool fe_winio_seek_file(fe_windows_file_t* file, int64_t offset, uint32_t origin, uint64_t* new_position) {
    if (!file || file->handle == INVALID_HANDLE_VALUE) {
        FE_LOG_ERROR("fe_winio_seek_file: Invalid file handle.");
        if (new_position) *new_position = 0;
        return false;
    }

    LARGE_INTEGER li_offset;
    li_offset.QuadPart = offset;
    LARGE_INTEGER li_new_pos;

    if (!SetFilePointerEx(file->handle, li_offset, &li_new_pos, origin)) {
        fe_winio_log_last_error("SetFilePointerEx", L"");
        if (new_position) *new_position = 0;
        return false;
    }

    if (new_position) *new_position = li_new_pos.QuadPart;
    return true;
}

uint64_t fe_winio_get_file_size(fe_windows_file_t* file) {
    if (!file || file->handle == INVALID_HANDLE_VALUE) {
        FE_LOG_ERROR("fe_winio_get_file_size: Invalid file handle.");
        return 0;
    }
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file->handle, &file_size)) {
        fe_winio_log_last_error("GetFileSizeEx", L"");
        return 0;
    }
    return file_size.QuadPart;
}

void fe_winio_close_file(fe_windows_file_t* file) {
    if (!file || file->handle == INVALID_HANDLE_VALUE) {
        return;
    }
    if (!CloseHandle(file->handle)) {
        fe_winio_log_last_error("CloseHandle", L"");
    }
    file->handle = INVALID_HANDLE_VALUE; // Geçersiz olarak işaretle
    file->size = 0;
    FE_LOG_DEBUG("File handle closed.");
}

bool fe_winio_file_exists(const wchar_t* file_path) {
    if (!file_path) {
        return false;
    }
    // GetFileAttributesW fonksiyonu, dosya veya dizin niteliklerini alır.
    // Fonksiyon başarısız olursa (INVALID_FILE_ATTRIBUTES), dosya mevcut değildir.
    DWORD attributes = GetFileAttributesW(file_path);
    return (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY));
}

bool fe_winio_delete_file(const wchar_t* file_path) {
    if (!file_path) {
        FE_LOG_ERROR("fe_winio_delete_file: Invalid file_path (NULL).");
        return false;
    }
    if (!DeleteFileW(file_path)) {
        fe_winio_log_last_error("DeleteFileW", file_path);
        return false;
    }
    FE_LOG_INFO("File deleted: '%ls'", file_path);
    return true;
}

bool fe_winio_rename_file(const wchar_t* old_path, const wchar_t* new_path) {
    if (!old_path || !new_path) {
        FE_LOG_ERROR("fe_winio_rename_file: Invalid paths (NULL).");
        return false;
    }
    // MoveFileExW'nin MOVEFILE_REPLACE_EXISTING bayrağı hedef dosya varsa üzerine yazar.
    // Aksi takdirde, var olan bir dosyayı üzerine yazmaya çalışırsa başarısız olur.
    if (!MoveFileExW(old_path, new_path, MOVEFILE_REPLACE_EXISTING)) {
        fe_winio_log_last_error("MoveFileExW", old_path);
        return false;
    }
    FE_LOG_INFO("File renamed/moved from '%ls' to '%ls'", old_path, new_path);
    return true;
}


// --- Bellek Eşlemeli Dosya Fonksiyonları Implementasyonları ---

bool fe_winio_map_file(fe_windows_memory_mapped_file_t* mapped_file, const wchar_t* file_path, fe_file_access_mode_t access_mode) {
    if (!mapped_file || !file_path) {
        FE_LOG_ERROR("fe_winio_map_file: Invalid parameters (NULL mapped_file or file_path).");
        return false;
    }
    if (access_mode != FE_FILE_ACCESS_READ_ONLY && access_mode != FE_FILE_ACCESS_READ_WRITE) {
        FE_LOG_ERROR("fe_winio_map_file: Invalid access mode for memory mapping. Only READ_ONLY or READ_WRITE allowed.");
        return false;
    }

    memset(mapped_file, 0, sizeof(fe_windows_memory_mapped_file_t));
    mapped_file->file_handle = INVALID_HANDLE_VALUE;
    mapped_file->mapping_handle = NULL;
    mapped_file->view_ptr = NULL;
    mapped_file->size = 0;

    DWORD desired_access = fe_access_mode_to_win_access(access_mode);
    DWORD file_map_access = (access_mode == FE_FILE_ACCESS_READ_ONLY) ? PAGE_READONLY : PAGE_READWRITE;
    DWORD view_access = (access_mode == FE_FILE_ACCESS_READ_ONLY) ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS;


    mapped_file->file_handle = CreateFileW(file_path,
                                           desired_access,
                                           FILE_SHARE_READ, // Okuma paylaşımına izin ver
                                           NULL,            // Güvenlik özellikleri
                                           OPEN_EXISTING,   // Sadece var olan dosyayı aç
                                           FILE_ATTRIBUTE_NORMAL,
                                           NULL);

    if (mapped_file->file_handle == INVALID_HANDLE_VALUE) {
        fe_winio_log_last_error("CreateFileW (for mapping)", file_path);
        return false;
    }

    // Dosya boyutunu al
    LARGE_INTEGER file_size_li;
    if (!GetFileSizeEx(mapped_file->file_handle, &file_size_li)) {
        fe_winio_log_last_error("GetFileSizeEx (for mapping)", file_path);
        CloseHandle(mapped_file->file_handle);
        return false;
    }
    mapped_file->size = file_size_li.QuadPart;

    if (mapped_file->size == 0) {
        FE_LOG_WARN("fe_winio_map_file: Attempted to map an empty file: '%ls'", file_path);
        CloseHandle(mapped_file->file_handle);
        return false;
    }

    mapped_file->mapping_handle = CreateFileMappingW(mapped_file->file_handle,
                                                     NULL,          // Güvenlik özellikleri
                                                     file_map_access, // Koruma (READONLY veya READWRITE)
                                                     0,             // Maksimum boyut (üst 32 bit)
                                                     0,             // Maksimum boyut (alt 32 bit) - 0 ise dosyanın boyutu kullanılır
                                                     NULL);         // Eşleme objesi adı

    if (mapped_file->mapping_handle == NULL) {
        fe_winio_log_last_error("CreateFileMappingW", file_path);
        CloseHandle(mapped_file->file_handle);
        return false;
    }

    mapped_file->view_ptr = MapViewOfFile(mapped_file->mapping_handle,
                                          view_access, // Erişim (FILE_MAP_READ veya FILE_MAP_ALL_ACCESS)
                                          0,           // Ofset (üst 32 bit)
                                          0,           // Ofset (alt 32 bit)
                                          0);          // Bayt sayısı (0 ise tüm dosya)

    if (mapped_file->view_ptr == NULL) {
        fe_winio_log_last_error("MapViewOfFile", file_path);
        CloseHandle(mapped_file->mapping_handle);
        CloseHandle(mapped_file->file_handle);
        return false;
    }

    FE_LOG_INFO("File '%ls' successfully memory-mapped (size: %llu bytes).", file_path, mapped_file->size);
    return true;
}

void fe_winio_unmap_file(fe_windows_memory_mapped_file_t* mapped_file) {
    if (!mapped_file || mapped_file->view_ptr == NULL) {
        return;
    }

    if (!UnmapViewOfFile(mapped_file->view_ptr)) {
        fe_winio_log_last_error("UnmapViewOfFile", L"");
    }
    mapped_file->view_ptr = NULL;

    if (mapped_file->mapping_handle != NULL) {
        if (!CloseHandle(mapped_file->mapping_handle)) {
            fe_winio_log_last_error("CloseHandle (mapping)", L"");
        }
        mapped_file->mapping_handle = NULL;
    }

    if (mapped_file->file_handle != INVALID_HANDLE_VALUE) {
        if (!CloseHandle(mapped_file->file_handle)) {
            fe_winio_log_last_error("CloseHandle (file)", L"");
        }
        mapped_file->file_handle = INVALID_HANDLE_VALUE;
    }
    mapped_file->size = 0;
    FE_LOG_DEBUG("Memory-mapped file unmapped and handles closed.");
}
