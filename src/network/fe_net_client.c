#include "network/fe_net_client.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h"
#include "core/containers/fe_buffer.h"
#include "core/containers/fe_array.h" // For resolved_addresses cleanup
#include "core/utils/fe_string.h"    // For fe_string_t management

// --- Dahili Yardımcı Fonksiyonlar ---

// Hata durumuna geçer ve geri çağırmayı tetikler
static void fe_net_client_set_error_state(fe_net_client_t* client, fe_net_error_t error_code) {
    if (client->state == FE_CLIENT_STATE_ERROR) {
        return; // Zaten hata durumundaysak tekrar tetikleme
    }
    FE_LOG_ERROR("Network client entered error state: %d", error_code);
    client->state = FE_CLIENT_STATE_ERROR;
    if (client->on_error_callback) {
        client->on_error_callback(client->user_data, error_code);
    }
    // Hata durumunda bağlantıyı kesmeye çalış
    fe_net_client_disconnect(client);
}

// Tamamen bağlantıyı keser ve kaynakları temizler (callback tetiklemez, bu harici aramalarda kullanılır)
static void fe_net_client_cleanup_connection(fe_net_client_t* client) {
    if (client->tcp_socket) {
        fe_net_socket_destroy(client->tcp_socket);
        client->tcp_socket = NULL;
    }
    client->state = FE_CLIENT_STATE_DISCONNECTED;
    client->current_addr_index = 0;
    fe_string_destroy(&client->host_name);
    fe_string_init(&client->host_name, ""); // Reset string
    
    // Çözümlenen adresleri temizle
    if (fe_array_is_initialized(&client->resolved_addresses)) {
        for (size_t i = 0; i < fe_array_get_size(&client->resolved_addresses); ++i) {
            fe_ip_address_t* addr = (fe_ip_address_t*)fe_array_get_at(&client->resolved_addresses, i);
            fe_ip_address_destroy(addr);
        }
        fe_array_destroy(&client->resolved_addresses);
    }
    fe_array_init(&client->resolved_addresses, sizeof(fe_ip_address_t), 0, FE_MEM_TYPE_NETWORK); // Yeniden başlat
}

// --- Ağ İstemci Fonksiyonları ---

bool fe_net_client_init(fe_net_client_t* client, size_t send_buffer_capacity, size_t recv_buffer_capacity, void* user_data) {
    if (!client) {
        FE_LOG_ERROR("fe_net_client_init: Client pointer is NULL.");
        return false;
    }
    memset(client, 0, sizeof(fe_net_client_t));

    client->state = FE_CLIENT_STATE_DISCONNECTED;
    client->user_data = user_data;

    if (!fe_buffer_init(&client->send_buffer, send_buffer_capacity, FE_MEM_TYPE_NETWORK_BUFFER) ||
        !fe_buffer_init(&client->recv_buffer, recv_buffer_capacity, FE_MEM_TYPE_NETWORK_BUFFER)) {
        FE_LOG_CRITICAL("Failed to initialize client send/recv buffers.");
        fe_buffer_destroy(&client->send_buffer); // Clean up partially initialized
        fe_buffer_destroy(&client->recv_buffer);
        return false;
    }

    fe_string_init(&client->host_name, "");
    fe_array_init(&client->resolved_addresses, sizeof(fe_ip_address_t), 0, FE_MEM_TYPE_NETWORK);

    FE_LOG_INFO("Network client initialized (Send Buffer: %zu, Recv Buffer: %zu).", send_buffer_capacity, recv_buffer_capacity);
    return true;
}

void fe_net_client_shutdown(fe_net_client_t* client) {
    if (!client) return;

    FE_LOG_INFO("Shutting down network client.");

    // Bağlantıyı kes ve kaynakları temizle
    fe_net_client_cleanup_connection(client);

    fe_buffer_destroy(&client->send_buffer);
    fe_buffer_destroy(&client->recv_buffer);
    fe_string_destroy(&client->host_name);
    // fe_array_destroy already called in cleanup_connection
    
    memset(client, 0, sizeof(fe_net_client_t)); // Clear the structure
}

fe_net_error_t fe_net_client_connect(fe_net_client_t* client, const char* host_name, uint16_t port) {
    if (!client || !host_name || port == 0) {
        FE_LOG_ERROR("Invalid arguments for fe_net_client_connect.");
        return FE_NET_INVALID_ARGUMENT;
    }

    if (client->state != FE_CLIENT_STATE_DISCONNECTED && client->state != FE_CLIENT_STATE_ERROR) {
        FE_LOG_WARN("Client is already connecting or connected. Disconnect first.");
        return FE_NET_INVALID_STATE;
    }

    // Önceki bağlantı ve adresleri temizle
    fe_net_client_cleanup_connection(client); // Bağlantıyı sıfırla

    fe_string_set(&client->host_name, host_name);
    client->port = port;
    client->state = FE_CLIENT_STATE_RESOLVING_ADDRESS;
    client->current_addr_index = 0;

    FE_LOG_INFO("Attempting to connect to %s:%hu...", host_name, port);
    
    // fe_net_init burada çağrılmıyor, uygulama genelinde bir kez çağrılması bekleniyor
    // Eğer çağrılmadıysa fe_net_resolve_address içinde hata dönecektir.

    return FE_NET_SUCCESS; // Bağlantı süreci başladı
}

fe_net_error_t fe_net_client_disconnect(fe_net_client_t* client) {
    if (!client) {
        return FE_NET_INVALID_ARGUMENT;
    }

    if (client->state == FE_CLIENT_STATE_DISCONNECTED) {
        FE_LOG_DEBUG("Client already disconnected.");
        return FE_NET_SUCCESS;
    }
    
    if (client->state == FE_CLIENT_STATE_DISCONNECTING) {
        FE_LOG_DEBUG("Client already in disconnecting state.");
        return FE_NET_SUCCESS;
    }

    FE_LOG_INFO("Initiating client disconnect.");
    client->state = FE_CLIENT_STATE_DISCONNECTING;
    // Bağlantıyı kapatmak için fe_net_socket_destroy çağrılacak ve on_disconnected callback'i tetiklenecek.
    // update döngüsü bu temizliği halledecektir.
    return FE_NET_SUCCESS;
}


void fe_net_client_update(fe_net_client_t* client) {
    if (!client) return;

    // Duruma göre işlem yap
    switch (client->state) {
        case FE_CLIENT_STATE_DISCONNECTED:
        case FE_CLIENT_STATE_ERROR:
            // Yapacak bir şey yok
            break;

        case FE_CLIENT_STATE_RESOLVING_ADDRESS: {
            // Adresi çözümle
            fe_net_error_t resolve_err = fe_net_resolve_address(
                client->host_name.data,
                client->port,
                &client->resolved_addresses,
                FE_SOCKET_TYPE_TCP
            );

            if (resolve_err != FE_NET_SUCCESS || fe_array_get_size(&client->resolved_addresses) == 0) {
                FE_LOG_ERROR("Failed to resolve address for %s:%hu. Error: %d", client->host_name.data, client->port, resolve_err);
                fe_net_client_set_error_state(client, resolve_err);
                break;
            }
            // Adres çözümlendi, bağlantı durumuna geç
            client->state = FE_CLIENT_STATE_CONNECTING;
            // Düşme: FE_CLIENT_STATE_CONNECTING durumuna geçer.
        }
        // No break here, fall through to connecting to start immediately.

        case FE_CLIENT_STATE_CONNECTING: {
            if (!client->tcp_socket) {
                // Yeni bir soket oluştur veya bir önceki bağlantı girişimi başarısız olduysa
                if (client->current_addr_index >= fe_array_get_size(&client->resolved_addresses)) {
                    FE_LOG_ERROR("All resolved addresses failed to connect.");
                    fe_net_client_set_error_state(client, FE_NET_CONNECT_FAILED);
                    break;
                }

                client->tcp_socket = fe_net_socket_create(FE_SOCKET_TYPE_TCP, false); // Bloklamayan soket
                if (!client->tcp_socket) {
                    FE_LOG_ERROR("Failed to create TCP socket for connection attempt.");
                    fe_net_client_set_error_state(client, FE_NET_SOCKET_CREATE_FAILED);
                    break;
                }

                fe_ip_address_t* target_addr = (fe_ip_address_t*)fe_array_get_at(&client->resolved_addresses, client->current_addr_index);
                FE_LOG_DEBUG("Attempting to connect to %s:%hu (address %zu/%zu)...",
                             target_addr->ip_string.data, target_addr->port,
                             client->current_addr_index + 1, fe_array_get_size(&client->resolved_addresses));

                fe_net_error_t connect_err = fe_net_socket_connect(client->tcp_socket, target_addr);
                if (connect_err == FE_NET_WOULD_BLOCK) {
                    // Bağlantı devam ediyor
                    return; // Bir sonraki karede tekrar kontrol et
                } else if (connect_err != FE_NET_SUCCESS) {
                    FE_LOG_WARN("Connection attempt to %s:%hu failed. Trying next address (if any). Error: %d",
                                target_addr->ip_string.data, target_addr->port, connect_err);
                    fe_net_socket_destroy(client->tcp_socket); // Mevcut soketi kapat
                    client->tcp_socket = NULL;
                    client->current_addr_index++; // Bir sonraki adrese geç
                    return; // Bir sonraki karede tekrar dene
                } else {
                    // Anında bağlandı (bloklayan veya çok hızlı non-blocking)
                    client->state = FE_CLIENT_STATE_CONNECTED;
                    FE_LOG_INFO("Client connected to %s:%hu.", target_addr->ip_string.data, target_addr->port);
                    if (client->on_connected_callback) {
                        client->on_connected_callback(client->user_data);
                    }
                    // Çözümlenen adresleri temizle (artık gerek yok)
                    for (size_t i = 0; i < fe_array_get_size(&client->resolved_addresses); ++i) {
                        fe_ip_address_t* addr = (fe_ip_address_t*)fe_array_get_at(&client->resolved_addresses, i);
                        fe_ip_address_destroy(addr);
                    }
                    fe_array_clear(&client->resolved_addresses);
                    // Düşme: Bağlandıktan sonra veri gönderme/alma durumuna geç.
                }
            } else {
                // tcp_socket zaten var, bağlantının tamamlanıp tamamlanmadığını kontrol et
                fe_net_error_t check_err = fe_net_socket_check_connect(client->tcp_socket);
                if (check_err == FE_NET_WOULD_BLOCK) {
                    // Hala devam ediyor
                    return;
                } else if (check_err != FE_NET_SUCCESS) {
                    FE_LOG_WARN("Non-blocking connection to %s:%hu failed. Trying next address (if any). Error: %d",
                                client->tcp_socket->remote_addr.ip_string.data, client->tcp_socket->remote_addr.port, check_err);
                    fe_net_socket_destroy(client->tcp_socket);
                    client->tcp_socket = NULL;
                    client->current_addr_index++; // Bir sonraki adrese geç
                    return; // Bir sonraki karede tekrar dene
                } else {
                    // Bağlantı tamamlandı
                    client->state = FE_CLIENT_STATE_CONNECTED;
                    FE_LOG_INFO("Client connected to %s:%hu.", client->tcp_socket->remote_addr.ip_string.data, client->tcp_socket->remote_addr.port);
                    if (client->on_connected_callback) {
                        client->on_connected_callback(client->user_data);
                    }
                    // Çözümlenen adresleri temizle
                    for (size_t i = 0; i < fe_array_get_size(&client->resolved_addresses); ++i) {
                        fe_ip_address_t* addr = (fe_ip_address_t*)fe_array_get_at(&client->resolved_addresses, i);
                        fe_ip_address_destroy(addr);
                    }
                    fe_array_clear(&client->resolved_addresses);
                    // Düşme: Bağlandıktan sonra veri gönderme/alma durumuna geç.
                }
            }
            break; // FE_CLIENT_STATE_CONNECTED durumuna geçişi kontrol eder
        }

        case FE_CLIENT_STATE_CONNECTED: {
            if (!client->tcp_socket || !client->tcp_socket->is_connected) {
                FE_LOG_ERROR("Client in CONNECTED state but socket is invalid or not connected. Forcing disconnect.");
                fe_net_client_set_error_state(client, FE_NET_UNKNOWN_ERROR);
                // Düşme: disconnected'a düşebilir, ancak set_error_state zaten disconnect'i çağırır.
                return;
            }

            // 1. Giden verileri gönder
            if (fe_buffer_get_used_size(&client->send_buffer) > 0) {
                size_t bytes_to_send = fe_buffer_get_used_size(&client->send_buffer);
                size_t sent_bytes = 0;
                fe_net_error_t send_err = fe_net_socket_send(client->tcp_socket, fe_buffer_get_read_ptr(&client->send_buffer), bytes_to_send, &sent_bytes);

                if (send_err == FE_NET_SUCCESS && sent_bytes > 0) {
                    fe_buffer_read_advance(&client->send_buffer, sent_bytes);
                    // FE_LOG_TRACE("Sent %zu bytes to server. %zu bytes remaining in send buffer.", sent_bytes, fe_buffer_get_used_size(&client->send_buffer));
                } else if (send_err != FE_NET_WOULD_BLOCK) {
                    // Kritik gönderme hatası (bağlantı koptu vb.)
                    FE_LOG_ERROR("Failed to send data. Error: %d", send_err);
                    fe_net_client_set_error_state(client, send_err);
                    return;
                }
            }

            // 2. Gelen verileri al
            // Tamponda yer varsa veri almaya çalış
            if (fe_buffer_get_free_size(&client->recv_buffer) > 0) {
                size_t bytes_to_recv = fe_buffer_get_free_size(&client->recv_buffer);
                size_t received_bytes = 0;
                fe_net_error_t recv_err = fe_net_socket_receive(client->tcp_socket, fe_buffer_get_write_ptr(&client->recv_buffer), bytes_to_recv, &received_bytes);

                if (recv_err == FE_NET_SUCCESS && received_bytes > 0) {
                    fe_buffer_write_advance(&client->recv_buffer, received_bytes);
                    // FE_LOG_TRACE("Received %zu bytes from server. %zu bytes in recv buffer.", received_bytes, fe_buffer_get_used_size(&client->recv_buffer));
                    
                    // Veri alındı callback'ini tetikle
                    if (client->on_data_received_callback) {
                        // fe_buffer_read_ptr ve fe_buffer_get_used_size ile veriyi doğrudan callback'e ver.
                        // Callback'in veriyi kopyalaması veya işlemesi beklenir.
                        // Daha sonra fe_buffer_read_advance ile bu veriyi tampondan tüketmesi beklenir.
                        client->on_data_received_callback(client->user_data, fe_buffer_get_read_ptr(&client->recv_buffer), received_bytes);
                        // Callback'in kaç byte tükettiğini bilmediğimizden,
                        // burada fe_buffer_read_advance çağrılmıyor.
                        // fe_buffer_get_read_ptr ve fe_buffer_get_used_size ile veriyi alıp
                        // kendisi işleyip fe_buffer_read_advance'i çağırması gerekir.
                        // Basitlik için burada tüm alınan veriyi tüketiyoruz.
                        fe_buffer_read_advance(&client->recv_buffer, received_bytes);
                    }
                } else if (recv_err == FE_NET_SUCCESS && received_bytes == 0) {
                    // Bağlantı uzaktan kapatıldı
                    FE_LOG_INFO("Server gracefully disconnected client %d.", (int)client->tcp_socket->handle);
                    fe_net_client_set_error_state(client, FE_NET_SUCCESS); // Başarılı disconnect olarak işaretle
                    // Düşme: Disconnecting durumuna geç.
                } else if (recv_err != FE_NET_NO_DATA && recv_err != FE_NET_WOULD_BLOCK) {
                    // Kritik alma hatası (bağlantı koptu vb.)
                    FE_LOG_ERROR("Failed to receive data. Error: %d", recv_err);
                    fe_net_client_set_error_state(client, recv_err);
                    return;
                }
            }
            break;
        }

        case FE_CLIENT_STATE_DISCONNECTING: {
            if (client->tcp_socket) {
                // Eğer bağlantı hala açıksa kapat
                fe_net_socket_destroy(client->tcp_socket);
                client->tcp_socket = NULL;
            }
            
            client->state = FE_CLIENT_STATE_DISCONNECTED;
            FE_LOG_INFO("Client fully disconnected.");
            if (client->on_disconnected_callback) {
                // Disconnect isteğe bağlı olduğundan, başarılı bir disconnect sayılır.
                client->on_disconnected_callback(client->user_data, FE_NET_SUCCESS);
            }
            // Kaynakları temizle (resolved_addresses vb.)
            fe_net_client_cleanup_connection(client);
            break;
        }
    }
}

bool fe_net_client_send_data(fe_net_client_t* client, const void* data, size_t size) {
    if (!client || !data || size == 0) {
        FE_LOG_ERROR("Invalid arguments for fe_net_client_send_data.");
        return false;
    }
    if (client->state != FE_CLIENT_STATE_CONNECTED && client->state != FE_CLIENT_STATE_CONNECTING) { // Connecting iken de kuyruğa alabiliriz
        FE_LOG_WARN("Cannot send data: client is not connected or connecting (State: %d).", client->state);
        return false;
    }

    if (fe_buffer_get_free_size(&client->send_buffer) < size) {
        FE_LOG_WARN("Send buffer full. Failed to queue %zu bytes.", size);
        return false;
    }

    fe_buffer_write(&client->send_buffer, data, size);
    // FE_LOG_DEBUG("Queued %zu bytes for sending. Send buffer used: %zu.", size, fe_buffer_get_used_size(&client->send_buffer));
    return true;
}

fe_client_state_t fe_net_client_get_state(const fe_net_client_t* client) {
    if (!client) {
        return FE_CLIENT_STATE_ERROR; // Hatalı kullanım
    }
    return client->state;
}

const fe_ip_address_t* fe_net_client_get_remote_address(const fe_net_client_t* client) {
    if (!client || client->state != FE_CLIENT_STATE_CONNECTED || !client->tcp_socket) {
        return NULL;
    }
    return &client->tcp_socket->remote_addr;
}

// --- Geri Çağırma Ayarlayıcıları ---
void fe_net_client_set_on_connected_callback(fe_net_client_t* client, PFN_fe_client_on_connected callback) {
    if (client) client->on_connected_callback = callback;
}
void fe_net_client_set_on_disconnected_callback(fe_net_client_t* client, PFN_fe_client_on_disconnected callback) {
    if (client) client->on_disconnected_callback = callback;
}
void fe_net_client_set_on_data_received_callback(fe_net_client_t* client, PFN_fe_client_on_data_received callback) {
    if (client) client->on_data_received_callback = callback;
}
void fe_net_client_set_on_error_callback(fe_net_client_t* client, PFN_fe_client_on_error callback) {
    if (client) client->on_error_callback = callback;
}
