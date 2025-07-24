#ifndef FE_BLUEPRINT_EDITOR_H
#define FE_BLUEPRINT_EDITOR_H

#include "core/utils/fe_types.h"
#include "core/containers/fe_dynamic_array.h" // Düğümleri, pinleri ve bağlantıları tutmak için
#include "core/containers/fe_hash_map.g.h"    // Düğümlere ID ile hızlı erişim için

// İleri bildirimler (ImGui bağımlılığını başlıkta azaltmak için)
namespace ImGui { struct ImVec2; struct ImU32; }

// --- Blueprint Pin Türleri ---
// Veri türleri ve yürütme (exec) pini için
typedef enum fe_blueprint_pin_type {
    FE_BLUEPRINT_PIN_TYPE_EXECUTION, // Yürütme akışı (beyaz oklar)
    FE_BLUEPRINT_PIN_TYPE_BOOL,
    FE_BLUEPRINT_PIN_TYPE_INT,
    FE_BLUEPRINT_PIN_TYPE_FLOAT,
    FE_BLUEPRINT_PIN_TYPE_STRING,
    FE_BLUEPRINT_PIN_TYPE_VECTOR3,
    FE_BLUEPRINT_PIN_TYPE_OBJECT, // Genel oyun nesnesi referansı
    // ... daha fazla tür eklenebilir
} fe_blueprint_pin_type_t;

// Pin'in yönü (giriş mi çıkış mı)
typedef enum fe_blueprint_pin_direction {
    FE_BLUEPRINT_PIN_DIR_INPUT,
    FE_BLUEPRINT_PIN_DIR_OUTPUT,
} fe_blueprint_pin_direction_t;

// --- Blueprint Düğüm Türleri ---
// Fonksiyon çağrıları, olaylar, değişkenler vb.
typedef enum fe_blueprint_node_type {
    FE_BLUEPRINT_NODE_TYPE_UNKNOWN = 0,
    FE_BLUEPRINT_NODE_TYPE_EVENT_START,  // Blueprint'in başlangıç noktası (örneğin "OnBeginPlay")
    FE_BLUEPRINT_NODE_TYPE_PRINT_STRING, // Basit bir debug çıktısı
    FE_BLUEPRINT_NODE_TYPE_ADD_INT,      // İki int'i toplama
    FE_BLUEPRINT_NODE_TYPE_GET_VARIABLE, // Değişken değeri alma
    FE_BLUEPRINT_NODE_TYPE_SET_VARIABLE, // Değişken değeri atama
    FE_BLUEPRINT_NODE_TYPE_IF,           // Koşullu dallanma
    FE_BLUEPRINT_NODE_TYPE_GET_OBJECT_LOCATION, // Oyun nesnesinin konumunu alma
    FE_BLUEPRINT_NODE_TYPE_SET_OBJECT_LOCATION, // Oyun nesnesinin konumunu ayarlama
    // ... motorunuzdaki fonksiyonlara veya olaylara karşılık gelen düğümler
} fe_blueprint_node_type_t;

// --- Blueprint Pin Yapısı ---
typedef struct fe_blueprint_pin {
    uint64_t                id;          // Benzersiz pin kimliği
    const char* name;        // Pin adı (örn. "Input A", "Output B", "Exec")
    fe_blueprint_pin_type_t type;        // Pin'in veri veya yürütme tipi
    fe_blueprint_pin_direction_t direction; // Giriş mi, çıkış mı?
    struct fe_blueprint_node* parent_node; // Hangi düğüme ait olduğu

    // Pin'in GUI'deki konumu (düğüm pozisyonuna göre ofset)
    ImGui::ImVec2           pos;
    float                   radius;      // Pin'in çizim yarıçapı

    // Bağlantı durumu
    bool                    is_connected; // Bu pinden/pine bir bağlantı var mı?
    // Pin'in değeri (eğer bir literal veya varsayılan değer ise)
    // Union veya void* ile farklı tipleri tutabiliriz
    union {
        bool    b_val;
        int     i_val;
        float   f_val;
        char* s_val; // Stringler için dinamik hafıza yönetimi gerekir
        // fe_vec3_t vec3_val; // Eğer fe_vec3_t tanımlıysa
        void* obj_val;
    } default_value; // Eğer bu bir giriş pini ise ve bağlı değilse kullanılacak varsayılan değer

} fe_blueprint_pin_t;

// --- Blueprint Düğüm Yapısı ---
typedef struct fe_blueprint_node {
    uint64_t                id;         // Benzersiz düğüm kimliği
    const char* name;       // Düğüm adı (örn. "Print String", "Add (Int)")
    fe_blueprint_node_type_t type;      // Düğümün tipi
    ImGui::ImVec2           pos;        // Düğümün grafik üzerindeki konumu
    ImGui::ImVec2           size;       // Düğümün boyutu (otomatik olarak hesaplanabilir)

    FE_DYNAMIC_ARRAY(fe_blueprint_pin_t*) input_pins;  // Giriş pinleri dizisi
    FE_DYNAMIC_ARRAY(fe_blueprint_pin_t*) output_pins; // Çıkış pinleri dizisi

    // Düğüme özel veriler (örn. bir değişken düğümü için değişken adı)
    void* node_data; // type'a göre cast edilecek özel veri

} fe_blueprint_node_t;

// --- Blueprint Bağlantı (Connection/Wire) Yapısı ---
typedef struct fe_blueprint_connection {
    uint64_t id;         // Benzersiz bağlantı kimliği
    uint64_t start_pin_id; // Çıkış pininin ID'si
    uint64_t end_pin_id;   // Giriş pininin ID'si

    // Sadece kolaylık için, gerçekte pin ID'leri ile aranabilir
    fe_blueprint_pin_t* start_pin;
    fe_blueprint_pin_t* end_pin;
} fe_blueprint_connection_t;

// --- Blueprint Grafiği Yapısı (Tüm blueprint'i temsil eder) ---
typedef struct fe_blueprint_graph {
    FE_DYNAMIC_ARRAY(fe_blueprint_node_t*)      nodes;      // Grafikteki tüm düğümler
    FE_DYNAMIC_ARRAY(fe_blueprint_connection_t*) connections; // Grafikteki tüm bağlantılar

    // Düğümlere ve pinlere hızlı erişim için haritalar
    FE_HASH_MAP_DEFINE(uint64_t, fe_blueprint_node_t*) id_to_node_map;
    FE_HASH_MAP_DEFINE(uint64_t, fe_blueprint_pin_t*) id_to_pin_map;

    uint64_t next_id; // Yeni düğüm, pin, bağlantı için kullanılacak ID
    char     name[128]; // Blueprint'in adı (örn. "Player_AI", "Door_Logic")
} fe_blueprint_graph_t;

// --- Blueprint Düzenleyici Yapısı ---
typedef struct fe_blueprint_editor {
    fe_blueprint_graph_t* current_graph; // Şu an üzerinde çalışılan blueprint

    ImGui::ImVec2 canvas_origin;  // Canvas'ın kaydırma ofseti
    float         canvas_zoom;    // Canvas'ın yakınlaştırma seviyesi

    // Düzenleyici durumu
    bool is_open;               // Editör penceresi açık mı?
    uint64_t dragged_node_id;   // Sürüklenen düğümün ID'si (0 ise sürükleme yok)
    uint64_t dragging_pin_id;   // Bağlantı başlatılan pinin ID'si (0 ise bağlantı başlatılmadı)
    fe_blueprint_pin_t* hovered_pin; // Fare imlecinin üzerinde olduğu pin
    fe_blueprint_node_t* hovered_node; // Fare imlecinin üzerinde olduğu düğüm

    // Bağlantı için geçici bilgiler
    fe_blueprint_pin_t* potential_connection_start_pin; // Bağlantı başlatılacak pin
    fe_blueprint_pin_t* potential_connection_end_pin;   // Bağlantı kurulabilecek hedef pin

    // Ekran boyutları veya render hedefine göre ayarlanacak
    ImGui::ImVec2 canvas_size;

    // Yürütme ortamı ile iletişim için (simülasyon, debug)
    // fe_blueprint_interpreter_t* interpreter; // veya sanal makine
} fe_blueprint_editor_t;

// --- Fonksiyon Deklarasyonları ---

/**
 * @brief Blueprint düzenleyiciyi başlatır.
 * @param editor fe_blueprint_editor_t yapısının işaretçisi.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_blueprint_editor_init(fe_blueprint_editor_t* editor);

/**
 * @brief Blueprint düzenleyiciyi kapatır ve kaynakları serbest bırakır.
 * @param editor fe_blueprint_editor_t yapısının işaretçisi.
 */
void fe_blueprint_editor_shutdown(fe_blueprint_editor_t* editor);

/**
 * @brief Blueprint düzenleyicinin GUI'sini çizer ve kullanıcı etkileşimlerini işler.
 * Bu fonksiyon her kare çağrılmalıdır.
 * @param editor fe_blueprint_editor_t yapısının işaretçisi.
 */
void fe_blueprint_editor_draw_gui(fe_blueprint_editor_t* editor);

/**
 * @brief Yeni bir boş blueprint grafiği oluşturur ve düzenleyicide aktif hale getirir.
 * @param editor fe_blueprint_editor_t yapısının işaretçisi.
 * @param graph_name Yeni grafiğin adı.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_blueprint_editor_new_graph(fe_blueprint_editor_t* editor, const char* graph_name);

/**
 * @brief Blueprint grafiğini bir dosyadan yükler ve düzenleyicide aktif hale getirir.
 * @param editor fe_blueprint_editor_t yapısının işaretçisi.
 * @param file_path Yükleme yolu.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_blueprint_editor_load_graph(fe_blueprint_editor_t* editor, const char* file_path);

/**
 * @brief Mevcut blueprint grafiğini bir dosyaya kaydeder.
 * @param editor fe_blueprint_editor_t yapısının işaretçisi.
 * @param file_path Kaydetme yolu.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_blueprint_editor_save_graph(fe_blueprint_editor_t* editor, const char* file_path);

/**
 * @brief Blueprint grafiğine yeni bir düğüm ekler.
 * @param editor fe_blueprint_editor_t yapısının işaretçisi.
 * @param node_type Eklenecek düğümün tipi.
 * @param name Düğümün adı (NULL ise varsayılan isim kullanılır).
 * @param initial_pos Düğümün canvas üzerindeki başlangıç konumu.
 * @return fe_blueprint_node_t* Oluşturulan düğümün işaretçisi, hata durumunda NULL.
 */
fe_blueprint_node_t* fe_blueprint_editor_add_node(fe_blueprint_editor_t* editor,
                                                  fe_blueprint_node_type_t node_type,
                                                  const char* name,
                                                  ImGui::ImVec2 initial_pos);

/**
 * @brief Blueprint grafiğinden bir düğümü siler ve ona bağlı tüm bağlantıları da kaldırır.
 * @param editor fe_blueprint_editor_t yapısının işaretçisi.
 * @param node_to_remove Silinecek düğümün işaretçisi.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_blueprint_editor_remove_node(fe_blueprint_editor_t* editor, fe_blueprint_node_t* node_to_remove);

/**
 * @brief İki pin arasında bir bağlantı oluşturur.
 * @param editor fe_blueprint_editor_t yapısının işaretçisi.
 * @param output_pin_id Bağlantının çıkış pininin ID'si.
 * @param input_pin_id Bağlantının giriş pininin ID'si.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_blueprint_editor_create_connection(fe_blueprint_editor_t* editor, uint64_t output_pin_id, uint64_t input_pin_id);

/**
 * @brief Bir bağlantıyı kaldırır.
 * @param editor fe_blueprint_editor_t yapısının işaretçisi.
 * @param connection_to_remove Kaldırılacak bağlantının işaretçisi.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
bool fe_blueprint_editor_remove_connection(fe_blueprint_editor_t* editor, fe_blueprint_connection_t* connection_to_remove);

// --- Dahili Yardımcı Fonksiyon Deklarasyonları (Kaynak dosyasında tanımlanacak) ---
// Bunlar, ImGui çizimini ve etkileşimleri yönetir.
static void fe_bp_editor_draw_node(fe_blueprint_editor_t* editor, fe_blueprint_node_t* node);
static void fe_bp_editor_draw_connection(fe_blueprint_editor_t* editor, fe_blueprint_connection_t* connection);
static void fe_bp_editor_handle_input(fe_blueprint_editor_t* editor);
static fe_blueprint_pin_t* fe_bp_editor_find_pin_by_id(fe_blueprint_graph_t* graph, uint64_t pin_id);
static fe_blueprint_node_t* fe_bp_editor_find_node_by_id(fe_blueprint_graph_t* graph, uint64_t node_id);
static bool fe_bp_editor_can_connect_pins(fe_blueprint_pin_t* pin_a, fe_blueprint_pin_t* pin_b);
static ImU32 fe_bp_editor_get_pin_color(fe_blueprint_pin_type_t type);
static void fe_bp_editor_setup_node_pins(fe_blueprint_node_t* node, fe_blueprint_graph_t* graph);
static fe_blueprint_pin_t* fe_bp_editor_create_pin(fe_blueprint_graph_t* graph, const char* name, fe_blueprint_pin_type_t type, fe_blueprint_pin_direction_t dir, fe_blueprint_node_t* parent_node);
static void fe_bp_editor_destroy_pin(fe_blueprint_graph_t* graph, fe_blueprint_pin_t* pin);
static void fe_bp_editor_destroy_node(fe_blueprint_graph_t* graph, fe_blueprint_node_t* node);
static void fe_bp_editor_destroy_connection(fe_blueprint_graph_t* graph, fe_blueprint_connection_t* connection);


#endif // FE_BLUEPRINT_EDITOR_H
