#include "core/containers/fe_list.h"
#include "core/utils/fe_logger.h" // Loglama için
#include <string.h>               // memcpy için
#include <stdio.h>                // Debug çıktıları için (gerekirse)

// --- Yardımcı Fonksiyonlar (Dahili Kullanım İçin) ---

/**
 * @brief Yeni bir liste düğümü oluşturur.
 * Veriyi kopyalar ve bir sonraki düğüm işaretçisini NULL olarak ayarlar.
 *
 * @param data Düğümde depolanacak veri.
 * @param data_size Verinin bayt cinsinden boyutu.
 * @param data_copy_cb Veri kopyalama geri arama fonksiyonu. NULL ise memcpy kullanılır.
 * @return fe_list_node_t* Oluşturulan düğümün işaretçisi, hata durumunda NULL.
 */
static fe_list_node_t* fe_list_create_node(const void* data, size_t data_size, fe_data_copy_func data_copy_cb) {
    fe_list_node_t* new_node = FE_MALLOC(sizeof(fe_list_node_t), FE_MEM_TYPE_CONTAINER);
    if (!new_node) {
        FE_LOG_ERROR("Failed to allocate memory for list node.");
        return NULL;
    }
    memset(new_node, 0, sizeof(fe_list_node_t));

    new_node->data = FE_MALLOC(data_size, FE_MEM_TYPE_CONTAINER);
    if (!new_node->data) {
        FE_LOG_ERROR("Failed to allocate memory for node data.");
        FE_FREE(new_node, FE_MEM_TYPE_CONTAINER);
        return NULL;
    }

    if (data_copy_cb) {
        if (!data_copy_cb(new_node->data, data, data_size)) {
            FE_LOG_ERROR("Failed to copy data using custom copy callback.");
            FE_FREE(new_node->data, FE_MEM_TYPE_CONTAINER);
            FE_FREE(new_node, FE_MEM_TYPE_CONTAINER);
            return NULL;
        }
    } else {
        memcpy(new_node->data, data, data_size);
    }
    new_node->data_size = data_size;
    new_node->next = NULL;

    return new_node;
}

/**
 * @brief Bir liste düğümünü ve varsa verisini serbest bırakır.
 *
 * @param node Serbest bırakılacak düğüm.
 * @param data_free_cb Veriyi serbest bırakmak için geri arama fonksiyonu.
 */
static void fe_list_destroy_node(fe_list_node_t* node, fe_data_free_func data_free_cb) {
    if (!node) return;

    if (node->data) {
        if (data_free_cb) {
            data_free_cb(node->data);
        } else {
            FE_FREE(node->data, FE_MEM_TYPE_CONTAINER);
        }
    }
    FE_FREE(node, FE_MEM_TYPE_CONTAINER);
}

// --- Liste Fonksiyonları Implementasyonları ---

bool fe_list_init(fe_list_t* list,
                  fe_data_free_func data_free_callback,
                  fe_compare_func data_compare_callback,
                  fe_data_copy_func data_copy_callback) {
    if (!list) {
        FE_LOG_ERROR("fe_list_init: List pointer is NULL.");
        return false;
    }
    memset(list, 0, sizeof(fe_list_t)); // Tüm alanları sıfırla

    list->head = NULL;
    list->size = 0;
    list->data_free_cb = data_free_callback;
    list->data_compare_cb = data_compare_callback;
    list->data_copy_cb = data_copy_callback;

    FE_LOG_INFO("Linked list initialized.");
    return true;
}

void fe_list_shutdown(fe_list_t* list) {
    if (!list) return;

    fe_list_clear(list); // Tüm elemanları temizle

    // Liste yapısının kendisi statik ise burada free edilmez, sadece içeriği temizlenir.
    // Eğer list dinamik olarak tahsis edildiyse, onu çağıran sorumlu olacaktır.
    memset(list, 0, sizeof(fe_list_t)); // Kapatıldıktan sonra listeyi geçersiz hale getir.
    FE_LOG_INFO("Linked list shut down.");
}

bool fe_list_prepend(fe_list_t* list, const void* data, size_t data_size) {
    if (!list || !data || data_size == 0) {
        FE_LOG_ERROR("fe_list_prepend: Invalid parameters.");
        return false;
    }

    fe_list_node_t* new_node = fe_list_create_node(data, data_size, list->data_copy_cb);
    if (!new_node) {
        return false;
    }

    new_node->next = list->head; // Yeni düğüm eski başı işaret etsin
    list->head = new_node;       // Yeni düğüm artık baş düğüm
    list->size++;

    FE_LOG_DEBUG("Prepended data to list. New size: %zu", list->size);
    return true;
}

bool fe_list_append(fe_list_t* list, const void* data, size_t data_size) {
    if (!list || !data || data_size == 0) {
        FE_LOG_ERROR("fe_list_append: Invalid parameters.");
        return false;
    }

    fe_list_node_t* new_node = fe_list_create_node(data, data_size, list->data_copy_cb);
    if (!new_node) {
        return false;
    }

    if (!list->head) {
        list->head = new_node; // Liste boşsa, yeni düğüm baş düğüm olur
    } else {
        fe_list_node_t* current = list->head;
        while (current->next) {
            current = current->next; // Listenin sonuna git
        }
        current->next = new_node; // Yeni düğümü sona ekle
    }
    list->size++;

    FE_LOG_DEBUG("Appended data to list. New size: %zu", list->size);
    return true;
}

bool fe_list_insert_at(fe_list_t* list, size_t index, const void* data, size_t data_size) {
    if (!list || !data || data_size == 0 || index > list->size) {
        FE_LOG_ERROR("fe_list_insert_at: Invalid parameters (index: %zu, size: %zu).", index, list->size);
        return false;
    }

    if (index == 0) {
        return fe_list_prepend(list, data, data_size);
    }
    // index == list->size ise, append ile aynı işi yaparız.
    // Aslında append'i burada çağırmak yerine aşağıda döngüyü kullanmak daha genel.

    fe_list_node_t* new_node = fe_list_create_node(data, data_size, list->data_copy_cb);
    if (!new_node) {
        return false;
    }

    fe_list_node_t* current = list->head;
    for (size_t i = 0; i < index - 1; ++i) { // Eklenecek konumdan önceki düğümü bul
        current = current->next;
    }

    new_node->next = current->next; // Yeni düğüm, geçerli düğümün sonraki düğümünü işaret etsin
    current->next = new_node;       // Geçerli düğüm, yeni düğümü işaret etsin
    list->size++;

    FE_LOG_DEBUG("Inserted data at index %zu. New size: %zu", index, list->size);
    return true;
}

bool fe_list_remove_head(fe_list_t* list) {
    if (!list || !list->head) {
        FE_LOG_ERROR("fe_list_remove_head: List is empty or NULL.");
        return false;
    }

    fe_list_node_t* node_to_remove = list->head;
    list->head = list->head->next; // Başı bir sonraki düğüme taşı
    fe_list_destroy_node(node_to_remove, list->data_free_cb);
    list->size--;

    FE_LOG_DEBUG("Removed head from list. New size: %zu", list->size);
    return true;
}

bool fe_list_remove_tail(fe_list_t* list) {
    if (!list || !list->head) {
        FE_LOG_ERROR("fe_list_remove_tail: List is empty or NULL.");
        return false;
    }

    if (list->size == 1) { // Tek eleman varsa
        return fe_list_remove_head(list);
    }

    fe_list_node_t* current = list->head;
    while (current->next->next) { // Son düğümden önceki düğümü bul
        current = current->next;
    }
    fe_list_node_t* node_to_remove = current->next;
    current->next = NULL; // Artık son düğüm değil
    fe_list_destroy_node(node_to_remove, list->data_free_cb);
    list->size--;

    FE_LOG_DEBUG("Removed tail from list. New size: %zu", list->size);
    return true;
}

bool fe_list_remove_at(fe_list_t* list, size_t index) {
    if (!list || !list->head || index >= list->size) {
        FE_LOG_ERROR("fe_list_remove_at: Invalid parameters (index: %zu, size: %zu).", index, list->size);
        return false;
    }

    if (index == 0) {
        return fe_list_remove_head(list);
    }

    fe_list_node_t* current = list->head;
    for (size_t i = 0; i < index - 1; ++i) { // Silinecek düğümden önceki düğümü bul
        current = current->next;
    }

    fe_list_node_t* node_to_remove = current->next;
    current->next = node_to_remove->next; // Geçerli düğüm, silinecek düğümün sonraki düğümünü işaret etsin
    fe_list_destroy_node(node_to_remove, list->data_free_cb);
    list->size--;

    FE_LOG_DEBUG("Removed data at index %zu. New size: %zu", index, list->size);
    return true;
}

bool fe_list_remove(fe_list_t* list, const void* data) {
    if (!list || !list->head || !data) {
        FE_LOG_ERROR("fe_list_remove: Invalid parameters.");
        return false;
    }
    if (!list->data_compare_cb) {
        FE_LOG_ERROR("fe_list_remove: Cannot remove, no comparison callback provided during init.");
        return false;
    }

    fe_list_node_t* current = list->head;
    fe_list_node_t* prev = NULL;

    while (current) {
        if (list->data_compare_cb(current->data, data) == 0) { // Eşleşme bulundu
            if (prev) {
                prev->next = current->next; // Önceki düğüm, geçerli düğümün sonrakini işaret etsin
            } else {
                list->head = current->next; // Baş düğüm siliniyor
            }
            fe_list_destroy_node(current, list->data_free_cb);
            list->size--;
            FE_LOG_DEBUG("Removed data from list by value. New size: %zu", list->size);
            return true;
        }
        prev = current;
        current = current->next;
    }

    FE_LOG_WARN("fe_list_remove: Data not found in list.");
    return false; // Veri bulunamadı
}

void* fe_list_peek_head(const fe_list_t* list) {
    if (!list || !list->head) {
        return NULL;
    }
    return list->head->data;
}

void* fe_list_peek_tail(const fe_list_t* list) {
    if (!list || !list->head) {
        return NULL;
    }

    if (list->size == 1) {
        return list->head->data;
    }

    fe_list_node_t* current = list->head;
    while (current->next) {
        current = current->next;
    }
    return current->data;
}

void* fe_list_get_at(const fe_list_t* list, size_t index) {
    if (!list || !list->head || index >= list->size) {
        return NULL;
    }

    fe_list_node_t* current = list->head;
    for (size_t i = 0; i < index; ++i) {
        current = current->next;
    }
    return current->data;
}

bool fe_list_contains(const fe_list_t* list, const void* data) {
    if (!list || !list->head || !data) {
        return false;
    }
    if (!list->data_compare_cb) {
        FE_LOG_ERROR("fe_list_contains: Cannot search, no comparison callback provided during init.");
        return false;
    }

    fe_list_node_t* current = list->head;
    while (current) {
        if (list->data_compare_cb(current->data, data) == 0) {
            return true;
        }
        current = current->next;
    }
    return false;
}

bool fe_list_is_empty(const fe_list_t* list) {
    return list ? (list->size == 0) : true;
}

size_t fe_list_size(const fe_list_t* list) {
    return list ? list->size : 0;
}

void fe_list_clear(fe_list_t* list) {
    if (!list) return;

    fe_list_node_t* current = list->head;
    fe_list_node_t* next_node;
    while (current) {
        next_node = current->next;
        fe_list_destroy_node(current, list->data_free_cb);
        current = next_node;
    }
    list->head = NULL;
    list->size = 0;
    FE_LOG_DEBUG("List cleared. Size: %zu", list->size);
}

fe_list_node_t* fe_list_get_iterator(const fe_list_t* list) {
    if (!list) return NULL;
    return list->head;
}

void fe_list_for_each(const fe_list_t* list, fe_list_iter_callback callback) {
    if (!list || !callback) {
        FE_LOG_WARN("fe_list_for_each: List or callback is NULL.");
        return;
    }

    fe_list_node_t* current = list->head;
    while (current) {
        callback(current->data);
        current = current->next;
    }
}
