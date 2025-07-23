#include "graphics/vulkan/fe_vk_device.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h" // Bellek yönetimi için

#include <string.h> // strcmp, memset
#include <stdlib.h> // malloc, free

// Global Vulkan Instance ve Surface referansları (fe_vk_renderer veya fe_vk_instance/fe_vk_window tarafından sağlanmalı)
// Gerçek projede bu, fe_vk_renderer'a bir argüman olarak geçirilmeli veya global bir bağlamda tutulmalıdır.
extern VkInstance g_vk_instance; // fe_vk_instance.c'den dışarıya aktarılmış
extern VkSurfaceKHR g_vk_surface; // fe_vk_window.c'den dışarıya aktarılmış (veya fe_vk_renderer'da oluşturulmuş)


// --- Yardımcı Fonksiyonların Uygulaması ---

// Bir fiziksel cihazın gerekli uzantıları destekleyip desteklemediğini kontrol eder
static bool fe_vk_device_check_extension_support(VkPhysicalDevice device,
                                                 const char** required_extensions,
                                                 uint32_t extension_count) {
    uint32_t available_extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &available_extension_count, NULL);

    VkExtensionProperties* available_extensions =
        fe_malloc(sizeof(VkExtensionProperties) * available_extension_count, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    if (!available_extensions) {
        FE_LOG_ERROR("Failed to allocate memory for available device extensions.");
        return false;
    }
    vkEnumerateDeviceExtensionProperties(device, NULL, &available_extension_count, available_extensions);

    bool all_extensions_supported = true;
    for (uint32_t i = 0; i < extension_count; ++i) {
        bool found = false;
        for (uint32_t j = 0; j < available_extension_count; ++j) {
            if (strcmp(required_extensions[i], available_extensions[j].extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            FE_LOG_WARN("Required device extension not supported: %s", required_extensions[i]);
            all_extensions_supported = false;
            break;
        }
    }

    fe_free(available_extensions, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    return all_extensions_supported;
}

// Fiziksel cihazın kuyruk ailesi indekslerini bulur
fe_vk_queue_family_indices_t fe_vk_device_find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) {
    fe_vk_queue_family_indices_t indices = {0};
    indices.graphics_family_found = false;
    indices.present_family_found = false;

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);

    VkQueueFamilyProperties* queue_families =
        fe_malloc(sizeof(VkQueueFamilyProperties) * queue_family_count, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    if (!queue_families) {
        FE_LOG_ERROR("Failed to allocate memory for queue family properties.");
        return indices; // Hata durumunda boş döndür
    }
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);

    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
            indices.graphics_family_found = true;
        }

        VkBool32 present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
        if (present_support) {
            indices.present_family = i;
            indices.present_family_found = true;
        }

        if (indices.graphics_family_found && indices.present_family_found) {
            break; // Hem grafik hem de sunum desteği bulundu
        }
    }

    fe_free(queue_families, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    return indices;
}

// Fiziksel cihaz için takas zinciri destek detaylarını sorgular
fe_vk_swap_chain_support_details_t fe_vk_device_query_swap_chain_support(VkPhysicalDevice device, VkSurfaceKHR surface) {
    fe_vk_swap_chain_support_details_t details = {0};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.format_count, NULL);
    if (details.format_count != 0) {
        details.formats = fe_malloc(sizeof(VkSurfaceFormatKHR) * details.format_count, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
        if (details.formats) {
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.format_count, details.formats);
        } else {
            details.format_count = 0; // Bellek tahsis edilemedi
            FE_LOG_ERROR("Failed to allocate memory for swap chain formats.");
        }
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.present_mode_count, NULL);
    if (details.present_mode_count != 0) {
        details.present_modes = fe_malloc(sizeof(VkPresentModeKHR) * details.present_mode_count, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
        if (details.present_modes) {
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.present_mode_count, details.present_modes);
        } else {
            details.present_mode_count = 0; // Bellek tahsis edilemedi
            FE_LOG_ERROR("Failed to allocate memory for swap chain present modes.");
        }
    }
    return details;
}

// Takas zinciri destek detaylarının belleğini serbest bırakır
static void fe_vk_device_free_swap_chain_support_details(fe_vk_swap_chain_support_details_t* details) {
    if (details->formats) {
        fe_free(details->formats, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
        details->formats = NULL;
    }
    details->format_count = 0;

    if (details->present_modes) {
        fe_free(details->present_modes, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
        details->present_modes = NULL;
    }
    details->present_mode_count = 0;
}

// Fiziksel cihazın uygun olup olmadığını kontrol eder
bool fe_vk_device_is_physical_device_suitable(VkPhysicalDevice device,
                                              VkSurfaceKHR surface,
                                              const char** required_extensions,
                                              uint32_t extension_count) {
    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceProperties(device, &device_properties);
    vkGetPhysicalDeviceFeatures(device, &device_features);

    FE_LOG_INFO("Evaluating physical device: %s (Type: %d)", device_properties.deviceName, device_properties.deviceType);

    // Temel özellik kontrolü (örneğin, ayrık GPU tercih edilebilir)
    bool is_discrete_gpu = device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    if (!is_discrete_gpu) {
        FE_LOG_DEBUG("  Not a discrete GPU. (Type: %d)", device_properties.deviceType);
    }

    // Kuyruk ailesi desteği kontrolü
    fe_vk_queue_family_indices_t indices = fe_vk_device_find_queue_families(device, surface);
    bool queue_families_complete = indices.graphics_family_found && indices.present_family_found;
    if (!queue_families_complete) {
        FE_LOG_DEBUG("  Required queue families not found (Graphics: %d, Present: %d)",
                     indices.graphics_family_found, indices.present_family_found);
    }

    // Cihaz uzantısı desteği kontrolü
    bool extensions_supported = fe_vk_device_check_extension_support(device, required_extensions, extension_count);
    if (!extensions_supported) {
        FE_LOG_DEBUG("  Required extensions not supported.");
    }

    // Takas zinciri desteği kontrolü (en az bir format ve bir sunum modu olmalı)
    fe_vk_swap_chain_support_details_t swap_chain_support = fe_vk_device_query_swap_chain_support(device, surface);
    bool swap_chain_adequate = swap_chain_support.format_count > 0 && swap_chain_support.present_mode_count > 0;
    if (!swap_chain_adequate) {
        FE_LOG_DEBUG("  Swap chain not adequate (Formats: %u, Present Modes: %u)",
                     swap_chain_support.format_count, swap_chain_support.present_mode_count);
    }
    fe_vk_device_free_swap_chain_support_details(&swap_chain_support); // Belleği serbest bırak

    // Gerekli tüm özelliklerin kontrolü
    bool all_features_met = queue_families_complete && extensions_supported && swap_chain_adequate;

    // Ayrıca, Vulkan özelliklerinin (örneğin geometryShader) desteklendiğini de kontrol edebiliriz
    // if (!device_features.geometryShader) {
    //     FE_LOG_DEBUG("  Geometry shader not supported.");
    //     all_features_met = false;
    // }

    if (all_features_met) {
        FE_LOG_INFO("  Device '%s' is suitable.", device_properties.deviceName);
    } else {
        FE_LOG_INFO("  Device '%s' is NOT suitable.", device_properties.deviceName);
    }

    return all_features_met;
}

// --- Ana Cihaz Yönetim Fonksiyonları Uygulaması ---

fe_vk_device_t* fe_vk_device_create(const char** required_device_extensions,
                                    uint32_t extension_count,
                                    bool enable_validation_layers) {
    if (g_vk_instance == VK_NULL_HANDLE || g_vk_surface == VK_NULL_HANDLE) {
        FE_LOG_CRITICAL("Vulkan instance or surface is NULL. Cannot create device.");
        return NULL;
    }

    fe_vk_device_t* vk_device = fe_malloc(sizeof(fe_vk_device_t), FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    if (!vk_device) {
        FE_LOG_CRITICAL("Failed to allocate memory for fe_vk_device_t.");
        return NULL;
    }
    memset(vk_device, 0, sizeof(fe_vk_device_t)); // Sıfırla

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_vk_instance, &device_count, NULL);
    if (device_count == 0) {
        FE_LOG_CRITICAL("Failed to find GPUs with Vulkan support!");
        fe_free(vk_device, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
        return NULL;
    }

    VkPhysicalDevice* devices = fe_malloc(sizeof(VkPhysicalDevice) * device_count, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    if (!devices) {
        FE_LOG_CRITICAL("Failed to allocate memory for physical devices.");
        fe_free(vk_device, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
        return NULL;
    }
    vkEnumeratePhysicalDevices(g_vk_instance, &device_count, devices);

    // En uygun fiziksel cihazı seç
    for (uint32_t i = 0; i < device_count; ++i) {
        if (fe_vk_device_is_physical_device_suitable(devices[i], g_vk_surface, required_device_extensions, extension_count)) {
            vk_device->physical_device = devices[i];
            vk_device->queue_family_indices = fe_vk_device_find_queue_families(vk_device->physical_device, g_vk_surface);
            vk_device->swap_chain_support = fe_vk_device_query_swap_chain_support(vk_device->physical_device, g_vk_surface);
            FE_LOG_INFO("Selected physical device: %s", vk_device->physical_device); // Hata: Bu şekilde %s ile yazdırılmaz.
            // VkPhysicalDeviceProperties'i kullanarak ismini yazdır.
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(vk_device->physical_device, &props);
            FE_LOG_INFO("Selected physical device: %s", props.deviceName);
            break;
        }
    }

    fe_free(devices, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);

    if (vk_device->physical_device == VK_NULL_HANDLE) {
        FE_LOG_CRITICAL("Failed to find a suitable GPU!");
        fe_free(vk_device, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
        return NULL;
    }

    // Mantıksal Cihaz (Logical Device) Oluşturma
    // Kuyruk oluşturma bilgileri
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_infos[2]; // Max 2 kuyruk (grafik ve sunum)
    uint32_t queue_create_info_count = 0;

    // Grafik kuyruğu
    VkDeviceQueueCreateInfo graphics_queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = vk_device->queue_family_indices.graphics_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };
    queue_create_infos[queue_create_info_count++] = graphics_queue_create_info;

    // Sunum kuyruğu (eğer grafik kuyruğundan farklıysa)
    if (vk_device->queue_family_indices.graphics_family != vk_device->queue_family_indices.present_family) {
        VkDeviceQueueCreateInfo present_queue_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = vk_device->queue_family_indices.present_family,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority
        };
        queue_create_infos[queue_create_info_count++] = present_queue_create_info;
    }

    // Cihaz özellikleri (şu an için boş bırakılabilir, veya tümünü etkinleştirmek için bir yapı)
    VkPhysicalDeviceFeatures device_features = {0};
    // device_features.samplerAnisotropy = VK_TRUE; // Eğer anizotropik filtreleme kullanılacaksa
    // device_features.geometryShader = VK_TRUE; // Eğer geometry shader kullanılacaksa

    // Mantıksal cihaz oluşturma bilgileri
    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = queue_create_infos,
        .queueCreateInfoCount = queue_create_info_count,
        .pEnabledFeatures = &device_features,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = required_device_extensions
    };

    // Doğrulama katmanlarını etkinleştir
#ifdef FE_DEBUG_BUILD
    if (enable_validation_layers) {
        // Bu kısım fe_vk_instance'dan alınmalı. Burada yeniden tanımlamaktan kaçınılmalı.
        // Güvenlik için, instance doğrulama katmanları ile aynı olmalı.
        const char* validationLayers[] = {
            "VK_LAYER_KHRONOS_validation"
        };
        create_info.enabledLayerCount = (uint32_t)ARRAY_SIZE(validationLayers);
        create_info.ppEnabledLayerNames = validationLayers;
    } else {
        create_info.enabledLayerCount = 0;
    }
#else
    create_info.enabledLayerCount = 0;
#endif

    // Mantıksal cihazı oluştur
    VkResult result = vkCreateDevice(vk_device->physical_device, &create_info, NULL, &vk_device->logical_device);
    if (result != VK_SUCCESS) {
        FE_LOG_CRITICAL("Failed to create logical device! VkResult: %d", result);
        fe_vk_device_free_swap_chain_support_details(&vk_device->swap_chain_support); // Hata durumunda belleği serbest bırak
        fe_free(vk_device, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
        return NULL;
    }

    // Kuyruk handle'larını al
    vkGetDeviceQueue(vk_device->logical_device, vk_device->queue_family_indices.graphics_family, 0, &vk_device->graphics_queue);
    vkGetDeviceQueue(vk_device->logical_device, vk_device->queue_family_indices.present_family, 0, &vk_device->present_queue);

    FE_LOG_INFO("Vulkan logical device created successfully.");
    return vk_device;
}

void fe_vk_device_destroy(fe_vk_device_t* device) {
    if (!device) {
        FE_LOG_WARN("Attempted to destroy NULL fe_vk_device_t.");
        return;
    }

    if (device->logical_device != VK_NULL_HANDLE) {
        vkDestroyDevice(device->logical_device, NULL);
        FE_LOG_INFO("Vulkan logical device destroyed.");
    }

    fe_vk_device_free_swap_chain_support_details(&device->swap_chain_support);

    fe_free(device, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    FE_LOG_INFO("fe_vk_device_t object freed.");
}
