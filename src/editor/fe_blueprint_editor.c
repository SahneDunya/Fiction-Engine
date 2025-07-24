#include "editor/fe_blueprint_editor.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h"
#include "platform/fe_platform_file.h" // Dosya I/O için
#include "core/math/fe_math.h" // fe_vec3_t, fe_vec2_t gibi yapılar ve matematiksel işlemler için (varsayım)

// ImGui entegrasyonu için
#include "imgui.h"
#include "imgui_internal.h" // ImRect, ImDrawList vb. için
#include "backends/imgui_impl_sdl3.h.h"
#include "backends/imgui_impl_opengl3.h.h"

#include <string.h> // strncpy, strcmp, strlen
#include <stdio.h>  // snprintf

// --- Yardımcı Renkler (ImU32 formatında, ARGB) ---
#define COLOR_NODE_HEADER   IM_COL32(50, 50, 50, 255)
#define COLOR_NODE_BODY     IM_COL32(35, 35, 35, 255)
#define COLOR_NODE_BORDER   IM_COL32(70, 70, 70, 255)
#define COLOR_PIN_EXEC      IM_COL32(255, 255, 255, 255) // Beyaz
#define COLOR_PIN_BOOL      IM_COL32(255, 0, 0, 255)     // Kırmızı
#define COLOR_PIN_INT       IM_COL32(0, 0, 255, 255)     // Mavi
#define COLOR_PIN_FLOAT     IM_COL32(0, 255, 0, 255)     // Yeşil
#define COLOR_PIN_STRING    IM_COL32(255, 255, 0, 255)   // Sarı
#define COLOR_PIN_VECTOR3   IM_COL32(0, 255, 255, 255)   // Turkuaz
#define COLOR_PIN_OBJECT    IM_COL32(128, 0, 128, 255)   // Mor
#define COLOR_CONNECTION    IM_COL32(200, 200, 200, 255)
#define COLOR_HIGHLIGHT     IM_COL32(255, 255, 0, 255) // Sarı highlight

// --- Forward Declarations (Statik fonksiyonlar için) ---
static ImU32 fe_bp_editor_get_pin_color(fe_blueprint_pin_type_t type);
static void fe_bp_editor_setup_node_pins(fe_blueprint_node_t* node, fe_blueprint_graph_t* graph);
static fe_blueprint_pin_t* fe_bp_editor_create_pin(fe_blueprint_graph_t* graph, const char* name, fe_blueprint_pin_type_t type, fe_blueprint_pin_direction_t dir, fe_blueprint_node_t* parent_node);
static void fe_bp_editor_destroy_pin(fe_blueprint_graph_t* graph, fe_blueprint_pin_t* pin);
static void fe_bp_editor_destroy_node(fe_blueprint_graph_t* graph, fe_blueprint_node_t* node);
static void fe_bp_editor_destroy_connection(fe_blueprint_graph_t* graph, fe_blueprint_connection_t* connection);

static fe_blueprint_pin_t* fe_bp_editor_find_pin_by_id(fe_blueprint_graph_t* graph, uint64_t pin_id);
static fe_blueprint_node_t* fe_bp_editor_find_node_by_id(fe_blueprint_graph_t* graph, uint64_t node_id);
static bool fe_bp_editor_can_connect_pins(fe_blueprint_pin_t* pin_a, fe_blueprint_pin_t* pin_b);
static void fe_bp_editor_draw_node(fe_blueprint_editor_t* editor, fe_blueprint_node_t* node);
static void fe_bp_editor_draw_connection(fe_blueprint_editor_t* editor, fe_blueprint_connection_t* connection);
static void fe_bp_editor_handle_input(fe_blueprint_editor_t* editor);


// --- Blueprint Graph Yönetimi ---
bool fe_blueprint_graph_init(fe_blueprint_graph_t* graph, const char* name) {
    if (!graph || !name) {
        return false;
    }
    memset(graph, 0, sizeof(fe_blueprint_graph_t));
    strncpy(graph->name, name, sizeof(graph->name) - 1);
    graph->name[sizeof(graph->name) - 1] = '\0';
    graph->next_id = 1; // ID'leri 1'den başlat

    if (!FE_DYNAMIC_ARRAY_INIT(&graph->nodes, 8)) return false;
    if (!FE_DYNAMIC_ARRAY_INIT(&graph->connections, 16)) return false;
    if (!FE_HASH_MAP_INIT(&graph->id_to_node_map, 16)) return false;
    if (!FE_HASH_MAP_INIT(&graph->id_to_pin_map, 32)) return false;

    return true;
}

void fe_blueprint_graph_shutdown(fe_blueprint_graph_t* graph) {
    if (!graph) return;

    // Bağlantıları serbest bırak
    for (size_t i = 0; i < graph->connections.size; ++i) {
        fe_blueprint_connection_t* conn = FE_DYNAMIC_ARRAY_GET(&graph->connections, i);
        FE_FREE(conn, FE_MEM_TYPE_EDITOR);
    }
    FE_DYNAMIC_ARRAY_SHUTDOWN(&graph->connections);

    // Düğümleri ve pinleri serbest bırak
    for (size_t i = 0; i < graph->nodes.size; ++i) {
        fe_blueprint_node_t* node = FE_DYNAMIC_ARRAY_GET(&graph->nodes, i);
        if (node) {
            fe_bp_editor_destroy_node(graph, node); // Bu da pinleri temizler
        }
    }
    FE_DYNAMIC_ARRAY_SHUTDOWN(&graph->nodes);

    FE_HASH_MAP_SHUTDOWN(&graph->id_to_node_map);
    FE_HASH_MAP_SHUTDOWN(&graph->id_to_pin_map);

    FE_LOG_INFO("Blueprint graph '%s' shut down.", graph->name);
}


// --- Blueprint Düzenleyici Fonksiyonları ---

bool fe_blueprint_editor_init(fe_blueprint_editor_t* editor) {
    if (!editor) {
        FE_LOG_FATAL("fe_blueprint_editor_init: Editor pointer is NULL.");
        return false;
    }

    memset(editor, 0, sizeof(fe_blueprint_editor_t));
    editor->is_open = true; // Başlangıçta açık olsun
    editor->canvas_zoom = 1.0f;
    editor->dragged_node_id = 0;
    editor->dragging_pin_id = 0;
    editor->hovered_pin = NULL;
    editor->hovered_node = NULL;
    editor->potential_connection_start_pin = NULL;
    editor->potential_connection_end_pin = NULL;

    // Varsayılan olarak boş bir blueprint grafiği oluştur
    editor->current_graph = FE_MALLOC(sizeof(fe_blueprint_graph_t), FE_MEM_TYPE_EDITOR);
    if (!editor->current_graph || !fe_blueprint_graph_init(editor->current_graph, "New Blueprint")) {
        FE_LOG_ERROR("Failed to create initial blueprint graph.");
        FE_FREE(editor->current_graph, FE_MEM_TYPE_EDITOR);
        return false;
    }

    FE_LOG_INFO("FE Blueprint Editor initialized.");
    return true;
}

void fe_blueprint_editor_shutdown(fe_blueprint_editor_t* editor) {
    if (!editor) {
        return;
    }

    if (editor->current_graph) {
        fe_blueprint_graph_shutdown(editor->current_graph);
        FE_FREE(editor->current_graph, FE_MEM_TYPE_EDITOR);
        editor->current_graph = NULL;
    }

    editor->hovered_pin = NULL;
    editor->hovered_node = NULL;
    editor->potential_connection_start_pin = NULL;
    editor->potential_connection_end_pin = NULL;

    FE_LOG_INFO("FE Blueprint Editor shut down.");
}

bool fe_blueprint_editor_new_graph(fe_blueprint_editor_t* editor, const char* graph_name) {
    if (!editor || !graph_name) {
        FE_LOG_ERROR("fe_blueprint_editor_new_graph: Invalid parameters.");
        return false;
    }

    // Mevcut grafiği kapat (kaydedilmemiş değişiklikler sorulmalı)
    if (editor->current_graph) {
        fe_blueprint_graph_shutdown(editor->current_graph);
        FE_FREE(editor->current_graph, FE_MEM_TYPE_EDITOR);
    }

    editor->current_graph = FE_MALLOC(sizeof(fe_blueprint_graph_t), FE_MEM_TYPE_EDITOR);
    if (!editor->current_graph || !fe_blueprint_graph_init(editor->current_graph, graph_name)) {
        FE_LOG_ERROR("Failed to create new blueprint graph.");
        FE_FREE(editor->current_graph, FE_MEM_TYPE_EDITOR);
        editor->current_graph = NULL;
        return false;
    }

    FE_LOG_INFO("New blueprint graph '%s' created.", graph_name);
    return true;
}

bool fe_blueprint_editor_load_graph(fe_blueprint_editor_t* editor, const char* file_path) {
    if (!editor || !file_path || strlen(file_path) == 0) {
        FE_LOG_ERROR("fe_blueprint_editor_load_graph: Invalid parameters.");
        return false;
    }

    FE_LOG_INFO("Attempting to load blueprint graph from: %s", file_path);

    // Mevcut grafiği kapat
    if (editor->current_graph) {
        fe_blueprint_graph_shutdown(editor->current_graph);
        FE_FREE(editor->current_graph, FE_MEM_TYPE_EDITOR);
        editor->current_graph = NULL;
    }

    fe_blueprint_graph_t* new_graph = FE_MALLOC(sizeof(fe_blueprint_graph_t), FE_MEM_TYPE_EDITOR);
    if (!new_graph || !fe_blueprint_graph_init(new_graph, "Loaded Blueprint")) {
        FE_LOG_ERROR("Failed to initialize new graph for loading.");
        FE_FREE(new_graph, FE_MEM_TYPE_EDITOR);
        return false;
    }

    char* json_data = fe_platform_file_read_text(file_path);
    if (!json_data) {
        FE_LOG_ERROR("Failed to read blueprint file: %s", file_path);
        fe_blueprint_graph_shutdown(new_graph);
        FE_FREE(new_graph, FE_MEM_TYPE_EDITOR);
        return false;
    }

    // TODO: JSON/XML parsing logic here to populate new_graph
    // This would be complex and involves deserializing nodes, pins, connections,
    // and their properties.
    FE_LOG_WARN("Blueprint graph loading from file is a TODO. Using dummy data.");

    // Dummy data for testing
    fe_blueprint_node_t* start_node = fe_blueprint_editor_add_node(editor, FE_BLUEPRINT_NODE_TYPE_EVENT_START, "OnBeginPlay", ImVec2(100, 100));
    fe_blueprint_node_t* print_node = fe_blueprint_editor_add_node(editor, FE_BLUEPRINT_NODE_TYPE_PRINT_STRING, "Print Debug String", ImVec2(300, 100));
    
    if (start_node && print_node) {
        // Find execution pins
        fe_blueprint_pin_t* start_exec_out = NULL;
        for (size_t i = 0; i < start_node->output_pins.size; ++i) {
            fe_blueprint_pin_t* p = FE_DYNAMIC_ARRAY_GET(&start_node->output_pins, i);
            if (p->type == FE_BLUEPRINT_PIN_TYPE_EXECUTION) {
                start_exec_out = p;
                break;
            }
        }
        fe_blueprint_pin_t* print_exec_in = NULL;
        for (size_t i = 0; i < print_node->input_pins.size; ++i) {
            fe_blueprint_pin_t* p = FE_DYNAMIC_ARRAY_GET(&print_node->input_pins, i);
            if (p->type == FE_BLUEPRINT_PIN_TYPE_EXECUTION) {
                print_exec_in = p;
                break;
            }
        }

        if (start_exec_out && print_exec_in) {
            fe_blueprint_editor_create_connection(editor, start_exec_out->id, print_exec_in->id);
        }
    }


    FE_FREE(json_data, FE_MEM_TYPE_STRING);
    editor->current_graph = new_graph;
    FE_LOG_INFO("Blueprint graph '%s' loaded (dummy).", new_graph->name);
    return true;
}

bool fe_blueprint_editor_save_graph(fe_blueprint_editor_t* editor, const char* file_path) {
    if (!editor || !editor->current_graph || !file_path || strlen(file_path) == 0) {
        FE_LOG_ERROR("fe_blueprint_editor_save_graph: Invalid parameters.");
        return false;
    }

    FE_LOG_INFO("Attempting to save blueprint graph to: %s", file_path);

    // TODO: JSON/XML serialization logic here
    // This would convert the current_graph structure (nodes, pins, connections,
    // and their properties) into a string format.
    const char* dummy_json_output = "{\n  \"blueprint_name\": \"MyDummyBlueprint\",\n  \"nodes\": [\n    { \"id\": 1, \"type\": \"Event_Start\", \"pos\": [100, 100] },\n    { \"id\": 2, \"type\": \"Print_String\", \"pos\": [300, 100] }\n  ],\n  \"connections\": [\n    { \"start_pin\": \"1_out_exec\", \"end_pin\": \"2_in_exec\" }\n  ]\n}";

    if (!fe_platform_file_write_text(file_path, dummy_json_output)) {
        FE_LOG_ERROR("Failed to write blueprint file: %s", file_path);
        return false;
    }

    FE_LOG_INFO("Blueprint graph '%s' saved (dummy).", editor->current_graph->name);
    return true;
}

fe_blueprint_node_t* fe_blueprint_editor_add_node(fe_blueprint_editor_t* editor,
                                                  fe_blueprint_node_type_t node_type,
                                                  const char* name,
                                                  ImGui::ImVec2 initial_pos) {
    if (!editor || !editor->current_graph || node_type == FE_BLUEPRINT_NODE_TYPE_UNKNOWN) {
        FE_LOG_ERROR("fe_blueprint_editor_add_node: Invalid parameters.");
        return NULL;
    }

    fe_blueprint_node_t* new_node = FE_MALLOC(sizeof(fe_blueprint_node_t), FE_MEM_TYPE_EDITOR);
    if (!new_node) {
        FE_LOG_ERROR("Failed to allocate memory for new blueprint node.");
        return NULL;
    }
    memset(new_node, 0, sizeof(fe_blueprint_node_t));

    new_node->id = editor->current_graph->next_id++;
    new_node->type = node_type;
    new_node->pos = initial_pos;
    new_node->size = ImVec2(150, 60); // Varsayılan boyut

    if (name) {
        // String'i kopyala ve yönetimini yap
        size_t name_len = strlen(name);
        new_node->name = FE_MALLOC(name_len + 1, FE_MEM_TYPE_EDITOR);
        if (new_node->name) {
            strcpy((char*)new_node->name, name);
        } else {
            FE_LOG_ERROR("Failed to allocate memory for node name.");
            // Hata durumunda bellek sızıntısı olmaması için temizle
            FE_FREE(new_node, FE_MEM_TYPE_EDITOR);
            return NULL;
        }
    } else {
        // Varsayılan isim ata
        char default_name[64];
        snprintf(default_name, sizeof(default_name), "Node_%llu", new_node->id);
        size_t name_len = strlen(default_name);
        new_node->name = FE_MALLOC(name_len + 1, FE_MEM_TYPE_EDITOR);
        if (new_node->name) {
            strcpy((char*)new_node->name, default_name);
        } else {
            FE_FREE(new_node, FE_MEM_TYPE_EDITOR);
            return NULL;
        }
    }

    if (!FE_DYNAMIC_ARRAY_INIT(&new_node->input_pins, 2) || !FE_DYNAMIC_ARRAY_INIT(&new_node->output_pins, 2)) {
        FE_LOG_ERROR("Failed to init pin arrays for new node.");
        FE_FREE((void*)new_node->name, FE_MEM_TYPE_EDITOR);
        FE_FREE(new_node, FE_MEM_TYPE_EDITOR);
        return NULL;
    }

    // Node tipine göre pinleri setup et
    fe_bp_editor_setup_node_pins(new_node, editor->current_graph);

    FE_DYNAMIC_ARRAY_ADD(&editor->current_graph->nodes, new_node);
    FE_HASH_MAP_INSERT(&editor->current_graph->id_to_node_map, new_node->id, new_node);

    FE_LOG_INFO("Added new blueprint node: %s (ID: %llu, Type: %d) at (%.1f, %.1f)",
                new_node->name, new_node->id, new_node->type, new_node->pos.x, new_node->pos.y);
    return new_node;
}

bool fe_blueprint_editor_remove_node(fe_blueprint_editor_t* editor, fe_blueprint_node_t* node_to_remove) {
    if (!editor || !editor->current_graph || !node_to_remove) {
        FE_LOG_ERROR("fe_blueprint_editor_remove_node: Invalid parameters.");
        return false;
    }

    // Önce bu düğüme bağlı tüm bağlantıları kaldır
    for (int i = (int)editor->current_graph->connections.size - 1; i >= 0; --i) {
        fe_blueprint_connection_t* conn = FE_DYNAMIC_ARRAY_GET(&editor->current_graph->connections, i);
        if (conn->start_pin->parent_node == node_to_remove || conn->end_pin->parent_node == node_to_remove) {
            fe_blueprint_editor_remove_connection(editor, conn);
        }
    }

    // Düğümü dinamik diziden ve hash map'ten kaldır
    bool removed_from_array = false;
    for (size_t i = 0; i < editor->current_graph->nodes.size; ++i) {
        if (FE_DYNAMIC_ARRAY_GET(&editor->current_graph->nodes, i) == node_to_remove) {
            FE_DYNAMIC_ARRAY_REMOVE_AT(&editor->current_graph->nodes, i);
            removed_from_array = true;
            break;
        }
    }

    FE_HASH_MAP_REMOVE(&editor->current_graph->id_to_node_map, node_to_remove->id);

    if (removed_from_array) {
        fe_bp_editor_destroy_node(editor->current_graph, node_to_remove);
        FE_LOG_INFO("Removed blueprint node: %s (ID: %llu)", node_to_remove->name, node_to_remove->id);
        return true;
    }

    FE_LOG_ERROR("Failed to find node to remove: %s (ID: %llu)", node_to_remove->name, node_to_remove->id);
    return false;
}

bool fe_blueprint_editor_create_connection(fe_blueprint_editor_t* editor, uint64_t output_pin_id, uint64_t input_pin_id) {
    if (!editor || !editor->current_graph) {
        FE_LOG_ERROR("fe_blueprint_editor_create_connection: Invalid editor or graph.");
        return false;
    }

    fe_blueprint_pin_t* out_pin = fe_bp_editor_find_pin_by_id(editor->current_graph, output_pin_id);
    fe_blueprint_pin_t* in_pin = fe_bp_editor_find_pin_by_id(editor->current_graph, input_pin_id);

    if (!out_pin || !in_pin) {
        FE_LOG_ERROR("fe_blueprint_editor_create_connection: One or both pins not found. Out: %llu, In: %llu", output_pin_id, input_pin_id);
        return false;
    }
    if (out_pin->direction != FE_BLUEPRINT_PIN_DIR_OUTPUT || in_pin->direction != FE_BLUEPRINT_PIN_DIR_INPUT) {
        FE_LOG_ERROR("fe_blueprint_editor_create_connection: Invalid pin directions. Must be Output to Input.");
        return false;
    }
    if (!fe_bp_editor_can_connect_pins(out_pin, in_pin)) {
        FE_LOG_ERROR("fe_blueprint_editor_create_connection: Pins are not compatible for connection. %s (%d) -> %s (%d)",
                     out_pin->name, out_pin->type, in_pin->name, in_pin->type);
        return false;
    }

    // Zaten bağlı olup olmadığını kontrol et
    for (size_t i = 0; i < editor->current_graph->connections.size; ++i) {
        fe_blueprint_connection_t* existing_conn = FE_DYNAMIC_ARRAY_GET(&editor->current_graph->connections, i);
        if ((existing_conn->start_pin == out_pin && existing_conn->end_pin == in_pin) ||
            (existing_conn->start_pin == in_pin && existing_conn->end_pin == out_pin)) // Sanırım burada sadece yönlü bağlantı kontrolü olmalı
        {
            FE_LOG_WARN("Connection already exists between pins: %s and %s", out_pin->name, in_pin->name);
            return false;
        }
    }
    
    // Eğer giriş pini bir yürütme pini değilse ve zaten bağlıysa, mevcut bağlantıyı kes
    // Bu, "tek girişli" veri pinleri için yaygın bir davranıştır.
    if (in_pin->type != FE_BLUEPRINT_PIN_TYPE_EXECUTION && in_pin->is_connected) {
        for (int i = (int)editor->current_graph->connections.size - 1; i >= 0; --i) {
            fe_blueprint_connection_t* conn = FE_DYNAMIC_ARRAY_GET(&editor->current_graph->connections, i);
            if (conn->end_pin == in_pin) {
                fe_blueprint_editor_remove_connection(editor, conn);
                FE_LOG_INFO("Removed existing connection to input pin %s before creating new one.", in_pin->name);
                break;
            }
        }
    }


    fe_blueprint_connection_t* new_conn = FE_MALLOC(sizeof(fe_blueprint_connection_t), FE_MEM_TYPE_EDITOR);
    if (!new_conn) {
        FE_LOG_ERROR("Failed to allocate memory for new blueprint connection.");
        return false;
    }
    memset(new_conn, 0, sizeof(fe_blueprint_connection_t));

    new_conn->id = editor->current_graph->next_id++;
    new_conn->start_pin_id = out_pin->id;
    new_conn->end_pin_id = in_pin->id;
    new_conn->start_pin = out_pin;
    new_conn->end_pin = in_pin;

    out_pin->is_connected = true;
    in_pin->is_connected = true;

    FE_DYNAMIC_ARRAY_ADD(&editor->current_graph->connections, new_conn);

    FE_LOG_INFO("Created new connection: %llu -> %llu", out_pin->id, in_pin->id);
    return true;
}

bool fe_blueprint_editor_remove_connection(fe_blueprint_editor_t* editor, fe_blueprint_connection_t* connection_to_remove) {
    if (!editor || !editor->current_graph || !connection_to_remove) {
        FE_LOG_ERROR("fe_blueprint_editor_remove_connection: Invalid parameters.");
        return false;
    }

    bool removed_from_array = false;
    for (size_t i = 0; i < editor->current_graph->connections.size; ++i) {
        if (FE_DYNAMIC_ARRAY_GET(&editor->current_graph->connections, i) == connection_to_remove) {
            FE_DYNAMIC_ARRAY_REMOVE_AT(&editor->current_graph->connections, i);
            removed_from_array = true;
            break;
        }
    }

    if (removed_from_array) {
        // Pinlerin bağlantı durumunu güncelle
        connection_to_remove->start_pin->is_connected = false; // Basit bir kontrol, birden fazla bağlantı varsa daha karmaşık olur
        connection_to_remove->end_pin->is_connected = false;

        FE_FREE(connection_to_remove, FE_MEM_TYPE_EDITOR);
        FE_LOG_INFO("Removed connection.");
        return true;
    }

    FE_LOG_ERROR("Failed to find connection to remove.");
    return false;
}


// --- Dahili Yardımcı Fonksiyon Implementasyonları ---

static ImU32 fe_bp_editor_get_pin_color(fe_blueprint_pin_type_t type) {
    switch (type) {
        case FE_BLUEPRINT_PIN_TYPE_EXECUTION: return COLOR_PIN_EXEC;
        case FE_BLUEPRINT_PIN_TYPE_BOOL:      return COLOR_PIN_BOOL;
        case FE_BLUEPRINT_PIN_TYPE_INT:       return COLOR_PIN_INT;
        case FE_BLUEPRINT_PIN_TYPE_FLOAT:     return COLOR_PIN_FLOAT;
        case FE_BLUEPRINT_PIN_TYPE_STRING:    return COLOR_PIN_STRING;
        case FE_BLUEPRINT_PIN_TYPE_VECTOR3:   return COLOR_PIN_VECTOR3;
        case FE_BLUEPRINT_PIN_TYPE_OBJECT:    return COLOR_PIN_OBJECT;
        default:                              return IM_COL32(100, 100, 100, 255); // Varsayılan gri
    }
}

static void fe_bp_editor_setup_node_pins(fe_blueprint_node_t* node, fe_blueprint_graph_t* graph) {
    // Bu fonksiyon her düğüm tipi için belirli pinleri oluşturur.
    // Gerçek bir motor, düğüm tanımlarını (node definitions) bir yerden okurdu.
    switch (node->type) {
        case FE_BLUEPRINT_NODE_TYPE_EVENT_START: {
            fe_bp_editor_create_pin(graph, "Exec", FE_BLUEPRINT_PIN_TYPE_EXECUTION, FE_BLUEPRINT_PIN_DIR_OUTPUT, node);
            break;
        }
        case FE_BLUEPRINT_NODE_TYPE_PRINT_STRING: {
            fe_bp_editor_create_pin(graph, "In", FE_BLUEPRINT_PIN_TYPE_EXECUTION, FE_BLUEPRINT_PIN_DIR_INPUT, node);
            fe_bp_editor_create_pin(graph, "Out", FE_BLUEPRINT_PIN_TYPE_EXECUTION, FE_BLUEPRINT_PIN_DIR_OUTPUT, node);
            fe_blueprint_pin_t* str_pin = fe_bp_editor_create_pin(graph, "String", FE_BLUEPRINT_PIN_TYPE_STRING, FE_BLUEPRINT_PIN_DIR_INPUT, node);
            if (str_pin) {
                // Varsayılan değer ayarla
                str_pin->default_value.s_val = FE_MALLOC(strlen("Hello World!") + 1, FE_MEM_TYPE_EDITOR);
                if (str_pin->default_value.s_val) strcpy(str_pin->default_value.s_val, "Hello World!");
            }
            break;
        }
        case FE_BLUEPRINT_NODE_TYPE_ADD_INT: {
            fe_bp_editor_create_pin(graph, "In A", FE_BLUEPRINT_PIN_TYPE_INT, FE_BLUEPRINT_PIN_DIR_INPUT, node)->default_value.i_val = 0;
            fe_bp_editor_create_pin(graph, "In B", FE_BLUEPRINT_PIN_TYPE_INT, FE_BLUEPRINT_PIN_DIR_INPUT, node)->default_value.i_val = 0;
            fe_bp_editor_create_pin(graph, "Result", FE_BLUEPRINT_PIN_TYPE_INT, FE_BLUEPRINT_PIN_DIR_OUTPUT, node);
            break;
        }
        case FE_BLUEPRINT_NODE_TYPE_IF: {
            fe_bp_editor_create_pin(graph, "In", FE_BLUEPRINT_PIN_TYPE_EXECUTION, FE_BLUEPRINT_PIN_DIR_INPUT, node);
            fe_bp_editor_create_pin(graph, "Condition", FE_BLUEPRINT_PIN_TYPE_BOOL, FE_BLUEPRINT_PIN_DIR_INPUT, node)->default_value.b_val = false;
            fe_bp_editor_create_pin(graph, "True", FE_BLUEPRINT_PIN_TYPE_EXECUTION, FE_BLUEPRINT_PIN_DIR_OUTPUT, node);
            fe_bp_editor_create_pin(graph, "False", FE_BLUEPRINT_PIN_TYPE_EXECUTION, FE_BLUEPRINT_PIN_DIR_OUTPUT, node);
            break;
        }
        case FE_BLUEPRINT_NODE_TYPE_GET_OBJECT_LOCATION: {
            fe_bp_editor_create_pin(graph, "Object", FE_BLUEPRINT_PIN_TYPE_OBJECT, FE_BLUEPRINT_PIN_DIR_INPUT, node);
            fe_bp_editor_create_pin(graph, "Location", FE_BLUEPRINT_PIN_TYPE_VECTOR3, FE_BLUEPRINT_PIN_DIR_OUTPUT, node);
            break;
        }
        case FE_BLUEPRINT_NODE_TYPE_SET_OBJECT_LOCATION: {
            fe_bp_editor_create_pin(graph, "In", FE_BLUEPRINT_PIN_TYPE_EXECUTION, FE_BLUEPRINT_PIN_DIR_INPUT, node);
            fe_bp_editor_create_pin(graph, "Out", FE_BLUEPRINT_PIN_TYPE_EXECUTION, FE_BLUEPRINT_PIN_DIR_OUTPUT, node);
            fe_bp_editor_create_pin(graph, "Object", FE_BLUEPRINT_PIN_TYPE_OBJECT, FE_BLUEPRINT_PIN_DIR_INPUT, node);
            fe_bp_editor_create_pin(graph, "Location", FE_BLUEPRINT_PIN_TYPE_VECTOR3, FE_BLUEPRINT_PIN_DIR_INPUT, node);
            break;
        }
        default:
            FE_LOG_WARN("Unknown blueprint node type: %d, no pins setup.", node->type);
            break;
    }
}

static fe_blueprint_pin_t* fe_bp_editor_create_pin(fe_blueprint_graph_t* graph, const char* name, fe_blueprint_pin_type_t type, fe_blueprint_pin_direction_t dir, fe_blueprint_node_t* parent_node) {
    fe_blueprint_pin_t* new_pin = FE_MALLOC(sizeof(fe_blueprint_pin_t), FE_MEM_TYPE_EDITOR);
    if (!new_pin) {
        FE_LOG_ERROR("Failed to allocate memory for new blueprint pin.");
        return NULL;
    }
    memset(new_pin, 0, sizeof(fe_blueprint_pin_t));

    new_pin->id = graph->next_id++;
    new_pin->name = name; // Sabit string kullanıyoruz, serileştirme aşamasında bu isimler bir enum'a veya hash'e çevrilir.
    new_pin->type = type;
    new_pin->direction = dir;
    new_pin->parent_node = parent_node;
    new_pin->radius = 5.0f; // Pin çizim yarıçapı
    new_pin->is_connected = false;

    // Pin'in varsayılan değeri başlat
    memset(&new_pin->default_value, 0, sizeof(new_pin->default_value));
    if (new_pin->type == FE_BLUEPRINT_PIN_TYPE_STRING) {
        // Stringler için NULL başlangıç değeri
        new_pin->default_value.s_val = NULL; 
    }

    if (dir == FE_BLUEPRINT_PIN_DIR_INPUT) {
        FE_DYNAMIC_ARRAY_ADD(&parent_node->input_pins, new_pin);
    } else {
        FE_DYNAMIC_ARRAY_ADD(&parent_node->output_pins, new_pin);
    }
    FE_HASH_MAP_INSERT(&graph->id_to_pin_map, new_pin->id, new_pin);

    return new_pin;
}

static void fe_bp_editor_destroy_pin(fe_blueprint_graph_t* graph, fe_blueprint_pin_t* pin) {
    if (!pin) return;
    
    // Bağlı olduğu düğümün pin listesinden kaldır
    if (pin->direction == FE_BLUEPRINT_PIN_DIR_INPUT) {
        for (size_t i = 0; i < pin->parent_node->input_pins.size; ++i) {
            if (FE_DYNAMIC_ARRAY_GET(&pin->parent_node->input_pins, i) == pin) {
                FE_DYNAMIC_ARRAY_REMOVE_AT(&pin->parent_node->input_pins, i);
                break;
            }
        }
    } else {
        for (size_t i = 0; i < pin->parent_node->output_pins.size; ++i) {
            if (FE_DYNAMIC_ARRAY_GET(&pin->parent_node->output_pins, i) == pin) {
                FE_DYNAMIC_ARRAY_REMOVE_AT(&pin->parent_node->output_pins, i);
                break;
            }
        }
    }

    // String default değeri varsa serbest bırak
    if (pin->type == FE_BLUEPRINT_PIN_TYPE_STRING && pin->default_value.s_val) {
        FE_FREE(pin->default_value.s_val, FE_MEM_TYPE_EDITOR);
    }

    FE_HASH_MAP_REMOVE(&graph->id_to_pin_map, pin->id);
    FE_FREE(pin, FE_MEM_TYPE_EDITOR);
}

static void fe_bp_editor_destroy_node(fe_blueprint_graph_t* graph, fe_blueprint_node_t* node) {
    if (!node) return;

    // Pinleri temizle
    for (int i = (int)node->input_pins.size - 1; i >= 0; --i) {
        fe_bp_editor_destroy_pin(graph, FE_DYNAMIC_ARRAY_GET(&node->input_pins, i));
    }
    FE_DYNAMIC_ARRAY_SHUTDOWN(&node->input_pins);

    for (int i = (int)node->output_pins.size - 1; i >= 0; --i) {
        fe_bp_editor_destroy_pin(graph, FE_DYNAMIC_ARRAY_GET(&node->output_pins, i));
    }
    FE_DYNAMIC_ARRAY_SHUTDOWN(&node->output_pins);
    
    // Düğüme özel veri varsa serbest bırak
    // TODO: node->node_data'nın yönetimi tipine göre yapılmalı

    FE_FREE((void*)node->name, FE_MEM_TYPE_EDITOR); // Node adını serbest bırak
    FE_FREE(node, FE_MEM_TYPE_EDITOR);
}

static void fe_bp_editor_destroy_connection(fe_blueprint_graph_t* graph, fe_blueprint_connection_t* connection) {
    if (!connection) return;
    // Bağlantıyı dynamic array'den kaldır
    for (size_t i = 0; i < graph->connections.size; ++i) {
        if (FE_DYNAMIC_ARRAY_GET(&graph->connections, i) == connection) {
            FE_DYNAMIC_ARRAY_REMOVE_AT(&graph->connections, i);
            break;
        }
    }
    FE_FREE(connection, FE_MEM_TYPE_EDITOR);
}


static fe_blueprint_pin_t* fe_bp_editor_find_pin_by_id(fe_blueprint_graph_t* graph, uint64_t pin_id) {
    fe_blueprint_pin_t** found_pin_ptr = FE_HASH_MAP_GET(&graph->id_to_pin_map, pin_id);
    return found_pin_ptr ? *found_pin_ptr : NULL;
}

static fe_blueprint_node_t* fe_bp_editor_find_node_by_id(fe_blueprint_graph_t* graph, uint64_t node_id) {
    fe_blueprint_node_t** found_node_ptr = FE_HASH_MAP_GET(&graph->id_to_node_map, node_id);
    return found_node_ptr ? *found_node_ptr : NULL;
}

static bool fe_bp_editor_can_connect_pins(fe_blueprint_pin_t* pin_a, fe_blueprint_pin_t* pin_b) {
    if (!pin_a || !pin_b || pin_a == pin_b || pin_a->parent_node == pin_b->parent_node) {
        return false; // Aynı pinler veya aynı düğümler arasında bağlantı yok
    }

    // Bir input, bir output olmalı
    bool a_is_output = (pin_a->direction == FE_BLUEPRINT_PIN_DIR_OUTPUT);
    bool b_is_output = (pin_b->direction == FE_BLUEPRINT_PIN_DIR_OUTPUT);

    if ((a_is_output && b_is_output) || (!a_is_output && !b_is_output)) {
        return false; // İkisi de input veya ikisi de output olamaz
    }

    // Yürütme pinleri sadece yürütme pinlerine bağlanabilir
    if (pin_a->type == FE_BLUEPRINT_PIN_TYPE_EXECUTION && pin_b->type == FE_BLUEPRINT_PIN_TYPE_EXECUTION) {
        return true;
    }
    if (pin_a->type == FE_BLUEPRINT_PIN_TYPE_EXECUTION || pin_b->type == FE_BLUEPRINT_PIN_TYPE_EXECUTION) {
        return false; // Birisi yürütme, diğeri veri piniyse olmaz
    }

    // Veri pinleri aynı tipten olmalı (veya uyumlu tipler)
    if (pin_a->type == pin_b->type) {
        return true;
    }

    // TODO: Implicit type conversion rules (e.g., int to float)
    // Şimdilik sadece tam eşleşme
    return false;
}

static void fe_bp_editor_draw_node(fe_blueprint_editor_t* editor, fe_blueprint_node_t* node) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();

    // Canvas ofsetini uygula
    ImVec2 node_pos_on_canvas = ImVec2(node->pos.x * editor->canvas_zoom + editor->canvas_origin.x,
                                       node->pos.y * editor->canvas_zoom + editor->canvas_origin.y);
    ImVec2 node_size_scaled = ImVec2(node->size.x * editor->canvas_zoom, node->size.y * editor->canvas_zoom);

    ImVec2 node_min = node_pos_on_canvas;
    ImVec2 node_max = ImVec2(node_min.x + node_size_scaled.x, node_min.y + node_size_scaled.y);
    ImRect node_rect(node_min, node_max);

    // Düğüm arkaplanı
    draw_list->AddRectFilled(node_min, node_max, COLOR_NODE_BODY, 5.0f);
    draw_list->AddRect(node_min, node_max, COLOR_NODE_BORDER, 5.0f, 0, 2.0f);

    // Düğüm başlığı
    ImVec2 header_min = node_min;
    ImVec2 header_max = ImVec2(node_max.x, node_min.y + 25 * editor->canvas_zoom); // Başlık yüksekliği
    draw_list->AddRectFilled(header_min, header_max, COLOR_NODE_HEADER, 5.0f, ImDrawFlags_RoundCornersTop);
    draw_list->AddText(header_min + ImVec2(5, 5), IM_COL32_WHITE, node->name);

    // Düğüm içeriği (input/output pinleri)
    float pin_y_offset = 30 * editor->canvas_zoom;
    float pin_spacing = 20 * editor->canvas_zoom;
    float pin_radius = 5.0f * editor->canvas_zoom;

    // Giriş Pinleri
    for (size_t i = 0; i < node->input_pins.size; ++i) {
        fe_blueprint_pin_t* pin = FE_DYNAMIC_ARRAY_GET(&node->input_pins, i);
        pin->pos = ImVec2(node_min.x, node_min.y + pin_y_offset + i * pin_spacing);
        
        ImU32 pin_color = fe_bp_editor_get_pin_color(pin->type);
        draw_list->AddCircleFilled(pin->pos, pin_radius, pin_color);
        draw_list->AddText(pin->pos + ImVec2(pin_radius + 5, -ImGui::GetTextLineHeight() / 2), IM_COL32_WHITE, pin->name);

        // Pin etkileşimi (mouse hover)
        ImRect pin_hit_rect(pin->pos - ImVec2(pin_radius, pin_radius), pin->pos + ImVec2(pin_radius, pin_radius));
        if (pin_hit_rect.Contains(io.MousePos)) {
            editor->hovered_pin = pin;
            draw_list->AddCircle(pin->pos, pin_radius + 2, COLOR_HIGHLIGHT); // Vurgula
        }
    }

    // Çıkış Pinleri
    for (size_t i = 0; i < node->output_pins.size; ++i) {
        fe_blueprint_pin_t* pin = FE_DYNAMIC_ARRAY_GET(&node->output_pins, i);
        pin->pos = ImVec2(node_max.x, node_min.y + pin_y_offset + i * pin_spacing);
        
        ImU32 pin_color = fe_bp_editor_get_pin_color(pin->type);
        draw_list->AddCircleFilled(pin->pos, pin_radius, pin_color);
        ImVec2 text_size = ImGui::CalcTextSize(pin->name);
        draw_list->AddText(pin->pos - ImVec2(pin_radius + 5 + text_size.x, -ImGui::GetTextLineHeight() / 2), IM_COL32_WHITE, pin->name);

        // Pin etkileşimi (mouse hover)
        ImRect pin_hit_rect(pin->pos - ImVec2(pin_radius, pin_radius), pin->pos + ImVec2(pin_radius, pin_radius));
        if (pin_hit_rect.Contains(io.MousePos)) {
            editor->hovered_pin = pin;
            draw_list->AddCircle(pin->pos, pin_radius + 2, COLOR_HIGHLIGHT); // Vurgula
        }
    }
    
    // Node etkileşimi (sürükleme, sağ tıklama)
    ImGui::SetCursorScreenPos(node_min);
    ImGui::InvisibleButton(node->name, node_size_scaled); // Düğümün üzerine tıklanabilir alan oluştur

    if (ImGui::IsItemHovered()) {
        editor->hovered_node = node;
    }

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        editor->dragged_node_id = node->id;
    }

    if (editor->dragged_node_id == node->id) {
        node->pos.x += io.MouseDelta.x / editor->canvas_zoom;
        node->pos.y += io.MouseDelta.y / editor->canvas_zoom;
    }
}

static void fe_bp_editor_draw_connection(fe_blueprint_editor_t* editor, fe_blueprint_connection_t* connection) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    ImVec2 p1 = connection->start_pin->pos;
    ImVec2 p2 = connection->end_pin->pos;

    //Bezier eğrisi için kontrol noktaları
    float tangent = 50.0f * editor->canvas_zoom; // Eğrinin gerginliği
    ImVec2 cp1 = ImVec2(p1.x + tangent, p1.y);
    ImVec2 cp2 = ImVec2(p2.x - tangent, p2.y);

    draw_list->AddBezierCubic(p1, cp1, cp2, p2, COLOR_CONNECTION, 3.0f * editor->canvas_zoom);
}

static void fe_bp_editor_handle_input(fe_blueprint_editor_t* editor) {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    
    // Canvas'ı sürükleme (sağ mouse butonu)
    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        editor->canvas_origin.x += io.MouseDelta.x;
        editor->canvas_origin.y += io.MouseDelta.y;
    }

    // Canvas'ı yakınlaştırma (fare tekerleği)
    if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f) {
        float old_zoom = editor->canvas_zoom;
        editor->canvas_zoom += io.MouseWheel * 0.1f;
        editor->canvas_zoom = ImClamp(editor->canvas_zoom, 0.5f, 2.0f); // Zoom limitleri

        // Zoom merkezini fare pozisyonuna göre ayarla
        ImVec2 mouse_pos_in_canvas = ImVec2((io.MousePos.x - editor->canvas_origin.x) / old_zoom,
                                            (io.MousePos.y - editor->canvas_origin.y) / old_zoom);
        editor->canvas_origin.x = io.MousePos.x - mouse_pos_in_canvas.x * editor->canvas_zoom;
        editor->canvas_origin.y = io.MousePos.y - mouse_pos_in_canvas.y * editor->canvas_zoom;
    }

    // Pin bağlantısı başlatma/bitirme
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (editor->dragging_pin_id != 0) { // Bağlantı sürükleniyorsa
            fe_blueprint_pin_t* start_pin = fe_bp_editor_find_pin_by_id(editor->current_graph, editor->dragging_pin_id);
            if (start_pin && editor->hovered_pin && fe_bp_editor_can_connect_pins(start_pin, editor->hovered_pin)) {
                // Pin yönleri doğru olduğundan emin ol (Output -> Input)
                if (start_pin->direction == FE_BLUEPRINT_PIN_DIR_OUTPUT && editor->hovered_pin->direction == FE_BLUEPRINT_PIN_DIR_INPUT) {
                    fe_blueprint_editor_create_connection(editor, start_pin->id, editor->hovered_pin->id);
                } else if (start_pin->direction == FE_BLUEPRINT_PIN_DIR_INPUT && editor->hovered_pin->direction == FE_BLUEPRINT_PIN_DIR_OUTPUT) {
                     fe_blueprint_editor_create_connection(editor, editor->hovered_pin->id, start_pin->id);
                } else {
                    FE_LOG_WARN("Cannot connect pins: Invalid directions or types.");
                }
            } else if (start_pin) {
                FE_LOG_DEBUG("Connection dropped: %s. No valid target pin found.", start_pin->name);
            }
            editor->dragging_pin_id = 0;
            editor->potential_connection_start_pin = NULL;
            editor->potential_connection_end_pin = NULL;
        }
        editor->dragged_node_id = 0; // Sürüklenen düğüm yok
    }

    // Pin üzerine tıklama (bağlantı başlatma)
    if (editor->hovered_pin && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        editor->dragging_pin_id = editor->hovered_pin->id;
        editor->potential_connection_start_pin = editor->hovered_pin;
        FE_LOG_DEBUG("Started dragging from pin: %s (ID: %llu)", editor->hovered_pin->name, editor->hovered_pin->id);
    }
    
    // Sağ tıklama menüsü (canvas üzerinde)
    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("BlueprintContextMenu");
    }

    if (ImGui::BeginPopup("BlueprintContextMenu")) {
        // İmleç pozisyonunu al ve canvas koordinatlarına çevir
        ImVec2 mouse_pos_in_world = ImVec2((io.MousePos.x - editor->canvas_origin.x) / editor->canvas_zoom,
                                           (io.MousePos.y - editor->canvas_origin.y) / editor->canvas_zoom);

        ImGui::Text("Create Node");
        if (ImGui::MenuItem("Event: OnBeginPlay")) {
            fe_blueprint_editor_add_node(editor, FE_BLUEPRINT_NODE_TYPE_EVENT_START, NULL, mouse_pos_in_world);
        }
        if (ImGui::MenuItem("Print String")) {
            fe_blueprint_editor_add_node(editor, FE_BLUEPRINT_NODE_TYPE_PRINT_STRING, NULL, mouse_pos_in_world);
        }
        if (ImGui::MenuItem("Add (Int)")) {
            fe_blueprint_editor_add_node(editor, FE_BLUEPRINT_NODE_TYPE_ADD_INT, NULL, mouse_pos_in_world);
        }
        if (ImGui::MenuItem("If")) {
            fe_blueprint_editor_add_node(editor, FE_BLUEPRINT_NODE_TYPE_IF, NULL, mouse_pos_in_world);
        }
        if (ImGui::MenuItem("Get Object Location")) {
            fe_blueprint_editor_add_node(editor, FE_BLUEPRINT_NODE_TYPE_GET_OBJECT_LOCATION, NULL, mouse_pos_in_world);
        }
        if (ImGui::MenuItem("Set Object Location")) {
            fe_blueprint_editor_add_node(editor, FE_BLUEPRINT_NODE_TYPE_SET_OBJECT_LOCATION, NULL, mouse_pos_in_world);
        }
        ImGui::EndPopup();
    }
    
    // Sağ tıklama menüsü (node üzerinde)
    if (editor->hovered_node && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("NodeContextMenu");
    }
    if (ImGui::BeginPopup("NodeContextMenu")) {
        if (ImGui::MenuItem("Delete Node")) {
            fe_blueprint_editor_remove_node(editor, editor->hovered_node);
        }
        ImGui::EndPopup();
    }
    
    // Sağ tıklama menüsü (connection üzerinde)
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
        // İmlecin altındaki bağlantıyı bul
        fe_blueprint_connection_t* conn_under_mouse = NULL;
        for (size_t i = 0; i < editor->current_graph->connections.size; ++i) {
            fe_blueprint_connection_t* conn = FE_DYNAMIC_ARRAY_GET(&editor->current_graph->connections, i);
            // Basit bir hit testi (daha hassas olması gerekir)
            ImVec2 p1 = conn->start_pin->pos;
            ImVec2 p2 = conn->end_pin->pos;
            float dist_to_line = fe_math_distance_point_to_line_segment(io.MousePos, p1, p2); // Varsayım: fe_math var
            if (dist_to_line < 10.0f) { // Belirli bir eşik içinde mi
                conn_under_mouse = conn;
                break;
            }
        }
        if (conn_under_mouse) {
            editor->potential_connection_end_pin = (fe_blueprint_pin_t*)conn_under_mouse; // Hacky way to store connection ptr
            ImGui::OpenPopup("ConnectionContextMenu");
        }
    }
    if (ImGui::BeginPopup("ConnectionContextMenu")) {
        if (ImGui::MenuItem("Delete Connection")) {
            fe_blueprint_editor_remove_connection(editor, (fe_blueprint_connection_t*)editor->potential_connection_end_pin);
        }
        ImGui::EndPopup();
    }
}


void fe_blueprint_editor_draw_gui(fe_blueprint_editor_t* editor) {
    if (!editor || !editor->current_graph || !editor->is_open) {
        return;
    }

    // Yeni ImGui çerçevesini başlat (ana uygulama/renderer tarafından yapılmalı)
    // ImGui_ImplOpenGL3_NewFrame();
    // ImGui_ImplSDL3_NewFrame();
    // ImGui::NewFrame();

    // Reset hovered states
    editor->hovered_pin = NULL;
    editor->hovered_node = NULL;

    // Ana Blueprint Editor penceresi
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Blueprint Editor", &editor->is_open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar)) {
        editor->canvas_size = ImGui::GetContentRegionAvail(); // Pencerenin kullanılabilir alanı

        // Menü Çubuğu
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Blueprint")) {
                    fe_blueprint_editor_new_graph(editor, "New Blueprint");
                }
                if (ImGui::MenuItem("Open Blueprint...")) {
                    // Basit dosya seçici örneği
                    static char file_path_buffer[256] = "";
                    ImGui::InputText("##OpenBPPath", file_path_buffer, sizeof(file_path_buffer));
                    ImGui::SameLine();
                    if (ImGui::Button("Open")) {
                        fe_blueprint_editor_load_graph(editor, file_path_buffer);
                    }
                }
                if (ImGui::MenuItem("Save Blueprint")) {
                    static char file_path_buffer[256] = "";
                    ImGui::InputText("##SaveBPPath", file_path_buffer, sizeof(file_path_buffer));
                    ImGui::SameLine();
                    if (ImGui::Button("Save")) {
                        fe_blueprint_editor_save_graph(editor, file_path_buffer);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // Çizim alanı (canvas)
        ImGui::BeginChild("blueprint_canvas", editor->canvas_size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_screen_pos = ImGui::GetCursorScreenPos(); // Canvas'ın penceredeki başlangıç pozisyonu

        // Izgara çizimi
        ImVec2 grid_step = ImVec2(50.0f * editor->canvas_zoom, 50.0f * editor->canvas_zoom);
        ImVec2 grid_origin_offset = ImVec2(fmodf(editor->canvas_origin.x, grid_step.x), fmodf(editor->canvas_origin.y, grid_step.y));
        
        for (float x = grid_origin_offset.x; x < editor->canvas_size.x; x += grid_step.x) {
            draw_list->AddLine(ImVec2(canvas_screen_pos.x + x, canvas_screen_pos.y), ImVec2(canvas_screen_pos.x + x, canvas_screen_pos.y + editor->canvas_size.y), IM_COL32(200, 200, 200, 40));
        }
        for (float y = grid_origin_offset.y; y < editor->canvas_size.y; y += grid_step.y) {
            draw_list->AddLine(ImVec2(canvas_screen_pos.x, canvas_screen_pos.y + y), ImVec2(canvas_screen_pos.x + editor->canvas_size.x, canvas_screen_pos.y + y), IM_COL32(200, 200, 200, 40));
        }

        // Canvas'ı mevcut ImGui penceresine hizala
        draw_list->PushClipRect(canvas_screen_pos, ImVec2(canvas_screen_pos.x + editor->canvas_size.x, canvas_screen_pos.y + editor->canvas_size.y), true);

        // Bağlantıları çiz
        for (size_t i = 0; i < editor->current_graph->connections.size; ++i) {
            fe_blueprint_connection_t* conn = FE_DYNAMIC_ARRAY_GET(&editor->current_graph->connections, i);
            fe_bp_editor_draw_connection(editor, conn);
        }

        // Düğümleri çiz
        for (size_t i = 0; i < editor->current_graph->nodes.size; ++i) {
            fe_blueprint_node_t* node = FE_DYNAMIC_ARRAY_GET(&editor->current_graph->nodes, i);
            fe_bp_editor_draw_node(editor, node);
        }

        // Eğer bir pin sürükleniyorsa, fare imlecinden pime bir bağlantı çiz
        if (editor->dragging_pin_id != 0 && editor->potential_connection_start_pin) {
            ImVec2 start_point = editor->potential_connection_start_pin->pos;
            ImVec2 end_point = io.MousePos;
            
            // Eğer bir hedef pin üzerinde geziniyorsak, oraya bağlanıyormuş gibi göster
            if (editor->hovered_pin && fe_bp_editor_can_connect_pins(editor->potential_connection_start_pin, editor->hovered_pin)) {
                end_point = editor->hovered_pin->pos;
            }

            float tangent = 50.0f * editor->canvas_zoom;
            ImVec2 cp1, cp2;
            if (editor->potential_connection_start_pin->direction == FE_BLUEPRINT_PIN_DIR_OUTPUT) {
                cp1 = ImVec2(start_point.x + tangent, start_point.y);
                cp2 = ImVec2(end_point.x - tangent, end_point.y);
            } else { // Input pininden sürükleniyorsa
                cp1 = ImVec2(start_point.x - tangent, start_point.y);
                cp2 = ImVec2(end_point.x + tangent, end_point.y);
            }
            
            draw_list->AddBezierCubic(start_point, cp1, cp2, end_point, COLOR_HIGHLIGHT, 3.0f * editor->canvas_zoom);
        }


        // Inputları işle (sürükleme, yakınlaştırma, düğüm/pin tıklamaları)
        fe_bp_editor_handle_input(editor);

        draw_list->PopClipRect();
        ImGui::EndChild(); // blueprint_canvas

        ImGui::End(); // Blueprint Editor
    }

    // ImGui çizimini sonlandır (ana uygulama/renderer tarafından yapılmalı)
    // ImGui::Render();
    // ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
