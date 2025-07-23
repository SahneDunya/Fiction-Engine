#include "core/utils/fe_timer.h"
#include "core/utils/fe_logger.h" // Loglama için
#include <stdio.h> // printf için (debug/test amaçlı)

// Platforma özgü zamanlama başlıkları
#ifdef _WIN32
#include <windows.h> // QueryPerformanceCounter, QueryPerformanceFrequency için
#else
#include <time.h>    // clock_gettime için (POSIX)
#include <unistd.h>  // _POSIX_MONOTONIC_CLOCK için
#endif

// --- Dahili Zamanlayıcı Durumu ---
static struct {
    long long current_time;     // Mevcut zaman damgası (ticks)
    long long last_time;        // Bir önceki karedeki zaman damgası (ticks)
    long long frequency;        // Saniyedeki zamanlayıcı tick'lerinin sayısı (Hz)
    double delta_time;          // Bir önceki kare ile mevcut kare arasındaki süre (saniye)
    double total_time;          // Motor başlatıldığından beri geçen toplam süre (saniye)
    unsigned int fps;           // Mevcut kare hızı
    unsigned int frames_this_second; // Bu saniyede işlenen kare sayısı
    double fps_timer;           // FPS sayacının en son sıfırlandığı zaman
    bool is_initialized;
} fe_timer_state;

// --- Zamanlayıcı Kontrol Fonksiyonları Uygulaması ---

fe_timer_error_t fe_timer_init() {
    if (fe_timer_state.is_initialized) {
        FE_LOG_WARN("Timer already initialized.");
        return FE_TIMER_SUCCESS;
    }

    // Durumu sıfırla
    memset(&fe_timer_state, 0, sizeof(fe_timer_state));

    // Platforma özgü frekansı al
#ifdef _WIN32
    LARGE_INTEGER li_frequency;
    if (!QueryPerformanceFrequency(&li_frequency)) {
        FE_LOG_CRITICAL("QueryPerformanceFrequency failed! Cannot initialize timer.");
        return FE_TIMER_PLATFORM_ERROR;
    }
    fe_timer_state.frequency = li_frequency.QuadPart;

    // İlk zaman damgasını al
    LARGE_INTEGER li_current_time;
    QueryPerformanceCounter(&li_current_time);
    fe_timer_state.current_time = li_current_time.QuadPart;
    fe_timer_state.last_time = fe_timer_state.current_time;
#else // POSIX (Linux, macOS, BSD)
    // Monotonic saat kullanılmalı ki sistem saat değişikliklerinden etkilenmesin
#ifdef _POSIX_MONOTONIC_CLOCK
    struct timespec ts;
    if (clock_getres(CLOCK_MONOTONIC, &ts) != 0) {
        FE_LOG_CRITICAL("clock_getres(CLOCK_MONOTONIC) failed! Cannot initialize timer.");
        return FE_TIMER_PLATFORM_ERROR;
    }
    // Frekans aslında burada doğrudan verilmez, çözünürlükten hesaplanır.
    // Ancak hassasiyet genellikle nanosaniyedir, yani frekans 1 milyar Hz civarındadır.
    fe_timer_state.frequency = 1000000000LL; // Nanoseconds per second

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        FE_LOG_CRITICAL("clock_gettime(CLOCK_MONOTONIC) failed! Cannot initialize timer.");
        return FE_TIMER_PLATFORM_ERROR;
    }
    fe_timer_state.current_time = (long long)ts.tv_sec * fe_timer_state.frequency + ts.tv_nsec;
    fe_timer_state.last_time = fe_timer_state.current_time;
#else
    // Eğer CLOCK_MONOTONIC yoksa, daha az kesin bir alternatif
    FE_LOG_WARN("CLOCK_MONOTONIC not available, falling back to lower resolution timer.");
    fe_timer_state.frequency = CLOCKS_PER_SEC; // saniyedeki clock tick sayısı
    fe_timer_state.current_time = clock();
    fe_timer_state.last_time = fe_timer_state.current_time;
#endif
#endif

    fe_timer_state.is_initialized = true;
    FE_LOG_INFO("Timer initialized. Frequency: %lld ticks/sec", fe_timer_state.frequency);

    return FE_TIMER_SUCCESS;
}

void fe_timer_shutdown() {
    if (!fe_timer_state.is_initialized) {
        FE_LOG_WARN("Timer not initialized, nothing to shut down.");
        return;
    }

    memset(&fe_timer_state, 0, sizeof(fe_timer_state)); // Durumu sıfırla
    FE_LOG_INFO("Timer shut down.");
}

void fe_timer_tick() {
    if (!fe_timer_state.is_initialized) {
        FE_LOG_ERROR("Timer not initialized, cannot tick!");
        return;
    }

    fe_timer_state.last_time = fe_timer_state.current_time;

#ifdef _WIN32
    LARGE_INTEGER li_current_time;
    QueryPerformanceCounter(&li_current_time);
    fe_timer_state.current_time = li_current_time.QuadPart;
#else
#ifdef _POSIX_MONOTONIC_CLOCK
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    fe_timer_state.current_time = (long long)ts.tv_sec * fe_timer_state.frequency + ts.tv_nsec;
#else
    fe_timer_state.current_time = clock();
#endif
#endif

    // Delta süreyi hesapla
    long long elapsed_ticks = fe_timer_state.current_time - fe_timer_state.last_time;
    if (elapsed_ticks < 0) { // Zamanlayıcı sıfırlandıysa veya bir sorun varsa
        FE_LOG_WARN("Timer detected negative elapsed ticks! Resetting delta time.");
        elapsed_ticks = 0; // Negatif delta time'ı önle
    }

    fe_timer_state.delta_time = (double)elapsed_ticks / fe_timer_state.frequency;
    fe_timer_state.total_time += fe_timer_state.delta_time;

    // FPS'yi güncelle
    fe_timer_state.frames_this_second++;
    fe_timer_state.fps_timer += fe_timer_state.delta_time;

    if (fe_timer_state.fps_timer >= 1.0) { // Bir saniye geçtiyse FPS'yi güncelle
        fe_timer_state.fps = fe_timer_state.frames_this_second;
        fe_timer_state.frames_this_second = 0;
        fe_timer_state.fps_timer = 0.0; // Sayacı sıfırla
        // FE_LOG_DEBUG("FPS: %u, Delta Time: %f ms", fe_timer_state.fps, fe_timer_state.delta_time * 1000.0);
    }
}

// --- Zamanlayıcı Bilgi Fonksiyonları Uygulaması ---

float fe_timer_get_delta_time() {
    return (float)fe_timer_state.delta_time;
}

unsigned int fe_timer_get_fps() {
    return fe_timer_state.fps;
}

double fe_timer_get_total_time() {
    return fe_timer_state.total_time;
}

long long fe_timer_get_frequency() {
    return fe_timer_state.frequency;
}
