#include "editor/fe_level_editor.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h"
#include "core/scene/fe_scene.h" // fe_scene_t ve fe_game_object_t'nin gerçek tanımlamaları için
#include "core/scene/fe_game_object.h" // fe_game_object_t için (eğer ayrı bir başlık ise)
#include "platform/fe_platform_file.h" // fe_platform_file_read_text, fe_platform_file_write_text için (Soyutlanmış dosya I/O)

// ImGui entegrasyonu için
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_opengl3.h" // veya diğer backend'ler (Vulkan, DirectX)

// --- Yardımcı Fonksiyonlar ---
// ImGui için basit bir dosya seçme iletişim kutusu simülasyonu
// Gerçekte native dosya iletişim kutuları kullanılmalıdır.
static bool fe_editor_show_file_dialog(const char* title, char* buffer, size_t buffer_size, bool save_mode) {
    ImGui::OpenPopup(title);
    bool result = false;
    if (ImGui::BeginPopupModal(title, NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter file path:");
        ImGui::InputText("##filepath", buffer, buffer_size);

        if (ImGui::Button("OK")) {
            result = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return result;
}

// --- Fonksiyon Implementasyonları ---

bool fe_level_editor_init(fe_level_editor_t* editor, fe_scene_t* initial_scene) {
    if (!editor) {
        FE_LOG_FATAL("fe_level_editor_init: Editor pointer is NULL.");
        return false;
    }

    memset(editor, 0, sizeof(fe_level_editor_t));

    editor->active_scene = initial_scene;
    editor->is_open = false; // Başlangıçta GUI kapalı olsun
    editor->show_demo_window = false;

    // Varsayılan ayarlar
    editor->settings.grid_snap_size = 1.0f;
    editor->settings.rotation_snap_angle = 15.0f;
    editor->settings.show_grid = true;
    editor->settings.show_colliders = false;

    // ImGui'nin zaten başlatıldığı varsayılıyor (fe_renderer veya ana uygulama tarafından)
    // Eğer burada başlatılacaksa, ImGui::CreateContext(), ImGui_ImplSDL3_Init(), ImGui_ImplOpenGL3_Init() çağrıları yapılmalı.

    FE_LOG_INFO("FE Level Editor initialized.");
    return true;
}

void fe_level_editor_shutdown(fe_level_editor_t* editor) {
    if (!editor) {
        return;
    }
    // Seçili nesneyi sıfırla
    editor->selected_object = NULL;

    // Diğer kaynakları serbest bırak (eğer editöre özel tahsis edilmişse)
    // Örneğin, editor_camera fe_memory_manager_destroy_camera(editor->editor_camera);

    // ImGui'nin burada kapatılmadığı varsayılıyor (ana uygulama veya renderer kapatır)
    // Eğer burada kapatılacaksa, ImGui_ImplOpenGL3_Shutdown(), ImGui_ImplSDL3_Shutdown(), ImGui::DestroyContext() çağrıları yapılmalı.

    FE_LOG_INFO("FE Level Editor shut down.");
}

void fe_level_editor_update(fe_level_editor_t* editor, float delta_time, fe_input_manager_t* input_manager) {
    if (!editor || !editor->active_scene || !input_manager) {
        return;
    }

    // Düzenleyici aktif değilse güncelleme yapma
    if (!editor->is_open) {
        return;
    }

    // Editör kamerası güncellemesi (gerçek implementasyonda kamera modülü kullanılmalı)
    // fe_camera_update(editor->editor_camera, delta_time, input_manager);

    // Basit nesne seçimi (örnek: fare sol tık)
    // Gerçekte 3D uzayda raycasting veya frustum culling ile seçim yapılır.
    if (fe_input_is_mouse_button_pressed(input_manager, FE_MOUSE_BUTTON_LEFT)) {
        // Fare konumu al ve 3D dünyada bir ray oluştur
        // Ray intersect ile nesneleri seç
        // Şimdilik sadece örnek bir nesne seçelim
        if (editor->active_scene && editor->active_scene->game_objects.size > 0) {
            // İlk nesneyi seç veya sıradaki nesneyi seç
            if (!editor->selected_object || editor->selected_object == fe_dynamic_array_get(&editor->active_scene->game_objects, editor->active_scene->game_objects.size - 1)) {
                fe_level_editor_select_object(editor, fe_dynamic_array_get(&editor->active_scene->game_objects, 0));
            } else {
                for (size_t i = 0; i < editor->active_scene->game_objects.size; ++i) {
                    fe_game_object_t* obj = fe_dynamic_array_get(&editor->active_scene->game_objects, i);
                    if (obj == editor->selected_object) {
                        if (i + 1 < editor->active_scene->game_objects.size) {
                            fe_level_editor_select_object(editor, fe_dynamic_array_get(&editor->active_scene->game_objects, i + 1));
                        } else {
                            fe_level_editor_select_object(editor, fe_dynamic_array_get(&editor->active_scene->game_objects, 0)); // Başa dön
                        }
                        break;
                    }
                }
            }
            FE_LOG_INFO("Selected object: %s", editor->selected_object ? editor->selected_object->name : "None");
        }
    }

    // Nesne manipülasyonu (seçili nesneyi taşıma - örnek)
    if (editor->selected_object) {
        if (fe_input_is_key_down(input_manager, FE_KEY_W)) {
            editor->selected_object->transform.position.z -= 5.0f * delta_time;
        }
        if (fe_input_is_key_down(input_manager, FE_KEY_S)) {
            editor->selected_object->transform.position.z += 5.0f * delta_time;
        }
        // Diğer hareketler...
    }
}

void fe_level_editor_draw_gui(fe_level_editor_t* editor) {
    if (!editor || !editor->active_scene) {
        return;
    }

    // Yeni ImGui çerçevesini başlat
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // ImGui Demo Penceresi (Hata ayıklama için kullanışlı)
    if (editor->show_demo_window) {
        ImGui::ShowDemoWindow(&editor->show_demo_window);
    }

    // Ana düzenleyici menü çubuğu
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene")) {
                fe_level_editor_new_scene(editor);
            }
            if (ImGui::MenuItem("Open Scene...")) {
                // Dosya seçici çağrısı
                static char file_path_buffer[256] = "";
                ImGui::PushID("OpenSceneDialog");
                if (fe_editor_show_file_dialog("Open Scene", file_path_buffer, sizeof(file_path_buffer), false)) {
                    fe_level_editor_load_scene(editor, file_path_buffer);
                }
                ImGui::PopID();
            }
            if (ImGui::MenuItem("Save Scene")) {
                static char file_path_buffer[256] = "";
                ImGui::PushID("SaveSceneDialog");
                 if (fe_editor_show_file_dialog("Save Scene", file_path_buffer, sizeof(file_path_buffer), true)) {
                    fe_level_editor_save_scene(editor, file_path_buffer);
                }
                ImGui::PopID();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                // Uygulamayı kapatma sinyali gönder (ana döngüde yakalanmalı)
                FE_LOG_INFO("Editor exit requested.");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo")) { /* TODO: Undo functionality */ }
            if (ImGui::MenuItem("Redo")) { /* TODO: Redo functionality */ }
            ImGui::Separator();
            if (ImGui::MenuItem("Editor Settings")) {
                // Ayarlar penceresini aç
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("ImGui Demo Window", NULL, &editor->show_demo_window);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // Sol panel: Nesne Hiyerarşisi
    ImGui::Begin("Hierarchy");
    if (editor->active_scene) {
        ImGui::Text("Current Scene: %s", editor->active_scene->name ? editor->active_scene->name : "Unnamed Scene");
        ImGui::Separator();
        // Nesne ekle düğmesi
        if (ImGui::Button("Add New Object")) {
            static int object_count = 0;
            char obj_name[64];
            snprintf(obj_name, sizeof(obj_name), "New_Object_%d", object_count++);
            fe_game_object_t* new_obj = fe_level_editor_add_object(editor, obj_name);
            if (new_obj) {
                fe_level_editor_select_object(editor, new_obj);
            }
        }

        ImGui::Spacing();
        ImGui::Text("Objects:");
        ImGui::Separator();
        if (editor->active_scene->game_objects.size == 0) {
            ImGui::Text("No objects in scene.");
        } else {
            for (size_t i = 0; i < editor->active_scene->game_objects.size; ++i) {
                fe_game_object_t* obj = fe_dynamic_array_get(&editor->active_scene->game_objects, i);
                if (!obj) continue;

                ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
                if (obj == editor->selected_object) {
                    node_flags |= ImGuiTreeNodeFlags_Selected;
                }
                
                bool node_open = ImGui::TreeNodeEx((void*)(intptr_t)obj->id, node_flags, "%s", obj->name);
                
                if (ImGui::IsItemClicked()) {
                    fe_level_editor_select_object(editor, obj);
                }

                if (node_open) {
                    // İç içe nesneler veya bileşenler burada listelenebilir
                    ImGui::TextDisabled("  [No children]");
                    ImGui::TreePop();
                }

                // Sağ tıklama menüsü
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Delete Object")) {
                        fe_level_editor_remove_object(editor, obj);
                        if (editor->selected_object == obj) {
                            editor->selected_object = NULL; // Seçimi kaldır
                        }
                    }
                    ImGui::EndPopup();
                }
            }
        }
    } else {
        ImGui::Text("No active scene.");
    }
    ImGui::End(); // Hierarchy

    // Sağ panel: Seçili Nesne Özellikleri (Inspector)
    ImGui::Begin("Inspector");
    if (editor->selected_object) {
        ImGui::Text("Selected Object: %s (ID: %llu)", editor->selected_object->name, editor->selected_object->id);
        ImGui::Separator();

        // Dönüşüm (Transform) bileşeni
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Position", &editor->selected_object->transform.position.x, 0.1f);
            // Dönme için Euler açılarını kullanmak genellikle daha basittir.
            // Quaternion'ları ImGui ile manipüle etmek biraz daha karmaşıktır.
            ImGui::DragFloat3("Rotation (Euler)", &editor->selected_object->transform.rotation_euler.x, 0.1f, -360.0f, 360.0f);
            ImGui::DragFloat3("Scale", &editor->selected_object->transform.scale.x, 0.1f, 0.01f);
        }

        // Diğer bileşenler burada listelenebilir ve düzenlenebilir (ör: Mesh, Material, Light, Script vb.)
        // Örneğin:
        // if (editor->selected_object->mesh_component) {
        //     if (ImGui::CollapsingHeader("Mesh Component")) {
        //         ImGui::Text("Mesh: %s", editor->selected_object->mesh_component->mesh_name);
        //         // ... mesh özelliklerini düzenleme ...
        //     }
        // }

    } else {
        ImGui::Text("No object selected.");
    }
    ImGui::End(); // Inspector

    // Alt panel: Konsol/Log penceresi (fe_logger ile entegre edilebilir)
    ImGui::Begin("Console");
    ImGui::Text("Logs would go here...");
    // Gerçekte, fe_logger'ın loglarını buraya yönlendirebilirsiniz.
    ImGui::End(); // Console


    // Render ImGui çizimleri
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool fe_level_editor_load_scene(fe_level_editor_t* editor, const char* file_path) {
    if (!editor || !file_path || strlen(file_path) == 0) {
        FE_LOG_ERROR("fe_level_editor_load_scene: Invalid parameters.");
        return false;
    }

    // Mevcut sahneyi temizle (kaydedilmemiş değişiklikleri sorulmalı)
    if (editor->active_scene) {
        FE_LOG_INFO("Unloading current scene before loading new one...");
        fe_scene_shutdown(editor->active_scene); // Sahneyi kapat
        FE_FREE(editor->active_scene, FE_MEM_TYPE_SCENE); // Sahne objesini serbest bırak
        editor->active_scene = NULL;
        editor->selected_object = NULL;
    }

    // Yeni bir sahne oluştur
    fe_scene_t* new_scene = FE_MALLOC(sizeof(fe_scene_t), FE_MEM_TYPE_SCENE);
    if (!new_scene || !fe_scene_init(new_scene, "Loaded Scene")) {
        FE_LOG_ERROR("Failed to create new scene for loading.");
        FE_FREE(new_scene, FE_MEM_TYPE_SCENE);
        return false;
    }

    // Dosyadan sahne verisini oku (örneğin JSON olarak)
    char* scene_data_str = fe_platform_file_read_text(file_path);
    if (!scene_data_str) {
        FE_LOG_ERROR("Failed to read scene file: '%s'", file_path);
        fe_scene_shutdown(new_scene);
        FE_FREE(new_scene, FE_MEM_TYPE_SCENE);
        return false;
    }

    // TODO: Burada JSON/XML/Binary parser ile sahne verisini ayrıştır ve new_scene'i doldur.
    // Örneğin: fe_scene_deserialize(new_scene, scene_data_str);

    FE_LOG_INFO("Scene loaded from '%s' (dummy load).", file_path);
    // Dummy obje ekleyelim test için
    fe_game_object_t* dummy_obj = fe_scene_add_game_object(new_scene, "LoadedObject_1");
    if(dummy_obj) {
        dummy_obj->transform.position.y = 10.0f;
    }

    editor->active_scene = new_scene;
    FE_FREE(scene_data_str, FE_MEM_TYPE_STRING); // Okunan string'i serbest bırak
    return true;
}

bool fe_level_editor_save_scene(fe_level_editor_t* editor, const char* file_path) {
    if (!editor || !editor->active_scene || !file_path || strlen(file_path) == 0) {
        FE_LOG_ERROR("fe_level_editor_save_scene: Invalid parameters.");
        return false;
    }

    // TODO: Sahne verisini JSON/XML/Binary formatına dönüştür.
    // Örneğin: char* scene_data_str = fe_scene_serialize(editor->active_scene);
    const char* dummy_scene_data = "{ \"scene\": { \"name\": \"My Saved Scene\", \"objects\": [] } }"; // Dummy data
    
    if (fe_platform_file_write_text(file_path, dummy_scene_data)) {
        FE_LOG_INFO("Scene saved to '%s' (dummy save).", file_path);
        // FE_FREE(scene_data_str, FE_MEM_TYPE_STRING); // Serileştirilen string'i serbest bırak
        return true;
    } else {
        FE_LOG_ERROR("Failed to write scene file: '%s'", file_path);
        // FE_FREE(scene_data_str, FE_MEM_TYPE_STRING);
        return false;
    }
}

bool fe_level_editor_new_scene(fe_level_editor_t* editor) {
    if (!editor) {
        FE_LOG_ERROR("fe_level_editor_new_scene: Editor pointer is NULL.");
        return false;
    }

    // Mevcut sahneyi temizle (kaydedilmemiş değişiklikleri sorulmalı)
    if (editor->active_scene) {
        FE_LOG_INFO("Unloading current scene before creating new one...");
        fe_scene_shutdown(editor->active_scene); // Sahneyi kapat
        FE_FREE(editor->active_scene, FE_MEM_TYPE_SCENE); // Sahne objesini serbest bırak
        editor->active_scene = NULL;
        editor->selected_object = NULL;
    }

    // Yeni boş bir sahne oluştur
    fe_scene_t* new_scene = FE_MALLOC(sizeof(fe_scene_t), FE_MEM_TYPE_SCENE);
    if (!new_scene || !fe_scene_init(new_scene, "New Scene")) {
        FE_LOG_ERROR("Failed to create new scene.");
        FE_FREE(new_scene, FE_MEM_TYPE_SCENE);
        return false;
    }

    editor->active_scene = new_scene;
    FE_LOG_INFO("New empty scene created.");
    return true;
}

fe_game_object_t* fe_level_editor_add_object(fe_level_editor_t* editor, const char* object_name) {
    if (!editor || !editor->active_scene || !object_name) {
        FE_LOG_ERROR("fe_level_editor_add_object: Invalid parameters.");
        return NULL;
    }

    fe_game_object_t* new_obj = fe_scene_add_game_object(editor->active_scene, object_name);
    if (new_obj) {
        FE_LOG_INFO("Added new object: %s (ID: %llu)", new_obj->name, new_obj->id);
    } else {
        FE_LOG_ERROR("Failed to add new object.");
    }
    return new_obj;
}

bool fe_level_editor_remove_object(fe_level_editor_t* editor, fe_game_object_t* object_to_remove) {
    if (!editor || !editor->active_scene || !object_to_remove) {
        FE_LOG_ERROR("fe_level_editor_remove_object: Invalid parameters.");
        return false;
    }

    if (editor->selected_object == object_to_remove) {
        editor->selected_object = NULL; // Seçimi kaldır
    }

    bool result = fe_scene_remove_game_object(editor->active_scene, object_to_remove);
    if (result) {
        FE_LOG_INFO("Removed object: %s (ID: %llu)", object_to_remove->name, object_to_remove->id);
    } else {
        FE_LOG_ERROR("Failed to remove object: %s", object_to_remove->name);
    }
    return result;
}

void fe_level_editor_select_object(fe_level_editor_t* editor, fe_game_object_t* object_to_select) {
    if (!editor) {
        return;
    }
    if (editor->selected_object == object_to_select) {
        return; // Zaten seçili
    }
    editor->selected_object = object_to_select;
    if (object_to_select) {
        FE_LOG_DEBUG("Object selected: %s (ID: %llu)", object_to_select->name, object_to_select->id);
    } else {
        FE_LOG_DEBUG("Object selection cleared.");
    }
}
