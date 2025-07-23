#ifndef FE_LOGGER_H
#define FE_LOGGER_H

#include <stdio.h>   // FILE ve fprintf için
#include <stdbool.h> // bool için

// --- Log Seviyeleri ---
typedef enum fe_log_level {
    FE_LOG_LEVEL_DEBUG = 0, // En düşük seviye: Detaylı hata ayıklama bilgisi
    FE_LOG_LEVEL_INFO,      // Bilgilendirme mesajları
    FE_LOG_LEVEL_WARN,      // Uyarı mesajları (potansiyel sorunlar)
    FE_LOG_LEVEL_ERROR,     // Hata mesajları (işlemi durdurmayan hatalar)
    FE_LOG_LEVEL_CRITICAL,  // Kritik hata mesajları (uygulamanın kararlılığını etkileyen)
    FE_LOG_LEVEL_NONE       // Hiçbir logu gösterme (sadece kapatmak için)
} fe_log_level_t;

// --- Renk Kodları (ANSI Escape Kodları) ---
#ifdef _WIN32
    // Windows için özel renk kodları veya API çağrıları gerekebilir.
    // Şimdilik ANSI kodları denenir, olmadığında kapatılır.
    #define FE_LOG_COLOR_RESET   ""
    #define FE_LOG_COLOR_RED     ""
    #define FE_LOG_COLOR_GREEN   ""
    #define FE_LOG_COLOR_YELLOW  ""
    #define FE_LOG_COLOR_BLUE    ""
    #define FE_LOG_COLOR_MAGENTA ""
    #define FE_LOG_COLOR_CYAN    ""
    #define FE_LOG_COLOR_WHITE   ""
    #define FE_LOG_COLOR_BRIGHT_RED    ""
#else
    // UNIX/Linux/macOS için ANSI renk kodları
    #define FE_LOG_COLOR_RESET   "\033[0m"
    #define FE_LOG_COLOR_RED     "\033[0;31m"
    #define FE_LOG_COLOR_GREEN   "\033[0;32m"
    #define FE_LOG_COLOR_YELLOW  "\033[0;33m"
    #define FE_LOG_COLOR_BLUE    "\033[0;34m"
    #define FE_LOG_COLOR_MAGENTA "\033[0;35m"
    #define FE_LOG_COLOR_CYAN    "\033[0;36m"
    #define FE_LOG_COLOR_WHITE   "\033[0;37m"
    #define FE_LOG_COLOR_BRIGHT_RED    "\033[1;31m"
#endif

// --- Loglama Fonksiyonu Deklarasyonu ---

/**
 * @brief Genel loglama fonksiyonu. Direkt olarak çağrılmamalı, makrolar kullanılmalıdır.
 *
 * @param level Log seviyesi (DEBUG, INFO, WARN, ERROR, CRITICAL).
 * @param file Log mesajının geldiği kaynak dosya adı.
 * @param line Log mesajının geldiği kaynak satır numarası.
 * @param format Format string'i (printf tarzı).
 * @param ... Değişken argümanlar.
 */
void fe_log_message(fe_log_level_t level, const char* file, int line, const char* format, ...);

// --- Log Makroları ---
// Makrolar __FILE__ ve __LINE__ otomatik olarak geçirerek hata ayıklamayı kolaylaştırır.
// __VA_ARGS__ kullanarak değişken argümanları destekler.

// DEBUG seviyesi genellikle sadece geliştirme ortamında etkin olmalıdır.
#ifdef FE_DEBUG_BUILD
    #define FE_LOG_DEBUG(fmt, ...) fe_log_message(FE_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
    #define FE_LOG_DEBUG(fmt, ...) do {} while(0) // Üretim derlemesinde hiçbir şey yapma
#endif

#define FE_LOG_INFO(fmt, ...)      fe_log_message(FE_LOG_LEVEL_INFO,      __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define FE_LOG_WARN(fmt, ...)      fe_log_message(FE_LOG_LEVEL_WARN,      __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define FE_LOG_ERROR(fmt, ...)     fe_log_message(FE_LOG_LEVEL_ERROR,     __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define FE_LOG_CRITICAL(fmt, ...)  fe_log_message(FE_LOG_LEVEL_CRITICAL,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)


// --- Logger Kontrol Fonksiyonları ---

/**
 * @brief Logger sistemini başlatır.
 * Konsol çıktısı varsayılan olarak etkindir.
 * Eğer fileName NULL değilse, log dosyası oluşturulur ve açılır.
 *
 * @param min_level Konsolda gösterilecek minimum log seviyesi.
 * @param file_min_level Dosyaya yazılacak minimum log seviyesi (dosya etkinse).
 * @param log_file_path Log dosyasının yolu ve adı. NULL ise dosyaya yazılmaz.
 * @return bool Başarılı olursa true, aksi takdirde false.
 */
bool fe_logger_init(fe_log_level_t min_level, fe_log_level_t file_min_level, const char* log_file_path);

/**
 * @brief Logger sistemini kapatır ve açık olan log dosyasını kapatır.
 */
void fe_logger_shutdown();

/**
 * @brief Konsola loglamanın etkin olup olmadığını ayarlar.
 *
 * @param enabled True ise etkinleştir, false ise devre dışı bırak.
 */
void fe_logger_set_console_output(bool enabled);

/**
 * @brief Dosyaya loglamanın etkin olup olmadığını ayarlar.
 *
 * @param enabled True ise etkinleştir, false ise devre dışı bırak.
 */
void fe_logger_set_file_output(bool enabled);

/**
 * @brief Konsol için minimum log seviyesini ayarlar.
 *
 * @param level Yeni minimum log seviyesi.
 */
void fe_logger_set_console_min_level(fe_log_level_t level);

/**
 * @brief Log dosyası için minimum log seviyesini ayarlar.
 *
 * @param level Yeni minimum log seviyesi.
 */
void fe_logger_set_file_min_level(fe_log_level_t level);

#endif // FE_LOGGER_H
