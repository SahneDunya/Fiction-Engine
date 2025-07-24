#ifndef FE_INPUT_MANAGER_H
#define FE_INPUT_MANAGER_H

#include "core/utils/fe_types.h" // Temel tipler için
#include "core/math/fe_vec2.h"   // 2D vektörler (fare pozisyonu için)
#include "SDL3/SDL.h"            // SDL3 kütüphanesi

// --- Klavye Girdi Durumları ---
// Maksimum tuş sayısı için SDL_NUM_SCANCODES veya özel bir sayı kullanılabilir.
#define FE_MAX_KEYBOARD_KEYS SDL_NUM_SCANCODES

typedef struct fe_keyboard_state {
    bool current_keys[FE_MAX_KEYBOARD_KEYS]; // Anlık tuş durumu (basılı mı?)
    bool prev_keys[FE_MAX_KEYBOARD_KEYS];    // Önceki karedeki tuş durumu
} fe_keyboard_state_t;

// --- Fare Girdi Durumları ---
#define FE_MAX_MOUSE_BUTTONS 8 // Genellikle 3-5 düğme yeterli olur, daha fazlası için genişletilebilir

typedef struct fe_mouse_state {
    fe_vec2_t position;         // Fare imlecinin ekran koordinatları
    fe_vec2_t delta_position;   // Son kareden bu yana fare hareketi (X, Y)
    int32_t wheel_delta_x;      // Fare tekerleği yatay kaydırma
    int32_t wheel_delta_y;      // Fare tekerleği dikey kaydırma
    bool current_buttons[FE_MAX_MOUSE_BUTTONS]; // Anlık düğme durumu (basılı mı?)
    bool prev_buttons[FE_MAX_MOUSE_BUTTONS];    // Önceki karedeki düğme durumu
} fe_mouse_state_t;

// --- Dokunmatik Girdi Durumları ---
// Çoklu dokunuşları desteklemek için her bir dokunuşu takip edelim.
#define FE_MAX_TOUCH_FINGERS 10 // Aynı anda algılanabilecek maksimum parmak sayısı

typedef struct fe_touch_finger_state {
    SDL_FingerID finger_id;     // Eşsiz parmak ID'si
    fe_vec2_t position;         // Ekran koordinatları (normalize edilmiş [0,1])
    fe_vec2_t delta_position;   // Son kareden bu yana hareket
    bool is_down;               // Parmak şu an basılı mı?
    bool was_down_prev_frame;   // Önceki karede basılı mıydı?
} fe_touch_finger_state_t;

typedef struct fe_touch_state {
    fe_touch_finger_state_t fingers[FE_MAX_TOUCH_FINGERS];
    uint32_t active_finger_count; // Aktif basılı parmak sayısı
} fe_touch_state_t;

// --- Girdi Yöneticisi Ana Yapısı ---
typedef struct fe_input_manager {
    fe_keyboard_state_t keyboard;
    fe_mouse_state_t mouse;
    fe_touch_state_t touch;

    // SDL olaylarını işlemek için pencere referansı (SDL_PollEvent genellikle bir pencereye bağlı değildir,
    // ancak bazı özel olaylar için pencere bağlamı gerekebilir.)
    // SDL_Window* window_handle; // Eğer belirli bir pencereye özel girdi işleme olacaksa

    bool quit_requested; // Uygulamadan çıkış isteği (örn. pencereyi kapatma)
} fe_input_manager_t;

// --- Girdi Yöneticisi Fonksiyonları ---

/**
 * @brief Girdi yöneticisi sistemini başlatır.
 * @param manager Başlatılacak girdi yöneticisi yapısının işaretçisi.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_input_manager_init(fe_input_manager_t* manager);

/**
 * @brief Girdi yöneticisi sistemini ve ilişkili tüm kaynakları serbest bırakır.
 * @param manager Serbest bırakılacak girdi yöneticisi yapısının işaretçisi.
 */
void fe_input_manager_destroy(fe_input_manager_t* manager);

/**
 * @brief SDL olay kuyruğunu işler ve girdi durumlarını günceller.
 * Bu fonksiyon her oyun döngüsü başında çağrılmalıdır.
 * @param manager Girdi yöneticisi işaretçisi.
 */
void fe_input_manager_process_events(fe_input_manager_t* manager);

/**
 * @brief Belirli bir klavye tuşunun basılı olup olmadığını kontrol eder.
 * @param manager Girdi yöneticisi işaretçisi.
 * @param scancode SDL tarama kodu (örn. SDL_SCANCODE_W).
 * @return bool Tuş basılı ise true, değilse false.
 */
bool fe_input_is_key_down(const fe_input_manager_t* manager, SDL_Scancode scancode);

/**
 * @brief Belirli bir klavye tuşunun bu karede yeni basıldığını kontrol eder.
 * (Sadece bir kez tetiklenir)
 * @param manager Girdi yöneticisi işaretçisi.
 * @param scancode SDL tarama kodu.
 * @return bool Tuş bu karede yeni basıldı ise true, değilse false.
 */
bool fe_input_is_key_pressed(const fe_input_manager_t* manager, SDL_Scancode scancode);

/**
 * @brief Belirli bir klavye tuşunun bu karede serbest bırakıldığını kontrol eder.
 * @param manager Girdi yöneticisi işaretçisi.
 * @param scancode SDL tarama kodu.
 * @return bool Tuş bu karede serbest bırakıldı ise true, değilse false.
 */
bool fe_input_is_key_released(const fe_input_manager_t* manager, SDL_Scancode scancode);

/**
 * @brief Belirli bir fare düğmesinin basılı olup olmadığını kontrol eder.
 * @param manager Girdi yöneticisi işaretçisi.
 * @param button Fare düğmesi kodu (örn. SDL_BUTTON_LEFT).
 * @return bool Düğme basılı ise true, değilse false.
 */
bool fe_input_is_mouse_button_down(const fe_input_manager_t* manager, uint8_t button);

/**
 * @brief Belirli bir fare düğmesinin bu karede yeni basıldığını kontrol eder.
 * @param manager Girdi yöneticisi işaretçisi.
 * @param button Fare düğmesi kodu.
 * @return bool Düğme bu karede yeni basıldı ise true, değilse false.
 */
bool fe_input_is_mouse_button_pressed(const fe_input_manager_t* manager, uint8_t button);

/**
 * @brief Belirli bir fare düğmesinin bu karede serbest bırakıldığını kontrol eder.
 * @param manager Girdi yöneticisi işaretçisi.
 * @param button Fare düğmesi kodu.
 * @return bool Düğme bu karede serbest bırakıldı ise true, değilse false.
 */
bool fe_input_is_mouse_button_released(const fe_input_manager_t* manager, uint8_t button);

/**
 * @brief Fare imlecinin mevcut pozisyonunu döndürür.
 * @param manager Girdi yöneticisi işaretçisi.
 * @return fe_vec2_t Fare imlecinin pozisyonu.
 */
fe_vec2_t fe_input_get_mouse_position(const fe_input_manager_t* manager);

/**
 * @brief Son kareden bu yana fare imlecinin hareket deltasını döndürür.
 * @param manager Girdi yöneticisi işaretçisi.
 * @return fe_vec2_t Fare imlecinin delta pozisyonu.
 */
fe_vec2_t fe_input_get_mouse_delta(const fe_input_manager_t* manager);

/**
 * @brief Fare tekerleği dikey kaydırma deltasını döndürür.
 * @param manager Girdi yöneticisi işaretçisi.
 * @return int32_t Dikey kaydırma miktarı.
 */
int32_t fe_input_get_mouse_wheel_delta_y(const fe_input_manager_t* manager);

/**
 * @brief Fare tekerleği yatay kaydırma deltasını döndürür.
 * @param manager Girdi yöneticisi işaretçisi.
 * @return int32_t Yatay kaydırma miktarı.
 */
int32_t fe_input_get_mouse_wheel_delta_x(const fe_input_manager_t* manager);

/**
 * @brief Belirli bir dokunma parmağının durumunu döndürür.
 * @param manager Girdi yöneticisi işaretçisi.
 * @param index Dokunma parmağının indeksi (0 ile FE_MAX_TOUCH_FINGERS-1 arası).
 * @return const fe_touch_finger_state_t* Parmak durumu işaretçisi veya geçersiz indeks için NULL.
 */
const fe_touch_finger_state_t* fe_input_get_touch_finger_state(const fe_input_manager_t* manager, uint32_t index);

/**
 * @brief Uygulamadan çıkış isteği olup olmadığını kontrol eder.
 * @param manager Girdi yöneticisi işaretçisi.
 * @return bool Çıkış isteği varsa true, aksi takdirde false.
 */
bool fe_input_should_quit(const fe_input_manager_t* manager);

#endif // FE_INPUT_MANAGER_H
