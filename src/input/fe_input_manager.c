#include "input/fe_input_manager.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h" // fe_malloc/free için (gerekirse, burada doğrudan SDL kullanılıyor)

#include <string.h> // memset, memcpy için

// --- Girdi Yöneticisi Fonksiyon Implementasyonları ---

bool fe_input_manager_init(fe_input_manager_t* manager) {
    if (!manager) {
        FE_LOG_ERROR("fe_input_manager_init: Manager pointer is NULL.");
        return false;
    }
    memset(manager, 0, sizeof(fe_input_manager_t));

    // Klavye durumlarını sıfırla
    memset(manager->keyboard.current_keys, 0, sizeof(manager->keyboard.current_keys));
    memset(manager->keyboard.prev_keys, 0, sizeof(manager->keyboard.prev_keys));

    // Fare durumlarını sıfırla
    manager->mouse.position = FE_VEC2_ZERO;
    manager->mouse.delta_position = FE_VEC2_ZERO;
    manager->mouse.wheel_delta_x = 0;
    manager->mouse.wheel_delta_y = 0;
    memset(manager->mouse.current_buttons, 0, sizeof(manager->mouse.current_buttons));
    memset(manager->mouse.prev_buttons, 0, sizeof(manager->mouse.prev_buttons));

    // Dokunmatik durumları sıfırla
    for (int i = 0; i < FE_MAX_TOUCH_FINGERS; ++i) {
        manager->touch.fingers[i].finger_id = SDL_FINGER_INVALID;
        manager->touch.fingers[i].position = FE_VEC2_ZERO;
        manager->touch.fingers[i].delta_position = FE_VEC2_ZERO;
        manager->touch.fingers[i].is_down = false;
        manager->touch.fingers[i].was_down_prev_frame = false;
    }
    manager->touch.active_finger_count = 0;

    manager->quit_requested = false;

    FE_LOG_INFO("Input Manager initialized.");
    return true;
}

void fe_input_manager_destroy(fe_input_manager_t* manager) {
    if (!manager) return;

    FE_LOG_INFO("Input Manager destroyed.");
    // Bellek temizliği memset ile init sırasında yapıldığı için burada ek bir serbest bırakma yok.
    // Ancak dinamik olarak ayrılmış hafıza olsaydı burada serbest bırakılırdı.
    memset(manager, 0, sizeof(fe_input_manager_t));
}

// Parmak ID'sine göre bir dokunma parmağı durumunu bulur veya yeni bir yuva atar.
static fe_touch_finger_state_t* fe_get_or_create_touch_finger_state(fe_touch_state_t* touch_state, SDL_FingerID finger_id) {
    for (uint32_t i = 0; i < FE_MAX_TOUCH_FINGERS; ++i) {
        if (touch_state->fingers[i].finger_id == finger_id) {
            return &touch_state->fingers[i];
        }
    }
    // Parmak bulunamadı, yeni bir yuva ara
    for (uint32_t i = 0; i < FE_MAX_TOUCH_FINGERS; ++i) {
        if (touch_state->fingers[i].finger_id == SDL_FINGER_INVALID) { // Boş yuva
            touch_state->fingers[i].finger_id = finger_id;
            touch_state->active_finger_count++;
            return &touch_state->fingers[i];
        }
    }
    FE_LOG_WARN("fe_input_manager: Exceeded max touch fingers (%d).", FE_MAX_TOUCH_FINGERS);
    return NULL; // Yer yok
}

// Parmak ID'sine göre dokunma parmağı durumunu bulur.
static fe_touch_finger_state_t* fe_get_touch_finger_state(fe_touch_state_t* touch_state, SDL_FingerID finger_id) {
    for (uint32_t i = 0; i < FE_MAX_TOUCH_FINGERS; ++i) {
        if (touch_state->fingers[i].finger_id == finger_id) {
            return &touch_state->fingers[i];
        }
    }
    return NULL;
}


void fe_input_manager_process_events(fe_input_manager_t* manager) {
    if (!manager) return;

    // Önceki karedeki durumları kaydet (basılı olanlar için)
    memcpy(manager->keyboard.prev_keys, manager->keyboard.current_keys, sizeof(manager->keyboard.current_keys));
    memcpy(manager->mouse.prev_buttons, manager->mouse.current_buttons, sizeof(manager->mouse.current_buttons));

    // Fare tekerleği ve delta hareketlerini sıfırla (sadece bu kareye özel)
    manager->mouse.delta_position = FE_VEC2_ZERO;
    manager->mouse.wheel_delta_x = 0;
    manager->mouse.wheel_delta_y = 0;

    // Dokunma parmaklarının önceki durumlarını kaydet ve delta'ları sıfırla
    for (int i = 0; i < FE_MAX_TOUCH_FINGERS; ++i) {
        manager->touch.fingers[i].was_down_prev_frame = manager->touch.fingers[i].is_down;
        manager->touch.fingers[i].delta_position = FE_VEC2_ZERO;
        // Eğer parmak bırakıldıysa, yuvasını sıfırla
        if (!manager->touch.fingers[i].is_down && manager->touch.fingers[i].finger_id != SDL_FINGER_INVALID) {
            manager->touch.fingers[i].finger_id = SDL_FINGER_INVALID;
            manager->touch.fingers[i].position = FE_VEC2_ZERO;
            manager->touch.fingers[i].delta_position = FE_VEC2_ZERO;
            manager->touch.fingers[i].was_down_prev_frame = false;
        }
    }
    manager->touch.active_finger_count = 0; // Her kare başında aktif parmak sayısını sıfırla

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                manager->quit_requested = true;
                FE_LOG_INFO("SDL_EVENT_QUIT received.");
                break;

            // --- Klavye Olayları ---
            case SDL_EVENT_KEY_DOWN:
                // Sadece tekrarlayan tuş basışlarını yoksay
                if (event.key.repeat) {
                    break;
                }
                if (event.key.scancode < FE_MAX_KEYBOARD_KEYS) {
                    manager->keyboard.current_keys[event.key.scancode] = true;
                    FE_LOG_DEBUG("Key Down: %s (Scancode: %d)", SDL_GetScancodeName(event.key.scancode), event.key.scancode);
                }
                break;
            case SDL_EVENT_KEY_UP:
                if (event.key.scancode < FE_MAX_KEYBOARD_KEYS) {
                    manager->keyboard.current_keys[event.key.scancode] = false;
                    FE_LOG_DEBUG("Key Up: %s (Scancode: %d)", SDL_GetScancodeName(event.key.scancode), event.key.scancode);
                }
                break;

            // --- Fare Olayları ---
            case SDL_EVENT_MOUSE_MOTION: {
                manager->mouse.delta_position.x = (float)event.motion.xrel;
                manager->mouse.delta_position.y = (float)event.motion.yrel;
                manager->mouse.position.x = (float)event.motion.x;
                manager->mouse.position.y = (float)event.motion.y;
                FE_LOG_DEBUG("Mouse Motion: X: %.0f, Y: %.0f, DeltaX: %.0f, DeltaY: %.0f",
                             manager->mouse.position.x, manager->mouse.position.y,
                             manager->mouse.delta_position.x, manager->mouse.delta_position.y);
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.button < FE_MAX_MOUSE_BUTTONS) {
                    manager->mouse.current_buttons[event.button.button] = true;
                    FE_LOG_DEBUG("Mouse Button Down: %d", event.button.button);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.button < FE_MAX_MOUSE_BUTTONS) {
                    manager->mouse.current_buttons[event.button.button] = false;
                    FE_LOG_DEBUG("Mouse Button Up: %d", event.button.button);
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                manager->mouse.wheel_delta_x = event.wheel.x;
                manager->mouse.wheel_delta_y = event.wheel.y;
                FE_LOG_DEBUG("Mouse Wheel: X: %d, Y: %d", event.wheel.x, event.wheel.y);
                break;

            // --- Dokunmatik Ekran Olayları ---
            case SDL_EVENT_FINGER_DOWN: {
                fe_touch_finger_state_t* finger = fe_get_or_create_touch_finger_state(&manager->touch, event.tfinger.fingerID);
                if (finger) {
                    // Dokunmatik olaylarda pozisyonlar genellikle normalize edilmiştir [0,1].
                    // Ekran genişliği/yüksekliği ile çarparak piksel koordinatlarına çevirmek gerekebilir.
                    // SDL3 docs'a göre .x ve .y zaten window koordinatlarında.
                    // fe_vec2_t prev_pos = finger->position; // Delta hesaplamak için önceki pozisyon
                    finger->position.x = event.tfinger.x;
                    finger->position.y = event.tfinger.y;
                    // finger->delta_position.x = finger->position.x - prev_pos.x;
                    // finger->delta_position.y = finger->position.y - prev_pos.y;
                    finger->is_down = true;
                    FE_LOG_DEBUG("Finger Down: ID %lld at (%.2f, %.2f)",
                                 (long long)event.tfinger.fingerID, finger->position.x, finger->position.y);
                }
                break;
            }
            case SDL_EVENT_FINGER_UP: {
                fe_touch_finger_state_t* finger = fe_get_touch_finger_state(&manager->touch, event.tfinger.fingerID);
                if (finger) {
                    finger->is_down = false; // İşaretle, bir sonraki karede yuva boşaltılacak
                    FE_LOG_DEBUG("Finger Up: ID %lld", (long long)event.tfinger.fingerID);
                }
                break;
            }
            case SDL_EVENT_FINGER_MOTION: {
                fe_touch_finger_state_t* finger = fe_get_touch_finger_state(&manager->touch, event.tfinger.fingerID);
                if (finger) {
                    fe_vec2_t prev_pos = finger->position;
                    finger->position.x = event.tfinger.x;
                    finger->position.y = event.tfinger.y;
                    finger->delta_position.x = finger->position.x - prev_pos.x;
                    finger->delta_position.y = finger->position.y - prev_pos.y;
                    FE_LOG_DEBUG("Finger Motion: ID %lld at (%.2f, %.2f), Delta (%.2f, %.2f)",
                                 (long long)event.tfinger.fingerID, finger->position.x, finger->position.y,
                                 finger->delta_position.x, finger->delta_position.y);
                }
                break;
            }
            // Diğer SDL olayları (örn. joystick, gamepad, pencere olayları) buraya eklenebilir
            default:
                break;
        }
    }
    // Her karede aktif parmak sayısını yeniden say
    manager->touch.active_finger_count = 0;
    for (int i = 0; i < FE_MAX_TOUCH_FINGERS; ++i) {
        if (manager->touch.fingers[i].is_down) {
            manager->touch.active_finger_count++;
        }
    }
}

// --- Klavye Sorgu Fonksiyonları ---

bool fe_input_is_key_down(const fe_input_manager_t* manager, SDL_Scancode scancode) {
    if (!manager || scancode >= FE_MAX_KEYBOARD_KEYS) return false;
    return manager->keyboard.current_keys[scancode];
}

bool fe_input_is_key_pressed(const fe_input_manager_t* manager, SDL_Scancode scancode) {
    if (!manager || scancode >= FE_MAX_KEYBOARD_KEYS) return false;
    return manager->keyboard.current_keys[scancode] && !manager->keyboard.prev_keys[scancode];
}

bool fe_input_is_key_released(const fe_input_manager_t* manager, SDL_Scancode scancode) {
    if (!manager || scancode >= FE_MAX_KEYBOARD_KEYS) return false;
    return !manager->keyboard.current_keys[scancode] && manager->keyboard.prev_keys[scancode];
}

// --- Fare Sorgu Fonksiyonları ---

bool fe_input_is_mouse_button_down(const fe_input_manager_t* manager, uint8_t button) {
    if (!manager || button >= FE_MAX_MOUSE_BUTTONS) return false;
    return manager->mouse.current_buttons[button];
}

bool fe_input_is_mouse_button_pressed(const fe_input_manager_t* manager, uint8_t button) {
    if (!manager || button >= FE_MAX_MOUSE_BUTTONS) return false;
    return manager->mouse.current_buttons[button] && !manager->mouse.prev_buttons[button];
}

bool fe_input_is_mouse_button_released(const fe_input_manager_t* manager, uint8_t button) {
    if (!manager || button >= FE_MAX_MOUSE_BUTTONS) return false;
    return !manager->mouse.current_buttons[button] && manager->mouse.prev_buttons[button];
}

fe_vec2_t fe_input_get_mouse_position(const fe_input_manager_t* manager) {
    if (!manager) return FE_VEC2_ZERO;
    return manager->mouse.position;
}

fe_vec2_t fe_input_get_mouse_delta(const fe_input_manager_t* manager) {
    if (!manager) return FE_VEC2_ZERO;
    return manager->mouse.delta_position;
}

int32_t fe_input_get_mouse_wheel_delta_y(const fe_input_manager_t* manager) {
    if (!manager) return 0;
    return manager->mouse.wheel_delta_y;
}

int32_t fe_input_get_mouse_wheel_delta_x(const fe_input_manager_t* manager) {
    if (!manager) return 0;
    return manager->mouse.wheel_delta_x;
}

// --- Dokunmatik Sorgu Fonksiyonları ---

const fe_touch_finger_state_t* fe_input_get_touch_finger_state(const fe_input_manager_t* manager, uint32_t index) {
    if (!manager || index >= FE_MAX_TOUCH_FINGERS) {
        FE_LOG_WARN("fe_input_get_touch_finger_state: Invalid finger index %u.", index);
        return NULL;
    }
    // Sadece aktif parmakları döndürmek için ekstra kontrol eklenebilir.
    // Ancak bu fonksiyon genellikle döngü içinde tüm FE_MAX_TOUCH_FINGERS üzerinde çağrılır.
    if (manager->touch.fingers[index].finger_id != SDL_FINGER_INVALID) {
        return &manager->touch.fingers[index];
    }
    return NULL;
}


// --- Genel Sorgu Fonksiyonları ---

bool fe_input_should_quit(const fe_input_manager_t* manager) {
    if (!manager) return false;
    return manager->quit_requested;
}
