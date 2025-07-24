#include "core/containers/fe_map.h"
#include "core/utils/fe_logger.h" // Loglama için
#include <string.h>               // memcpy için
#include <math.h>                 // ceilf için (load factor hesaplamasında)

// --- Yardımcı Fonksiyonlar (Dahili Kullanım İçin) ---

/**
 * @brief Yeni bir harita girişi (entry) oluşturur ve verileri kopyalar.
 *
 * @param key Anahtar verisi.
 * @param key_size Anahtarın boyutu.
 * @param value Değer verisi.
 * @param value_size Değerin boyutu.
 * @param key_hash Anahtarın hash değeri.
 * @param copy_key_cb Anahtar kopyalama callback'i.
 * @param copy_value_cb Değer kopyalama callback'i.
 * @return fe_map_entry_t* Oluşturulan girişin işaretçisi, hata durumunda NULL.
 */
static fe_map_entry_t* fe_map_create_entry(const void* key, size_t key_size,
                                           const void* value, size_t value_size,
                                           uint64_t key_hash,
                                           fe_data_copy_func copy_key_cb,
                                           fe_data_copy_func copy_value_cb) {
    fe_map_entry_t* entry = FE_MALLOC(sizeof(fe_map_entry_t), FE_MEM_TYPE_CONTAINER);
    if (!entry) {
        FE_LOG_ERROR("fe_map_create_entry: Failed to allocate memory for map entry.");
        return NULL;
    }
    memset(entry, 0, sizeof(fe_map_entry_t));

    // Anahtarı kopyala
    entry->key = FE_MALLOC(key_size, FE_MEM_TYPE_CONTAINER);
    if (!entry->key) {
        FE_LOG_ERROR("fe_map_create_entry: Failed to allocate memory for key.");
        FE_FREE(entry, FE_MEM_TYPE_CONTAINER);
        return NULL;
    }
    if (copy_key_cb) {
        if (!copy_key_cb(entry->key, key, key_size)) {
            FE_LOG_ERROR("fe_map_create_entry: Custom key copy failed.");
            FE_FREE(entry->key, FE_MEM_TYPE_CONTAINER);
            FE_FREE(entry, FE_MEM_TYPE_CONTAINER);
            return NULL;
        }
    } else {
        memcpy(entry->key, key, key_size);
    }
    entry->key_size = key_size;

    // Değeri kopyala
    entry->value = FE_MALLOC(value_size, FE_MEM_TYPE_CONTAINER);
    if (!entry->value) {
        FE_LOG_ERROR("fe_map_create_entry: Failed to allocate memory for value.");
        FE_FREE(entry->key, FE_MEM_TYPE_CONTAINER);
        FE_FREE(entry, FE_MEM_TYPE_CONTAINER);
        return NULL;
    }
    if (copy_value_cb) {
        if (!copy_value_cb(entry->value, value, value_size)) {
            FE_LOG_ERROR("fe_map_create_entry: Custom value copy failed.");
            FE_FREE(entry->value, FE_MEM_TYPE_CONTAINER);
            FE_FREE(entry->key, FE_MEM_TYPE_CONTAINER);
            FE_FREE(entry, FE_MEM_TYPE_CONTAINER);
            return NULL;
        }
    } else {
        memcpy(entry->value, value, value_size);
    }
    entry->value_size = value_size;
    entry->key_hash = key_hash;

    return entry;
}

/**
 * @brief Bir harita girişini ve varsa içerdiği anahtar/değer verilerini serbest bırakır.
 *
 * @param entry Serbest bırakılacak giriş.
 * @param free_key_cb Anahtar için serbest bırakma callback'i.
 * @param free_value_cb Değer için serbest bırakma callback'i.
 */
static void fe_map_destroy_entry(fe_map_entry_t* entry,
                                 fe_data_free_func free_key_cb,
                                 fe_data_free_func free_value_cb) {
    if (!entry) return;

    if (entry->key) {
        if (free_key_cb) {
            free_key_cb(entry->key);
        } else {
            FE_FREE(entry->key, FE_MEM_TYPE_CONTAINER);
        }
    }

    if (entry->value) {
        if (free_value_cb) {
            free_value_cb(entry->value);
        } else {
            FE_FREE(entry->value, FE_MEM_TYPE_CONTAINER);
        }
    }
    FE_FREE(entry, FE_MEM_TYPE_CONTAINER);
}

/**
 * @brief Haritayı yeniden boyutlandırır (rehashing).
 * Tüm mevcut elemanları yeni, daha büyük bir kova dizisine yeniden dağıtır.
 *
 * @param map Yeniden boyutlandırılacak harita.
 * @return bool Başarılı ise true, aksi takdirde false.
 */
static bool fe_map_resize(fe_map_t* map) {
    if (!map) return false;

    size_t old_capacity = map->capacity;
    size_t new_capacity = old_capacity * 2; // Basitçe kapasiteyi ikiye katla
    if (new_capacity < 8) new_capacity = 8; // Minimum kapasiteyi koru

    FE_LOG_INFO("Resizing map from %zu to %zu capacity.", old_capacity, new_capacity);

    fe_list_t* new_buckets = FE_MALLOC(sizeof(fe_list_t) * new_capacity, FE_MEM_TYPE_CONTAINER);
    if (!new_buckets) {
        FE_LOG_ERROR("fe_map_resize: Failed to allocate new buckets array.");
        return false;
    }

    // Yeni kovaları başlat
    for (size_t i = 0; i < new_capacity; ++i) {
        // Liste düğümlerini yok eden özel bir serbest bırakma işlevi sağlıyoruz.
        // fe_list_destroy_node zaten fe_map_entry_t*'yi serbest bırakacak.
        fe_list_init(&new_buckets[i], NULL, NULL, NULL); 
    }

    // Eski kovaları dolaş ve elemanları yeni kovalara rehashing yap
    for (size_t i = 0; i < old_capacity; ++i) {
        fe_list_node_t* current_node = fe_list_get_iterator(&map->buckets[i]);
        while (current_node) {
            fe_map_entry_t* entry = (fe_map_entry_t*)current_node->data;
            
            // Yeni indeksi hesapla
            size_t new_index = entry->key_hash % new_capacity;
            
            // Mevcut düğümü doğrudan yeni listeye ekle (veriyi yeniden kopyalamadan)
            // Bu, fe_list'in append/prepend'ini kullanmak yerine doğrudan düğüm bağlantısını değiştirir.
            // Bu durumda, fe_list'in dahili belleğini bypass ediyoruz, bu yüzden dikkatli olun.
            // Daha sağlam bir yaklaşım, entry->key ve entry->value'yu kopyalamaktır.
            // Basitlik ve performans için burada düğümü taşıyoruz.
            
            fe_list_node_t* next_old_node = current_node->next;
            
            // Düğümü eski listeden kaldır
            // Bu, fe_list_remove_at veya fe_list_remove_head gibi bir mekanizma gerektirir.
            // Ancak şu anki fe_list implementasyonumuzda bunu doğrudan yapmak karmaşık.
            // En basit yol: eski listeyi temizle ve yeni listeye yeniden ekle.
            // Ancak bu, data_copy_cb'ye ihtiyaç duyar.
            
            // Yeniden ekleme stratejisi:
            // Her girişi çıkar, yeni kova dizisine ekle.
            // Bu, her anahtar/değer için yeniden bellek tahsisatı ve kopyalama anlamına gelir.
            // Daha verimli olan, düğümleri doğrudan taşımaktır.
            // Mevcut fe_list yapımız bunu doğrudan desteklemediği için
            // bu örnekte yeniden ekleme yolunu seçelim, ancak bunun bir performans bedeli var.

            // Mevcut anahtar ve değerleri kopyala (derin kopya)
            fe_map_set(&new_buckets[new_index], entry->key, entry->key_size, entry->value, entry->value_size);
            
            current_node = next_old_node;
        }
    }
    
    // Eski kovaları ve içindeki düğümleri temizle (düğüm verisi taşındığı için sadece düğüm bellekleri temizlenir)
    for (size_t i = 0; i < old_capacity; ++i) {
        fe_list_shutdown(&map->buckets[i]); // Sadece liste yapısını kapatır, düğüm içeriğini serbest bırakmaz.
                                            // Çünkü düğüm verileri (entryler) yeni listeye taşındı.
                                            // TODO: fe_list_shutdown'ın derin temizleme yapıp yapmadığını kontrol et.
                                            // Eğer yapıyorsa, buradaki yaklaşım yanlış olabilir.
                                            // fe_list_shutdown, node->data'yı da serbest bırakır.
                                            // Bu durumda, entryleri tek tek taşımalıyız.

        // Revize Edilmiş Rehashing Stratejisi:
        // Her elemanı tek tek çıkar, yeni kovaya ekle.
        // Bu, kopyalama geri arama fonksiyonları gerektirir.
        // fe_list_init için NULL free_cb ile başlattığımız kovalar, fe_map_entry_t*'yi serbest bırakmamalı.
    }
    
    // Eski kova dizisini serbest bırak
    FE_FREE(map->buckets, FE_MEM_TYPE_CONTAINER);

    map->buckets = new_buckets;
    map->capacity = new_capacity;
    
    FE_LOG_INFO("Map resized successfully to %zu capacity.", map->capacity);
    return true;
}

// --- Hash Haritası Fonksiyonları Implementasyonları ---

bool fe_map_init(fe_map_t* map,
                 size_t initial_capacity,
                 fe_hash_func hash_key_callback,
                 fe_compare_func compare_key_callback,
                 fe_data_free_func free_key_callback,
                 fe_data_free_func free_value_callback,
                 fe_data_copy_func copy_key_callback,
                 fe_data_copy_func copy_value_callback) {
    if (!map || !hash_key_callback || !compare_key_callback) {
        FE_LOG_ERROR("fe_map_init: Invalid parameters (map, hash_key_callback, or compare_key_callback is NULL).");
        return false;
    }
    if (initial_capacity == 0) {
        initial_capacity = 8; // Minimum başlangıç kapasitesi
        FE_LOG_WARN("fe_map_init: Initial capacity was 0, defaulting to 8.");
    }

    memset(map, 0, sizeof(fe_map_t));

    map->capacity = initial_capacity;
    map->size = 0;
    map->load_factor_threshold = 0.75f; // Yaygın bir varsayılan yük faktörü

    map->hash_key_cb = hash_key_callback;
    map->compare_key_cb = compare_key_callback;
    map->free_key_cb = free_key_callback;
    map->free_value_cb = free_value_callback;
    map->copy_key_cb = copy_key_callback;
    map->copy_value_cb = copy_value_callback;

    map->buckets = FE_MALLOC(sizeof(fe_list_t) * map->capacity, FE_MEM_TYPE_CONTAINER);
    if (!map->buckets) {
        FE_LOG_ERROR("fe_map_init: Failed to allocate memory for buckets array.");
        return false;
    }

    // Her kovayı bir bağlantılı liste olarak başlat
    for (size_t i = 0; i < map->capacity; ++i) {
        // fe_map_entry_t* olarak saklandığı için, fe_list'in kendi elemanlarını serbest bırakmasını istemiyoruz.
        // fe_map_entry_t*'nin serbest bırakılması fe_map_destroy_entry tarafından yönetilecek.
        // Ancak fe_list_node_t.data pointer'ı doğrudan fe_map_entry_t* olduğu için,
        // fe_list_node_t'yi serbest bırakırken, node->data serbest bırakılmamalı.
        // Bu yüzden NULL free_cb kullanıyoruz.
        if (!fe_list_init(&map->buckets[i], NULL, NULL, NULL)) { 
            FE_LOG_ERROR("fe_map_init: Failed to initialize bucket list at index %zu.", i);
            // Başarısız durumda önceki tahsisleri temizle
            for (size_t j = 0; j < i; ++j) {
                fe_list_shutdown(&map->buckets[j]);
            }
            FE_FREE(map->buckets, FE_MEM_TYPE_CONTAINER);
            return false;
        }
    }

    FE_LOG_INFO("Hash map initialized with capacity %zu.", map->capacity);
    return true;
}

void fe_map_shutdown(fe_map_t* map) {
    if (!map) return;

    fe_map_clear(map); // Tüm elemanları temizle

    // Kovaları kapat ve serbest bırak
    for (size_t i = 0; i < map->capacity; ++i) {
        fe_list_shutdown(&map->buckets[i]);
    }
    FE_FREE(map->buckets, FE_MEM_TYPE_CONTAINER);

    memset(map, 0, sizeof(fe_map_t)); // Harita yapısını sıfırla
    FE_LOG_INFO("Hash map shut down.");
}

bool fe_map_set(fe_map_t* map, const void* key, size_t key_size, const void* value, size_t value_size) {
    if (!map || !key || key_size == 0 || !value || value_size == 0) {
        FE_LOG_ERROR("fe_map_set: Invalid parameters.");
        return false;
    }

    // Yük faktörünü kontrol et ve gerekiyorsa yeniden boyutlandır
    if ((float)(map->size + 1) / map->capacity > map->load_factor_threshold) {
        if (!fe_map_resize(map)) {
            FE_LOG_ERROR("fe_map_set: Failed to resize map during insertion.");
            return false; // Yeniden boyutlandırma hatası
        }
    }

    uint64_t key_hash = map->hash_key_cb(key, key_size);
    size_t bucket_index = key_hash % map->capacity;

    fe_list_t* bucket = &map->buckets[bucket_index];

    // Kovayı dolaş, mevcut anahtar varsa değerini güncelle
    fe_list_node_t* current_node = fe_list_get_iterator(bucket);
    while (current_node) {
        fe_map_entry_t* existing_entry = (fe_map_entry_t*)current_node->data;
        if (existing_entry->key_hash == key_hash &&
            map->compare_key_cb(existing_entry->key, key) == 0) {
            // Anahtar bulundu, değeri güncelle
            FE_LOG_DEBUG("fe_map_set: Updating existing key. Key Hash: %llu", key_hash);
            
            // Eski değeri serbest bırak (eğer free_value_cb varsa)
            if (existing_entry->value) {
                if (map->free_value_cb) {
                    map->free_value_cb(existing_entry->value);
                } else {
                    FE_FREE(existing_entry->value, FE_MEM_TYPE_CONTAINER);
                }
            }
            
            // Yeni değeri kopyala
            existing_entry->value = FE_MALLOC(value_size, FE_MEM_TYPE_CONTAINER);
            if (!existing_entry->value) {
                FE_LOG_ERROR("fe_map_set: Failed to reallocate memory for updated value.");
                return false;
            }
            if (map->copy_value_cb) {
                if (!map->copy_value_cb(existing_entry->value, value, value_size)) {
                    FE_LOG_ERROR("fe_map_set: Custom value copy failed for update.");
                    FE_FREE(existing_entry->value, FE_MEM_TYPE_CONTAINER);
                    return false;
                }
            } else {
                memcpy(existing_entry->value, value, value_size);
            }
            existing_entry->value_size = value_size;
            return true;
        }
        current_node = current_node->next;
    }

    // Anahtar bulunamadı, yeni bir giriş ekle
    fe_map_entry_t* new_entry = fe_map_create_entry(key, key_size, value, value_size, key_hash, map->copy_key_cb, map->copy_value_cb);
    if (!new_entry) {
        return false;
    }

    // Entry'yi kovadaki listeye ekle.
    // fe_list'in dahili copy_cb'si (bizim fe_map_create_entry'mizin tersine)
    // fe_map_entry_t* adresini kopyalayacak, kendisini değil. Bu yüzden fe_list_append'e NULL veriyoruz.
    if (!fe_list_append(bucket, new_entry, sizeof(fe_map_entry_t*))) { // fe_map_entry_t*'nin boyutu
        FE_LOG_ERROR("fe_map_set: Failed to append entry to bucket list.");
        fe_map_destroy_entry(new_entry, map->free_key_cb, map->free_value_cb); // Başarısız olursa serbest bırak
        return false;
    }
    
    map->size++;
    FE_LOG_DEBUG("fe_map_set: Added new entry. Total size: %zu, Capacity: %zu", map->size, map->capacity);
    return true;
}

bool fe_map_get(const fe_map_t* map, const void* key, size_t key_size, void** out_value, size_t* out_value_size) {
    if (!map || !key || key_size == 0) {
        FE_LOG_ERROR("fe_map_get: Invalid parameters.");
        return false;
    }
    if (map->size == 0) {
        return false; // Harita boş
    }

    uint64_t key_hash = map->hash_key_cb(key, key_size);
    size_t bucket_index = key_hash % map->capacity;

    fe_list_t* bucket = &map->buckets[bucket_index];
    fe_list_node_t* current_node = fe_list_get_iterator(bucket);

    while (current_node) {
        fe_map_entry_t* entry = (fe_map_entry_t*)current_node->data;
        if (entry->key_hash == key_hash &&
            map->compare_key_cb(entry->key, key) == 0) {
            // Anahtar bulundu
            if (out_value) {
                *out_value = entry->value; // Doğrudan değeri döndür, kullanıcı bunu kopyalamalı veya kullanmalı
            }
            if (out_value_size) {
                *out_value_size = entry->value_size;
            }
            return true;
        }
        current_node = current_node->next;
    }
    FE_LOG_DEBUG("fe_map_get: Key not found. Key Hash: %llu", key_hash);
    return false; // Anahtar bulunamadı
}

bool fe_map_remove(fe_map_t* map, const void* key, size_t key_size) {
    if (!map || !key || key_size == 0 || map->size == 0) {
        FE_LOG_ERROR("fe_map_remove: Invalid parameters or map is empty.");
        return false;
    }

    uint64_t key_hash = map->hash_key_cb(key, key_size);
    size_t bucket_index = key_hash % map->capacity;

    fe_list_t* bucket = &map->buckets[bucket_index];
    
    fe_list_node_t* current_node = fe_list_get_iterator(bucket);
    fe_list_node_t* prev_node = NULL;
    int index_in_bucket = 0; // Kovadaki pozisyonu bulmak için

    while (current_node) {
        fe_map_entry_t* entry_to_check = (fe_map_entry_t*)current_node->data;
        if (entry_to_check->key_hash == key_hash &&
            map->compare_key_cb(entry_to_check->key, key) == 0) {
            // Anahtar bulundu, kaldır
            fe_map_destroy_entry(entry_to_check, map->free_key_cb, map->free_value_cb);
            
            // fe_list'ten düğümü kaldır
            // fe_list_remove_at fonksiyonu, düğüm verisinin serbest bırakılmasını yönetir.
            // Ancak biz entry'yi zaten serbest bıraktık. Bu yüzden fe_list_remove_at'in
            // düğüm verisini serbest bırakmasını engelleyecek şekilde NULL free_cb ile başlatmıştık listeyi.
            // Bu kısım, fe_list implementasyonunun nasıl yapıldığına bağlı olarak kritik.
            // Eğer fe_list_remove_at çağrıldığında, içindeki fe_list_destroy_node
            // kendi data_free_cb'ini çağırıyorsa ve bu NULL ise, sorun yok.
            // Eğer doğrudan FE_FREE(node->data) yapıyorsa, zaten bizim fe_map_entry_t*'mizi serbest bırakmış oluruz.
            // En güvenlisi, fe_list_remove_at'in bir void* döndürüp dışarıda serbest bırakmaya izin vermesidir.
            // Mevcut fe_list_remove_at bool döndürdüğü için, biz burada destroy'u yaptık ve şimdi listeyi manipüle edeceğiz.
            
            if (prev_node) {
                prev_node->next = current_node->next;
            } else {
                bucket->head = current_node->next;
            }
            FE_FREE(current_node, FE_MEM_TYPE_CONTAINER); // Düğümün kendisini serbest bırak
            bucket->size--; // Kovadaki liste boyutunu güncelle
            
            map->size--; // Haritadaki toplam eleman sayısını güncelle
            FE_LOG_DEBUG("fe_map_remove: Key removed. New map size: %zu", map->size);
            return true;
        }
        prev_node = current_node;
        current_node = current_node->next;
        index_in_bucket++;
    }

    FE_LOG_WARN("fe_map_remove: Key not found for removal. Key Hash: %llu", key_hash);
    return false; // Anahtar bulunamadı
}

bool fe_map_contains(const fe_map_t* map, const void* key, size_t key_size) {
    void* temp_val = NULL;
    size_t temp_size = 0;
    return fe_map_get(map, key, key_size, &temp_val, &temp_size); // Sadece varlık kontrolü için get'i kullan
}

size_t fe_map_size(const fe_map_t* map) {
    return map ? map->size : 0;
}

bool fe_map_is_empty(const fe_map_t* map) {
    return map ? (map->size == 0) : true;
}

void fe_map_clear(fe_map_t* map) {
    if (!map) return;

    for (size_t i = 0; i < map->capacity; ++i) {
        fe_list_t* bucket = &map->buckets[i];
        fe_list_node_t* current_node = fe_list_get_iterator(bucket);
        while (current_node) {
            fe_map_entry_t* entry = (fe_map_entry_t*)current_node->data;
            fe_map_destroy_entry(entry, map->free_key_cb, map->free_value_cb);
            current_node = current_node->next; // Bir sonraki düğüme geçmeden önce entry'yi serbest bıraktık
        }
        fe_list_clear(bucket); // Sadece düğümleri serbest bırakır (data serbest bırakılmaz, çünkü zaten biz hallettik)
    }
    map->size = 0;
    FE_LOG_DEBUG("Map cleared. Size: %zu", map->size);
}

void fe_map_for_each(const fe_map_t* map, fe_map_iter_callback callback) {
    if (!map || !callback) {
        FE_LOG_WARN("fe_map_for_each: Map or callback is NULL.");
        return;
    }

    for (size_t i = 0; i < map->capacity; ++i) {
        fe_list_t* bucket = &map->buckets[i];
        fe_list_node_t* current_node = fe_list_get_iterator(bucket);
        while (current_node) {
            fe_map_entry_t* entry = (fe_map_entry_t*)current_node->data;
            callback(entry->key, entry->key_size, entry->value, entry->value_size);
            current_node = current_node->next;
        }
    }
}


// --- Rehashing için yeniden gözden geçirilmiş ve daha güvenli fe_map_resize ---
static bool fe_map_resize(fe_map_t* map) {
    if (!map) return false;

    size_t old_capacity = map->capacity;
    size_t new_capacity = old_capacity * 2; // Basitçe kapasiteyi ikiye katla
    if (new_capacity < 8) new_capacity = 8; // Minimum kapasiteyi koru

    FE_LOG_INFO("Resizing map from %zu to %zu capacity.", old_capacity, new_capacity);

    fe_list_t* new_buckets = FE_MALLOC(sizeof(fe_list_t) * new_capacity, FE_MEM_TYPE_CONTAINER);
    if (!new_buckets) {
        FE_LOG_ERROR("fe_map_resize: Failed to allocate new buckets array.");
        return false;
    }

    // Yeni kovaları başlat, liste düğüm verisini serbest bırakmayacak şekilde
    for (size_t i = 0; i < new_capacity; ++i) {
        if (!fe_list_init(&new_buckets[i], NULL, NULL, NULL)) { 
            FE_LOG_ERROR("fe_map_resize: Failed to initialize new bucket list at index %zu.", i);
            // Hata durumunda önceden tahsis edilen yeni kovaları temizle
            for (size_t j = 0; j < i; ++j) {
                fe_list_shutdown(&new_buckets[j]);
            }
            FE_FREE(new_buckets, FE_MEM_TYPE_CONTAINER);
            return false;
        }
    }

    // Mevcut elemanları yeni kovalara taşı (rehashing)
    // Bu işlem sırasında, fe_map_entry_t'lerin kendileri yeniden tahsis edilmez, sadece listeler arası taşınır.
    size_t rehashed_count = 0;
    for (size_t i = 0; i < old_capacity; ++i) {
        // Eski kova listesinin başına ve sonuna referanslar alıyoruz
        fe_list_node_t* current_node = map->buckets[i].head;
        map->buckets[i].head = NULL; // Eski kovayı boşalt
        map->buckets[i].size = 0; // Eski kovanın boyutunu sıfırla

        while (current_node) {
            fe_map_entry_t* entry = (fe_map_entry_t*)current_node->data;
            size_t new_index = entry->key_hash % new_capacity;

            // Düğümü eski listeden ayır
            fe_list_node_t* next_old_node = current_node->next;
            current_node->next = NULL; // Düğümü bağımsız yap

            // Düğümü doğrudan yeni kova listesinin başına ekle (en hızlı)
            current_node->next = new_buckets[new_index].head;
            new_buckets[new_index].head = current_node;
            new_buckets[new_index].size++;
            
            current_node = next_old_node;
            rehashed_count++;
        }
    }

    // Eski kova dizisini serbest bırak
    for (size_t i = 0; i < old_capacity; ++i) {
        // Zaten listeleri boşalttık, sadece liste başlıklarını free ediyoruz.
        // Aslında fe_list_shutdown'ı burada çağırmaya gerek yok, çünkü head'i NULL yaptık.
        // Ama yine de güvenlik için çağrılabilir, zaten boş olduğu için bir şey yapmaz.
        fe_list_shutdown(&map->buckets[i]); 
    }
    FE_FREE(map->buckets, FE_MEM_TYPE_CONTAINER);

    map->buckets = new_buckets;
    map->capacity = new_capacity;
    
    FE_LOG_INFO("Map resized successfully. Total rehashed entries: %zu", rehashed_count);
    return true;
}
