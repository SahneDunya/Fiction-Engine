#ifndef FE_LEVEL_EDITOR_H
#define FE_LEVEL_EDITOR_H

#include "core/utils/fe_types.h"
#include "core/containers/fe_dynamic_array.h" // Seviye nesnelerini tutmak için

// İleri bildirimler (tam tanımlamalara gerek yoksa)
typedef struct fe_game_object fe_game_object_t;
typedef struct fe_scene fe_scene_t;
typedef struct fe_renderer fe_renderer_t;
typedef struct fe_input_manager fe_input_manager_t;

// --- Seviye Düzenleyici Ayarları ---
typedef struct fe_editor_settings {
    float grid_snap_size;    // Izgara yakalama boyutu
    float rotation_snap_angle; // Döndürme yakalama açısı (derece)
    bool  show_grid;         // Izgarayı göster
    bool  show_colliders;    // Çarpışmaları göster
} fe_editor_settings_t;

// --- Seviye Düzenleyici Durumu ---
typedef struct fe_level_editor {
    // Düzenleyicinin genel ayarları
    fe_editor_settings_t settings;

    // Düzenleyicinin etkileşimde bulunduğu oyun dünyası/sahne
    fe_scene_t* active_scene;

    // Seçili nesne(ler)
    fe_game_object_t* selected_object; // Şimdilik tekil seçim

    // Düzenleyiciye özel kamera
    // fe_camera_t *editor_camera; // Motorun kendi kamera modülü varsa

    // GUI durumu (ImGui gibi kütüphaneden gelen durumlar)
    bool is_open; // Düzenleyici UI'si açık mı?
    bool show_demo_window; // ImGui demo penceresi açık mı?

    // Diğer editör bileşenleri veya araçları için yer tutucular
    // fe_asset_browser_t* asset_browser;
    // fe_property_panel_t* property_panel;

} fe_level_editor_t;

// --- Fonksiyon Deklarasyonları ---

/**
 * @brief Seviye düzenleyiciyi başlatır.
 * @param editor fe_level_editor_t yapısının işaretçisi.
 * @param initial_scene Düzenlenecek başlangıç sahnesi.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_level_editor_init(fe_level_editor_t* editor, fe_scene_t* initial_scene);

/**
 * @brief Seviye düzenleyiciyi kapatır ve kaynakları serbest bırakır.
 * @param editor fe_level_editor_t yapısının işaretçisi.
 */
void fe_level_editor_shutdown(fe_level_editor_t* editor);

/**
 * @brief Seviye düzenleyicinin durumunu günceller (input işleme, nesne manipülasyonu vb.).
 * Bu fonksiyon her kare çağrılmalıdır.
 * @param editor fe_level_editor_t yapısının işaretçisi.
 * @param delta_time Geçen zaman (saniye cinsinden).
 * @param input_manager Giriş yöneticisi.
 */
void fe_level_editor_update(fe_level_editor_t* editor, float delta_time, fe_input_manager_t* input_manager);

/**
 * @brief Seviye düzenleyicinin GUI'sini çizer.
 * Bu fonksiyon her kare çağrılmalıdır. Render loop'unun bir parçasıdır.
 * @param editor fe_level_editor_t yapısının işaretçisi.
 */
void fe_level_editor_draw_gui(fe_level_editor_t* editor);

/**
 * @brief Mevcut sahneyi bir dosyadan yükler.
 * @param editor fe_level_editor_t yapısının işaretçisi.
 * @param file_path Yükleme yolu.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_level_editor_load_scene(fe_level_editor_t* editor, const char* file_path);

/**
 * @brief Mevcut sahneyi bir dosyaya kaydeder.
 * @param editor fe_level_editor_t yapısının işaretçisi.
 * @param file_path Kaydetme yolu.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_level_editor_save_scene(fe_level_editor_t* editor, const char* file_path);

/**
 * @brief Editörde yeni bir boş sahne oluşturur.
 * @param editor fe_level_editor_t yapısının işaretçisi.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_level_editor_new_scene(fe_level_editor_t* editor);

/**
 * @brief Editöre yeni bir oyun nesnesi ekler.
 * @param editor fe_level_editor_t yapısının işaretçisi.
 * @param object_name Yeni nesnenin adı.
 * @return fe_game_object_t* Oluşturulan nesnenin işaretçisi, hata durumunda NULL.
 */
fe_game_object_t* fe_level_editor_add_object(fe_level_editor_t* editor, const char* object_name);

/**
 * @brief Editörden bir oyun nesnesini siler.
 * @param editor fe_level_editor_t yapısının işaretçisi.
 * @param object_to_remove Silinecek nesnenin işaretçisi.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_level_editor_remove_object(fe_level_editor_t* editor, fe_game_object_t* object_to_remove);

/**
 * @brief Editörde bir oyun nesnesini seçer.
 * @param editor fe_level_editor_t yapısının işaretçisi.
 * @param object_to_select Seçilecek nesnenin işaretçisi. NULL verilirse seçimi kaldırır.
 */
void fe_level_editor_select_object(fe_level_editor_t* editor, fe_game_object_t* object_to_select);


#endif // FE_LEVEL_EDITOR_H
