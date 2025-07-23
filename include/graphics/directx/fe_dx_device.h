#ifndef FE_DX_DEVICE_H
#define FE_DX_DEVICE_H

// Windows SDK'dan Direct3D 12 başlıkları
#include <d3d12.h>
#include <dxgi1_6.h> // DXGI 1.6 için (daha yeni özellikler için)
#include <wrl.h>     // Microsoft::WRL::ComPtr için (COM nesneleri yönetimi)

#include <stdbool.h>
#include <stdint.h>

// COM nesneleri için ComPtr kullanımı
using namespace Microsoft::WRL;

// --- DirectX Cihaz Hata Kodları ---
typedef enum fe_dx_device_error {
    FE_DX_DEVICE_SUCCESS = 0,
    FE_DX_DEVICE_NOT_INITIALIZED,
    FE_DX_DEVICE_FACTORY_CREATION_FAILED,     // DXGI Fabrikası oluşturma hatası
    FE_DX_DEVICE_NO_SUITABLE_ADAPTER,         // Uygun adaptör bulunamadı
    FE_DX_DEVICE_DEVICE_CREATION_FAILED,      // ID3D12Device oluşturma hatası
    FE_DX_DEVICE_DEBUG_LAYER_ACTIVATION_FAILED, // Debug katmanı etkinleştirme hatası
    FE_DX_DEVICE_UNKNOWN_ERROR
} fe_dx_device_error_t;

// --- FE DirectX Cihaz Yapısı (Gizli Tutaç) ---
// Bu, dahili DirectX cihaz nesnelerini kapsüller.
// Dışarıdan doğrudan erişilmez.
typedef struct fe_dx_device {
    ComPtr<IDXGIFactory4> dxgi_factory;     // DXGI Fabrikası
    ComPtr<ID3D12Device> d3d_device;        // Ana D3D12 mantıksal cihaz
    ComPtr<IDXGIAdapter1> dxgi_adapter;     // Seçilen fiziksel adaptör

    D3D_FEATURE_LEVEL feature_level;        // Cihazın desteklediği özellik seviyesi

    // Diğer gerekli D3D12 nesneleri veya bilgiler burada tutulabilir.
    // Örn: Command Queue, Command Allocator, vb.
} fe_dx_device_t;

// --- Fonksiyonlar ---

/**
 * @brief En uygun adaptörü seçer ve mantıksal D3D12 cihazını oluşturur.
 * Bu fonksiyon, DXGI fabrikasını ve adaptörleri kullanarak bir GPU seçer ve
 * bir ID3D12Device arayüzü oluşturur.
 *
 * @param enable_debug_layer Debug katmanının etkinleştirilip etkinleştirilmeyeceği.
 * @return fe_dx_device_t* Oluşturulan DirectX cihazına işaretçi, başarısız olursa NULL.
 */
fe_dx_device_t* fe_dx_device_create(bool enable_debug_layer);

/**
 * @brief DirectX cihazını yok eder ve tüm ilişkili kaynakları serbest bırakır.
 *
 * @param device Yok edilecek DirectX cihaz nesnesi.
 */
void fe_dx_device_destroy(fe_dx_device_t* device);

// --- Yardımcı Fonksiyonlar (Dahili veya Debug Amaçlı) ---

/**
 * @brief Bir adaptörün D3D12 için uygun olup olmadığını kontrol eder.
 * @param adapter Kontrol edilecek adaptör.
 * @param required_feature_level Gerekli en düşük özellik seviyesi.
 * @return bool Adaptör uygunsa true, aksi takdirde false.
 */
bool fe_dx_device_is_adapter_suitable(IDXGIAdapter1* adapter, D3D_FEATURE_LEVEL required_feature_level);

#endif // FE_DX_DEVICE_H
