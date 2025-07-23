#ifndef FE_VK_DEVICE_H
#define FE_VK_DEVICE_H

#include <vulkan/vulkan.h> // Vulkan API başlığı
#include <stdbool.h>
#include <stdint.h> // uint32_t için

// --- Vulkan Cihaz Hata Kodları ---
// fe_renderer_error_t'ye benzer, ancak Vulkan'a özgü detaylar eklenebilir.
typedef enum fe_vk_device_error {
    FE_VK_DEVICE_SUCCESS = 0,
    FE_VK_DEVICE_NOT_INITIALIZED,
    FE_VK_DEVICE_NO_SUITABLE_PHYSICAL_DEVICE, // Uygun fiziksel cihaz bulunamadı
    FE_VK_DEVICE_QUEUE_FAMILY_NOT_FOUND,      // Gerekli kuyruk ailesi bulunamadı
    FE_VK_DEVICE_EXTENSION_NOT_SUPPORTED,     // Gerekli cihaz uzantısı desteklenmiyor
    FE_VK_DEVICE_LOGICAL_DEVICE_CREATION_FAILED, // Mantıksal cihaz oluşturma hatası
    FE_VK_DEVICE_OUT_OF_MEMORY,
    FE_VK_DEVICE_UNKNOWN_ERROR
} fe_vk_device_error_t;

// --- Kuyruk Ailesi İndeksleri ---
typedef struct fe_vk_queue_family_indices {
    uint32_t graphics_family; // Grafik komutları için kuyruk ailesi indeksi
    uint32_t present_family;  // Sunum komutları için kuyruk ailesi indeksi (genellikle grafikle aynı)
    bool graphics_family_found;
    bool present_family_found;
} fe_vk_queue_family_indices_t;

// --- Takas Zinciri Destek Detayları ---
// Bir fiziksel cihazın takas zincirini destekleyip desteklemediğini kontrol etmek için
typedef struct fe_vk_swap_chain_support_details {
    VkSurfaceCapabilitiesKHR capabilities;   // Yüzey yetenekleri (min/max çözünürlük vb.)
    uint32_t format_count;
    VkSurfaceFormatKHR* formats;             // Desteklenen yüzey formatları (renk alanı, format)
    uint32_t present_mode_count;
    VkPresentModeKHR* present_modes;         // Desteklenen sunum modları (VSync, anında vb.)
} fe_vk_swap_chain_support_details_t;

// --- FE Vulkan Cihaz Yapısı (Gizli Tutaç) ---
// Bu, dahili Vulkan cihaz nesnelerini kapsüller.
// Dışarıdan doğrudan erişilmez.
typedef struct fe_vk_device {
    VkPhysicalDevice physical_device;       // Seçilen fiziksel cihaz
    VkDevice logical_device;                // Oluşturulan mantıksal cihaz
    VkQueue graphics_queue;                 // Grafik kuyruğu handle'ı
    VkQueue present_queue;                  // Sunum kuyruğu handle'ı

    fe_vk_queue_family_indices_t queue_family_indices; // Kuyruk ailesi indeksleri
    fe_vk_swap_chain_support_details_t swap_chain_support; // Takas zinciri desteği

    // Diğer gerekli Vulkan nesneleri veya bilgiler burada tutulabilir.
    // Örn: VkCommandPool, VkDescriptorPool, aktif uzantılar vb.
} fe_vk_device_t;

// Global Vulkan Instance (fe_vk_instance'dan alınacak)
// fe_vk_instance modülü daha önce oluşturulmuşsa, bu modül ona ihtiyaç duyacaktır.
// Basitlik için şimdilik global bir referans varsayılıyor.
// Gerçekte fe_vk_renderer_init'ten geçirilmelidir.
extern VkInstance g_vk_instance; // fe_vk_instance.h'ten alınacak
extern VkSurfaceKHR g_vk_surface; // fe_vk_window.h veya fe_vk_renderer'dan alınacak

// --- Fonksiyonlar ---

/**
 * @brief En uygun fiziksel cihazı seçer ve mantıksal cihazı oluşturur.
 * Bu fonksiyon, Vulkan instance ve render yüzeyini kullanarak bir GPU seçer,
 * gerekli kuyruk ailelerini bulur ve bir mantıksal cihaz oluşturur.
 *
 * @param required_device_extensions Fiziksel cihazın desteklemesi gereken uzantıların dizisi (örneğin VK_KHR_SWAPCHAIN_EXTENSION_NAME).
 * @param extension_count required_device_extensions dizisindeki eleman sayısı.
 * @param enable_validation_layers Doğrulama katmanlarının etkinleştirilip etkinleştirilmeyeceği.
 * @return fe_vk_device_t* Oluşturulan Vulkan cihazına işaretçi, başarısız olursa NULL.
 */
fe_vk_device_t* fe_vk_device_create(const char** required_device_extensions,
                                    uint32_t extension_count,
                                    bool enable_validation_layers);

/**
 * @brief Vulkan cihazını yok eder ve tüm ilişkili kaynakları serbest bırakır.
 *
 * @param device Yok edilecek Vulkan cihaz nesnesi.
 */
void fe_vk_device_destroy(fe_vk_device_t* device);

// --- Yardımcı Fonksiyonlar (Dahili veya Debug Amaçlı) ---
// Bu fonksiyonlar genellikle dahili yardımcılar veya hata ayıklama amaçlıdır.

/**
 * @brief Bir fiziksel cihazın uygun olup olmadığını kontrol eder.
 * @param device Kontrol edilecek fiziksel cihaz.
 * @param surface Render yüzeyi (takas zinciri uyumluluğu için).
 * @param required_extensions Gerekli cihaz uzantıları.
 * @param extension_count Gerekli uzantı sayısı.
 * @return bool Cihaz uygunsa true, aksi takdirde false.
 */
bool fe_vk_device_is_physical_device_suitable(VkPhysicalDevice device,
                                              VkSurfaceKHR surface,
                                              const char** required_extensions,
                                              uint32_t extension_count);

/**
 * @brief Bir fiziksel cihaz için kuyruk ailesi indekslerini bulur.
 * @param device Kontrol edilecek fiziksel cihaz.
 * @param surface Render yüzeyi (sunum desteği için).
 * @return fe_vk_queue_family_indices_t Bulunan indeksleri içeren yapı.
 */
fe_vk_queue_family_indices_t fe_vk_device_find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface);

/**
 * @brief Bir fiziksel cihaz için takas zinciri destek detaylarını sorgular.
 * @param device Kontrol edilecek fiziksel cihaz.
 * @param surface Render yüzeyi.
 * @return fe_vk_swap_chain_support_details_t Destek detayları.
 */
fe_vk_swap_chain_support_details_t fe_vk_device_query_swap_chain_support(VkPhysicalDevice device, VkSurfaceKHR surface);

#endif // FE_VK_DEVICE_H
