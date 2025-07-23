#ifndef FE_MATERIAL_EDITOR_H
#define FE_MATERIAL_EDITOR_H

#include "core/utils/fe_types.h" // fe_string, fe_vec3, fe_vec4, fe_mat4 vb. için
#include "graphics/resource/fe_texture.h" // fe_texture_t referansı için

// Materyal Düzenleyici Hata Kodları
typedef enum fe_material_editor_error {
    FE_MATERIAL_EDITOR_SUCCESS = 0,
    FE_MATERIAL_EDITOR_INVALID_ARGUMENT,
    FE_MATERIAL_EDITOR_FILE_NOT_FOUND,
    FE_MATERIAL_EDITOR_FILE_READ_ERROR,
    FE_MATERIAL_EDITOR_FILE_WRITE_ERROR,
    FE_MATERIAL_EDITOR_PARSE_ERROR,
    FE_MATERIAL_EDITOR_OUT_OF_MEMORY,
    FE_MATERIAL_EDITOR_MATERIAL_NOT_FOUND,
    FE_MATERIAL_EDITOR_UNKNOWN_ERROR
} fe_material_editor_error_t;

// --- Materyal Parametre Tipleri ---
// Shader'lara gönderilen parametrelerin tipleri
typedef enum fe_material_param_type {
    FE_MATERIAL_PARAM_TYPE_FLOAT,
    FE_MATERIAL_PARAM_TYPE_VEC2,
    FE_MATERIAL_PARAM_TYPE_VEC3,
    FE_MATERIAL_PARAM_TYPE_VEC4,
    FE_MATERIAL_PARAM_TYPE_COLOR_RGB,   // fe_vec3 olarak depolanır, 0-1 aralığında
    FE_MATERIAL_PARAM_TYPE_COLOR_RGBA,  // fe_vec4 olarak depolanır, 0-1 aralığında
    FE_MATERIAL_PARAM_TYPE_INT,
    FE_MATERIAL_PARAM_TYPE_BOOL,
    FE_MATERIAL_PARAM_TYPE_TEXTURE2D,   // fe_texture_t* referansı
    FE_MATERIAL_PARAM_TYPE_COUNT
} fe_material_param_type_t;

// --- Materyal Parametresi Birleşimi (Union) ---
// Farklı tipteki parametre değerlerini tutmak için union kullanılır.
typedef union fe_material_param_value {
    float float_val;
    fe_vec2 vec2_val;
    fe_vec3 vec3_val;
    fe_vec4 vec4_val;
    int int_val;
    bool bool_val;
    fe_texture_t* texture_val; // Dokuyu referans olarak tutar, sahibi değildir.
} fe_material_param_value_t;

// --- Materyal Parametresi Yapısı ---
typedef struct fe_material_parameter {
    fe_string name; // Parametrenin adı (shader'daki ismiyle eşleşmeli)
    fe_material_param_type_t type; // Parametre tipi
    fe_material_param_value_t value; // Parametre değeri
} fe_material_parameter_t;

// --- Materyal Tanımı ---
// Bir materyalin temel yapısını ve özelliklerini tanımlar.
typedef struct fe_material {
    fe_string id;              // Benzersiz materyal ID'si (UUID veya dosya yolu)
    fe_string name;            // Kullanıcı dostu materyal adı
    fe_string shader_path;     // Bu materyali kullanan shader dosyasının yolu
    fe_string description;     // Materyalin kısa açıklaması

    // Parametre listesi (dinamik dizi)
    fe_material_parameter_t* parameters;
    size_t parameter_count;
    size_t parameter_capacity; // Ayrılmış kapasite

    // Materyal editor'e özel dahili durumlar veya işaretçiler eklenebilir.
    bool is_dirty; // Materyalde değişiklik yapıldı mı?
} fe_material_t;

// --- Materyal Düzenleyici Durumu ---
// Editörün dahili durumunu tutan global veya singleton yapı.
typedef struct fe_material_editor_state {
    // Yüklü materyallerin listesi (cache)
    fe_material_t** loaded_materials;
    size_t material_count;
    size_t material_capacity;

    // Şu an düzenlenen materyal (eğer varsa)
    fe_material_t* current_material_being_edited;

    // Diğer dahili durum değişkenleri
} fe_material_editor_state_t;

// --- Fonksiyon Deklarasyonları ---

/**
 * @brief Materyal düzenleyici sistemini başlatır.
 * Cache'i ve diğer dahili yapıları hazırlar.
 *
 * @return fe_material_editor_error_t Başarı durumunu döner.
 */
fe_material_editor_error_t fe_material_editor_init();

/**
 * @brief Materyal düzenleyici sistemini kapatır ve tüm kaynakları serbest bırakır.
 * Yüklü tüm materyalleri bellekten temizler.
 */
void fe_material_editor_shutdown();

/**
 * @brief Yeni bir boş materyal oluşturur ve düzenleyiciye ekler.
 * Materyal bellekte oluşturulur ve `current_material_being_edited` olarak ayarlanır.
 *
 * @param name Yeni materyalin adı.
 * @param shader_path Bu materyalin kullanacağı shader'ın yolu.
 * @return fe_material_t* Oluşturulan materyal nesnesine işaretçi, başarısız olursa NULL.
 */
fe_material_t* fe_material_editor_create_new_material(const char* name, const char* shader_path);

/**
 * @brief Belirtilen yoldan bir materyali yükler.
 * Materyal dosyadan okunur, ayrıştırılır ve belleğe yüklenir.
 * Yüklenen materyal `current_material_being_edited` olarak ayarlanır.
 *
 * @param file_path Yüklenecek materyal dosyasının yolu (örneğin, "materials/default.fem").
 * @return fe_material_t* Yüklenen materyal nesnesine işaretçi, başarısız olursa NULL.
 */
fe_material_t* fe_material_editor_load_material(const char* file_path);

/**
 * @brief Belirtilen materyali dosya sistemine kaydeder.
 * Materyal özellikleri belirtilen yola serileştirilir.
 *
 * @param material Kaydedilecek materyal nesnesi.
 * @param file_path Kaydedilecek dosyanın yolu. Eğer NULL ise, materyalin ID'si/adı kullanılarak varsayılan bir yol oluşturulur.
 * @return fe_material_editor_error_t Başarı durumunu döner.
 */
fe_material_editor_error_t fe_material_editor_save_material(fe_material_t* material, const char* file_path);

/**
 * @brief Düzenleyiciden bir materyali kaldırır ve bellekten serbest bırakır.
 * Eğer kaldırılan materyal şu an düzenlenen materyalse, `current_material_being_edited` NULL'lanır.
 *
 * @param material_id Kaldırılacak materyalin ID'si.
 * @return fe_material_editor_error_t Başarı durumunu döner.
 */
fe_material_editor_error_t fe_material_editor_remove_material(const char* material_id);

/**
 * @brief Materyale yeni bir parametre ekler.
 *
 * @param material Parametrenin ekleneceği materyal.
 * @param name Parametrenin adı.
 * @param type Parametrenin tipi.
 * @param initial_value Başlangıç değeri (union olarak geçilir).
 * @return fe_material_editor_error_t Başarı durumunu döner.
 */
fe_material_editor_error_t fe_material_editor_add_parameter(fe_material_t* material, const char* name, fe_material_param_type_t type, fe_material_param_value_t initial_value);

/**
 * @brief Bir materyaldeki parametrenin değerini günceller.
 *
 * @param material Parametresinin güncelleneceği materyal.
 * @param name Güncellenecek parametrenin adı.
 * @param new_value Yeni parametre değeri.
 * @return fe_material_editor_error_t Başarı durumunu döner.
 */
fe_material_editor_error_t fe_material_editor_update_parameter(fe_material_t* material, const char* name, fe_material_param_value_t new_value);

/**
 * @brief Bir materyalden bir parametreyi kaldırır.
 *
 * @param material Parametresinin kaldırılacağı materyal.
 * @param name Kaldırılacak parametrenin adı.
 * @return fe_material_editor_error_t Başarı durumunu döner.
 */
fe_material_editor_error_t fe_material_editor_remove_parameter(fe_material_t* material, const char* name);

/**
 * @brief Bir materyalin belirli bir parametresini adına göre alır.
 *
 * @param material Parametresi alınacak materyal.
 * @param name Alınacak parametrenin adı.
 * @return fe_material_parameter_t* Bulunan parametreye işaretçi, bulunamazsa NULL.
 */
fe_material_parameter_t* fe_material_editor_get_parameter(fe_material_t* material, const char* name);

/**
 * @brief Bir materyalin belirli bir ID'ye sahip olup olmadığını kontrol eder.
 * @param material_id Kontrol edilecek materyal ID'si.
 * @return fe_material_t* Bulunan materyal nesnesine işaretçi, bulunamazsa NULL.
 */
fe_material_t* fe_material_editor_get_material_by_id(const char* material_id);


/**
 * @brief Halihazırda düzenlenmekte olan materyali döndürür.
 * @return fe_material_t* Şu anki materyale işaretçi, yoksa NULL.
 */
fe_material_t* fe_material_editor_get_current_material();

/**
 * @brief Şu anda düzenlenmekte olan materyali ayarlar.
 * @param material Ayarlanacak materyal. NULL olabilir.
 */
void fe_material_editor_set_current_material(fe_material_t* material);


#endif // FE_MATERIAL_EDITOR_H
