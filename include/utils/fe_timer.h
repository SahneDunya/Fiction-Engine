#ifndef FE_TIMER_H
#define FE_TIMER_H

#include <stdbool.h> // bool için

// --- Zamanlayıcı Hata Kodları ---
typedef enum fe_timer_error {
    FE_TIMER_SUCCESS = 0,
    FE_TIMER_NOT_INITIALIZED,
    FE_TIMER_PLATFORM_ERROR,
    FE_TIMER_INVALID_STATE
} fe_timer_error_t;

// --- Zamanlayıcı Kontrol Fonksiyonları ---

/**
 * @brief Zamanlayıcı sistemini başlatır.
 * Yüksek çözünürlüklü zamanlayıcıyı ayarlar ve başlangıç zaman damgasını kaydeder.
 * Bu fonksiyon motorun başlangıcında çağrılmalıdır.
 * @return fe_timer_error_t Başarılı olursa FE_TIMER_SUCCESS, aksi takdirde bir hata kodu.
 */
fe_timer_error_t fe_timer_init();

/**
 * @brief Zamanlayıcı sistemini kapatır.
 * Kaynakları serbest bırakır (gerekirse).
 * Bu fonksiyon motorun kapanışında çağrılmalıdır.
 */
void fe_timer_shutdown();

/**
 * @brief Zamanlayıcının durumunu bir sonraki kareye ilerletir.
 * Delta süreyi hesaplar ve FPS'yi günceller.
 * Bu fonksiyon oyun döngüsünün her iterasyonunda çağrılmalıdır.
 */
void fe_timer_tick();

// --- Zamanlayıcı Bilgi Fonksiyonları ---

/**
 * @brief Bir önceki kare ile mevcut kare arasında geçen süreyi saniye cinsinden döndürür.
 * (Delta Time)
 * @return float Delta süresi (saniye).
 */
float fe_timer_get_delta_time();

/**
 * @brief Mevcut kare hızını (Frame Per Second - FPS) döndürür.
 * @return unsigned int Geçerli FPS değeri.
 */
unsigned int fe_timer_get_fps();

/**
 * @brief Motorun başlatılmasından bu yana geçen toplam süreyi saniye cinsinden döndürür.
 * @return double Toplam geçen süre (saniye).
 */
double fe_timer_get_total_time();

/**
 * @brief Yüksek çözünürlüklü zamanlayıcının saniyedeki 'tick' sayısını döndürür.
 * Bu, zamanlayıcının hassasiyetini gösterir.
 * @return long long Saniyedeki tick sayısı (frekans).
 */
long long fe_timer_get_frequency();

#endif // FE_TIMER_H
