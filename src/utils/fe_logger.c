#include "include/utils/fe_logger.h"
#include <stdarg.h> // Değişken argümanlar için
#include <string.h> // strlen, strcpy için
#include <time.h>   // Zaman damgası için

#ifdef _WIN32
#include <windows.h> // Windows konsol renkleri için
#endif

// --- Dahili Logger Durumu ---
static struct {
    FILE* log_file;
    fe_log_level_t console_min_level;
    fe_log_level_t file_min_level;
    bool console_output_enabled;
    bool file_output_enabled;
    bool is_initialized;

#ifdef _WIN32
    HANDLE hConsole;
    WORD original_console_attrs;
#endif
} fe_logger_state;

// --- Dahili Yardımcı Fonksiyonlar ---

// Log seviyesini string olarak döndürür
static const char* fe_log_level_to_string(fe_log_level_t level) {
    switch (level) {
        case FE_LOG_LEVEL_DEBUG:    return "DEBUG";
        case FE_LOG_LEVEL_INFO:     return "INFO "; // Boşluklar hizalama için
        case FE_LOG_LEVEL_WARN:     return "WARN ";
        case FE_LOG_LEVEL_ERROR:    return "ERROR";
        case FE_LOG_LEVEL_CRITICAL: return "CRIT ";
        default:                    return "UNKNOWN";
    }
}

// Log seviyesine göre konsol rengini ayarlar
static void fe_log_set_console_color(fe_log_level_t level) {
#ifdef _WIN32
    if (fe_logger_state.hConsole) {
        WORD color = fe_logger_state.original_console_attrs; // Varsayılan

        switch (level) {
            case FE_LOG_LEVEL_DEBUG:    color = FOREGROUND_INTENSITY | FOREGROUND_BLUE | FOREGROUND_GREEN; break; // Cyan benzeri
            case FE_LOG_LEVEL_INFO:     color = FOREGROUND_INTENSITY | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED; break; // Beyaz
            case FE_LOG_LEVEL_WARN:     color = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN; break; // Parlak Sarı
            case FE_LOG_LEVEL_ERROR:    color = FOREGROUND_INTENSITY | FOREGROUND_RED; break; // Parlak Kırmızı
            case FE_LOG_LEVEL_CRITICAL: color = BACKGROUND_RED | FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; break; // Kırmızı Arkaplan, Beyaz yazı
            default: break;
        }
        SetConsoleTextAttribute(fe_logger_state.hConsole, color);
    }
#else
    // UNIX/Linux/macOS için ANSI renk kodları
    switch (level) {
        case FE_LOG_LEVEL_DEBUG:    fprintf(stdout, FE_LOG_COLOR_CYAN); break;
        case FE_LOG_LEVEL_INFO:     fprintf(stdout, FE_LOG_COLOR_WHITE); break;
        case FE_LOG_LEVEL_WARN:     fprintf(stdout, FE_LOG_COLOR_YELLOW); break;
        case FE_LOG_LEVEL_ERROR:    fprintf(stdout, FE_LOG_COLOR_RED); break;
        case FE_LOG_LEVEL_CRITICAL: fprintf(stdout, FE_LOG_COLOR_BRIGHT_RED); break;
        default: break;
    }
#endif
}

// Konsol rengini sıfırlar
static void fe_log_reset_console_color() {
#ifdef _WIN32
    if (fe_logger_state.hConsole) {
        SetConsoleTextAttribute(fe_logger_state.hConsole, fe_logger_state.original_console_attrs);
    }
#else
    fprintf(stdout, FE_LOG_COLOR_RESET);
#endif
}


// --- Genel Loglama Fonksiyonu ---
void fe_log_message(fe_log_level_t level, const char* file, int line, const char* format, ...) {
    if (!fe_logger_state.is_initialized) {
        // Logger başlatılmamışsa, en temel şekilde stderr'a yaz
        fprintf(stderr, "[UNINIT LOG] %s:%d: %s\n", file, line, format);
        return;
    }

    // Mesajın geçerli seviye eşiğinin altında olup olmadığını kontrol et
    if (level < fe_logger_state.console_min_level && level < fe_logger_state.file_min_level) {
        return; // Hem konsol hem de dosya için ilgili log seviyesi eşiğinin altında
    }

    char time_str[32];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    char final_message[2048]; // Maksimum log mesajı boyutu (yeterince büyük olmalı)
    int offset = 0;

    // Zaman damgası ve seviye bilgisi
    offset += snprintf(final_message + offset, sizeof(final_message) - offset,
                       "[%s] [%s]", time_str, fe_log_level_to_string(level));

    // Dosya ve satır bilgisi (sadece DEBUG modunda veya ERROR/CRITICAL için)
#ifdef FE_DEBUG_BUILD
    if (level <= FE_LOG_LEVEL_DEBUG || level >= FE_LOG_LEVEL_ERROR) { // Debug seviyesi aktifse her zaman, diğerleri için sadece hata
#else
    if (level >= FE_LOG_LEVEL_ERROR) { // Üretim modunda sadece hata ve kritik için
#endif
        offset += snprintf(final_message + offset, sizeof(final_message) - offset,
                           " (%s:%d)", file, line);
    }

    offset += snprintf(final_message + offset, sizeof(final_message) - offset, ": ");

    // Kullanıcı mesajını formatla
    va_list args;
    va_start(args, format);
    vsnprintf(final_message + offset, sizeof(final_message) - offset, format, args);
    va_end(args);

    // Konsol çıktısı
    if (fe_logger_state.console_output_enabled && level >= fe_logger_state.console_min_level) {
        fe_log_set_console_color(level);
        fprintf(stdout, "%s\n", final_message);
        fe_log_reset_console_color();
    }

    // Dosya çıktısı
    if (fe_logger_state.file_output_enabled && fe_logger_state.log_file && level >= fe_logger_state.file_min_level) {
        fprintf(fe_logger_state.log_file, "%s\n", final_message);
        fflush(fe_logger_state.log_file); // Dosyaya hemen yazdır
    }
}

// --- Logger Kontrol Fonksiyonları Uygulaması ---

bool fe_logger_init(fe_log_level_t min_level, fe_log_level_t file_min_level, const char* log_file_path) {
    if (fe_logger_state.is_initialized) {
        fprintf(stderr, "[WARN] Logger already initialized.\n");
        return true;
    }

    memset(&fe_logger_state, 0, sizeof(fe_logger_state));
    fe_logger_state.console_min_level = min_level;
    fe_logger_state.file_min_level = file_min_level;
    fe_logger_state.console_output_enabled = true; // Varsayılan olarak konsol etkin
    fe_logger_state.file_output_enabled = false;   // Varsayılan olarak dosya etkin değil

#ifdef _WIN32
    fe_logger_state.hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (fe_logger_state.hConsole != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
        GetConsoleScreenBufferInfo(fe_logger_state.hConsole, &consoleInfo);
        fe_logger_state.original_console_attrs = consoleInfo.wAttributes;
    } else {
        fprintf(stderr, "[WARN] Could not get console handle for Windows color support.\n");
    }
#endif

    if (log_file_path != NULL && strlen(log_file_path) > 0) {
        fe_logger_state.log_file = fopen(log_file_path, "w"); // "w" ile aç, dosya varsa içeriğini siler
        if (fe_logger_state.log_file == NULL) {
            fprintf(stderr, "[ERROR] Failed to open log file: %s\n", log_file_path);
            fe_logger_state.file_output_enabled = false; // Dosya açılamazsa devre dışı bırak
            // Ancak, konsol çıktısı yine de etkin kalabilir
        } else {
            fe_logger_state.file_output_enabled = true;
            fprintf(fe_logger_state.log_file, "--- Fiction Engine Log Started ---\n");
            fprintf(fe_logger_state.log_file, "Log Level (Console): %s, Log Level (File): %s\n",
                    fe_log_level_to_string(fe_logger_state.console_min_level),
                    fe_log_level_to_string(fe_logger_state.file_min_level));
            fflush(fe_logger_state.log_file);
        }
    }

    fe_logger_state.is_initialized = true;
    FE_LOG_INFO("Logger initialized.");
    return true;
}

void fe_logger_shutdown() {
    if (!fe_logger_state.is_initialized) {
        fprintf(stderr, "[WARN] Logger not initialized, cannot shut down.\n");
        return;
    }

    FE_LOG_INFO("Logger shutting down.");

    if (fe_logger_state.log_file) {
        fprintf(fe_logger_state.log_file, "--- Fiction Engine Log Ended ---\n");
        fclose(fe_logger_state.log_file);
        fe_logger_state.log_file = NULL;
    }

#ifdef _WIN32
    // Windows konsol rengini orijinaline geri döndür
    if (fe_logger_state.hConsole) {
        SetConsoleTextAttribute(fe_logger_state.hConsole, fe_logger_state.original_console_attrs);
    }
#endif

    memset(&fe_logger_state, 0, sizeof(fe_logger_state));
}

void fe_logger_set_console_output(bool enabled) {
    if (!fe_logger_state.is_initialized) {
        fprintf(stderr, "[WARN] Logger not initialized, cannot set console output.\n");
        return;
    }
    fe_logger_state.console_output_enabled = enabled;
}

void fe_logger_set_file_output(bool enabled) {
    if (!fe_logger_state.is_initialized) {
        fprintf(stderr, "[WARN] Logger not initialized, cannot set file output.\n");
        return;
    }
    fe_logger_state.file_output_enabled = enabled;
}

void fe_logger_set_console_min_level(fe_log_level_t level) {
    if (!fe_logger_state.is_initialized) {
        fprintf(stderr, "[WARN] Logger not initialized, cannot set console min level.\n");
        return;
    }
    fe_logger_state.console_min_level = level;
    FE_LOG_INFO("Console minimum log level set to: %s", fe_log_level_to_string(level));
}

void fe_logger_set_file_min_level(fe_log_level_t level) {
    if (!fe_logger_state.is_initialized) {
        fprintf(stderr, "[WARN] Logger not initialized, cannot set file min level.\n");
        return;
    }
    if (fe_logger_state.log_file == NULL) {
        FE_LOG_WARN("Log file not open, cannot set file min level.");
        return;
    }
    fe_logger_state.file_min_level = level;
    FE_LOG_INFO("File minimum log level set to: %s", fe_log_level_to_string(level));
}
