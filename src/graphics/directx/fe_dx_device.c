#include "graphics/directx/fe_dx_device.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h" // Bellek yönetimi için

#include <vector> // Geçici adaptör listesi için

// COM hata kodlarını string'e çevirmek için basit bir makro/fonksiyon
#define HR_CHECK(hr, msg) \
    if (FAILED(hr)) { \
        FE_LOG_CRITICAL("%s HRESULT: 0x%lx", msg, (unsigned long)hr); \
        return NULL; \
    }

// --- Dahili Yardımcı Fonksiyonlar ---

// Debug Katmanını etkinleştirir
static void fe_dx_device_enable_debug_layer() {
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        FE_LOG_INFO("Direct3D 12 Debug Layer enabled.");
    } else {
        FE_LOG_WARN("Failed to enable Direct3D 12 Debug Layer. Make sure Graphics Tools are installed.");
    }
}

// Bir adaptörün D3D12 için uygun olup olmadığını kontrol eder
bool fe_dx_device_is_adapter_suitable(IDXGIAdapter1* adapter, D3D_FEATURE_LEVEL required_feature_level) {
    DXGI_ADAPTER_DESC1 desc;
    adapter->GetDesc1(&desc);

    FE_LOG_INFO("Evaluating DXGI adapter: %ls (Dedicated Video Memory: %llu MB)",
                desc.Description, desc.DedicatedVideoMemory / (1024 * 1024));

    // Microsoft Basic Render Driver'ı atla (genellikle yazılım adaptörü)
    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        FE_LOG_DEBUG("  Skipping software adapter.");
        return false;
    }

    // Adaptörün istenen Feature Level'ı destekleyip desteklemediğini kontrol et
    ComPtr<ID3D12Device> test_device;
    HRESULT hr = D3D12CreateDevice(
        adapter,
        required_feature_level,
        IID_PPV_ARGS(&test_device)
    );

    if (SUCCEEDED(hr)) {
        FE_LOG_INFO("  Adapter '%ls' is suitable (Supports Feature Level %d.%d).",
                    desc.Description,
                    (int)required_feature_level >> 12, // Major version
                    ((int)required_feature_level >> 8) & 0xF // Minor version
        );
        return true;
    } else {
        FE_LOG_DEBUG("  Adapter '%ls' does NOT support Feature Level %d.%d (HRESULT: 0x%lx).",
                     desc.Description,
                     (int)required_feature_level >> 12,
                     ((int)required_feature_level >> 8) & 0xF,
                     (unsigned long)hr);
        return false;
    }
}

// --- Ana Cihaz Yönetim Fonksiyonları Uygulaması ---

fe_dx_device_t* fe_dx_device_create(bool enable_debug_layer) {
    fe_dx_device_t* dx_device = fe_malloc(sizeof(fe_dx_device_t), FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    if (!dx_device) {
        FE_LOG_CRITICAL("Failed to allocate memory for fe_dx_device_t.");
        return NULL;
    }
    // ComPtr'lar otomatik olarak nullptr'a initialize olur, ancak struct'ı sıfırlamak yine de iyi bir pratik.
    memset(dx_device, 0, sizeof(fe_dx_device_t));

#ifdef FE_DEBUG_BUILD
    if (enable_debug_layer) {
        fe_dx_device_enable_debug_layer();
    }
#endif

    // 1. DXGI Fabrikasını Oluştur (IDXGIFactory4)
    UINT create_factory_flags = 0;
#ifdef FE_DEBUG_BUILD
    if (enable_debug_layer) {
        create_factory_flags = DXGI_CREATE_FACTORY_DEBUG;
    }
#endif
    HRESULT hr = CreateDXGIFactory2(create_factory_flags, IID_PPV_ARGS(&dx_device->dxgi_factory));
    HR_CHECK(hr, "Failed to create DXGI factory!");

    // 2. En uygun adaptörü seç
    // Tercih edilen özellik seviyesi (örneğin, D3D_FEATURE_LEVEL_11_0 veya D3D_FEATURE_LEVEL_12_0)
    // Motorunuzun desteklediği en düşük seviyeyi burada belirtmelisiniz.
    D3D_FEATURE_LEVEL required_feature_level = D3D_FEATURE_LEVEL_11_0; // Varsayılan olarak DX11 seviyesi
    // D3D_FEATURE_LEVEL_12_0; // Eğer DX12 özelikleri kullanılacaksa

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; dx_device->dxgi_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        if (fe_dx_device_is_adapter_suitable(adapter.Get(), required_feature_level)) {
            dx_device->dxgi_adapter = adapter;
            dx_device->feature_level = required_feature_level; // Seçilen adaptör bu seviyeyi desteklediği için set et
            FE_LOG_INFO("Selected DXGI adapter.");
            break;
        }
        adapter.Reset(); // Bir sonraki adaptöre geçmek için önceki adaptörü serbest bırak
    }

    if (dx_device->dxgi_adapter == nullptr) {
        FE_LOG_CRITICAL("Failed to find a suitable DXGI adapter!");
        fe_dx_device_destroy(dx_device); // Oluşturulan fabrikayı temizle
        return NULL;
    }

    // 3. Mantıksal D3D12 Cihazını Oluştur (ID3D12Device)
    hr = D3D12CreateDevice(
        dx_device->dxgi_adapter.Get(),
        dx_device->feature_level, // Seçilen feature level'ı kullan
        IID_PPV_ARGS(&dx_device->d3d_device)
    );
    HR_CHECK(hr, "Failed to create D3D12 device!");

    // Cihazın aslında desteklediği feature level'ı al (eğer required_feature_level daha yüksek bir adaptörde başarılı olursa)
    // Bu, istenen Feature Level ile oluşturulan cihazın desteklediği gerçek Feature Level'ı karşılaştırmak için faydalıdır.
     dx_device->feature_level = dx_device->d3d_device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, ...); // Bu karmaşık, şimdilik atlandı.
    // Genellikle `D3D12CreateDevice` çağrısı zaten en yüksek desteklenen seviyeyi dener veya belirtilen seviyeyi geçer.

    FE_LOG_INFO("DirectX 12 logical device created successfully (Feature Level: %d.%d).",
                (int)dx_device->feature_level >> 12,
                ((int)dx_device->feature_level >> 8) & 0xF);

    return dx_device;
}

void fe_dx_device_destroy(fe_dx_device_t* device) {
    if (!device) {
        FE_LOG_WARN("Attempted to destroy NULL fe_dx_device_t.");
        return;
    }

    // ComPtr'lar kapsam dışına çıktığında otomatik olarak Release() çağırır.
    // Ancak açıkça NULL'a set etmek de netlik sağlar.
    device->d3d_device.Reset();
    device->dxgi_adapter.Reset();
    device->dxgi_factory.Reset();

    fe_free(device, FE_MEM_TYPE_GRAPHICS, __FILE__, __LINE__);
    FE_LOG_INFO("fe_dx_device_t object freed.");
}
