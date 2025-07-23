#include "network/fe_net_server.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h"
#include "core/containers/fe_buffer.h"
#include "core/containers/fe_array.h" // fe_array_add_element, fe_array_remove_at

// --- Dahili Yardımcı Fonksiyonlar ---

// Hata durumuna geçer ve geri çağırmayı tetikler
static void fe_net_server_set_error_state(fe_net_server_t* server, fe_net_error_t error_code) {
    if (server->state == FE_SERVER_STATE_ERROR) {
        return; // Zaten hata durumundaysak tekrar tetikleme
    }
    FE_LOG_ERROR("Network server entered error state: %d", error_code);
    server->state = FE_SERVER_STATE_ERROR;
    if (server->on_error_callback) {
        server->on_error_callback(server->user_data, error_code);
    }
    // Hata durumunda sunucuyu durdurmaya çalış
    fe_net_server_stop(server);
}

// Belirli bir istemciyi bulmak için yardımcı fonksiyon
static fe_connected_client_t* fe_net_server_get_client_by_id(fe_net_server_t* server, fe_client_id_t client_id) {
    for (size_t i = 0; i < fe_array_get_size(&server->connected_clients); ++i) {
        fe_connected_client_t* client = (fe_connected_client_t*)fe_array_get_at(&server->connected_clients, i);
        if (client->id == client_id) {
            return client;
        }
    }
    return NULL;
}

// İstemci kaynaklarını temizler (socket, buffers, user_data)
static void fe_net_server_cleanup_client(fe_connected_client_t* client) {
    if (client->socket) {
        fe_net_socket_destroy(client->socket);
        client->socket = NULL;
    }
    fe_buffer_destroy(&client->send_buffer);
    fe_buffer_destroy(&client->recv_buffer);
    // user_data'nın yönetimi uygulamanın sorumluluğundadır, burada serbest bırakılmaz
    memset(client, 0, sizeof(fe_connected_client_t)); // Yapıyı sıfırla
    client->id = FE_INVALID_CLIENT_ID;
}

// --- Ağ Sunucusu Fonksiyonları ---

bool fe_net_server_init(fe_net_server_t* server, size_t client_send_buffer_capacity, size_t client_recv_buffer_capacity, size_t max_clients, void* user_data) {
    if (!server || max_clients == 0) {
        FE_LOG_ERROR("fe_net_server_init: Invalid arguments (server NULL or max_clients 0).");
        return false;
    }
    memset(server, 0, sizeof(fe_net_server_t));

    server->state = FE_SERVER_STATE_STOPPED;
    server->user_data = user_data;
    server->next_client_id = 1; // 0 (FE_INVALID_CLIENT_ID) kullanılmayacak
    server->client_send_buffer_capacity = client_send_buffer_capacity;
    server->client_recv_buffer_capacity = client_recv_buffer_capacity;

    // Bağlı istemciler için dinamik dizi oluştur
    if (!fe_array_init(&server->connected_clients, sizeof(fe_connected_client_t), max_clients, FE_MEM_TYPE_NETWORK)) {
        FE_LOG_CRITICAL("Failed to initialize server connected_clients array.");
        return false;
    }
    fe_array_set_capacity(&server->connected_clients, max_clients); // Max clients ile kapasiteyi ayarla

    FE_LOG_INFO("Network server initialized (Max Clients: %zu, Client Send Buffer: %zu, Recv Buffer: %zu).",
                max_clients, client_send_buffer_capacity, client_recv_buffer_capacity);
    return true;
}

void fe_net_server_shutdown(fe_net_server_t* server) {
    if (!server) return;

    FE_LOG_INFO("Shutting down network server.");

    // Sunucuyu durdur (dinlemeyi bırak ve tüm istemcilerin bağlantısını kes)
    fe_net_server_stop(server); // Bu zaten iç temizliği halleder

    // Array içindeki tüm istemci kaynaklarını temizle
    for (size_t i = 0; i < fe_array_get_size(&server->connected_clients); ++i) {
        fe_connected_client_t* client = (fe_connected_client_t*)fe_array_get_at(&server->connected_clients, i);
        fe_net_server_cleanup_client(client);
    }
    fe_array_destroy(&server->connected_clients); // Dinamik diziyi serbest bırak

    memset(server, 0, sizeof(fe_net_server_t)); // Yapıyı sıfırla
}

fe_net_error_t fe_net_server_start(fe_net_server_t* server, uint16_t port, int backlog) {
    if (!server || port == 0 || backlog <= 0) {
        FE_LOG_ERROR("Invalid arguments for fe_net_server_start.");
        return FE_NET_INVALID_ARGUMENT;
    }

    if (server->state != FE_SERVER_STATE_STOPPED && server->state != FE_SERVER_STATE_ERROR) {
        FE_LOG_WARN("Server is already running or shutting down. Stop first.");
        return FE_NET_INVALID_STATE;
    }

    server->port = port;
    server->state = FE_SERVER_STATE_STARTING;

    // Dinleme soketi oluştur
    server->listen_socket = fe_net_socket_create(FE_SOCKET_TYPE_TCP, false); // Non-blocking
    if (!server->listen_socket) {
        FE_LOG_ERROR("Failed to create listen socket.");
        fe_net_server_set_error_state(server, FE_NET_SOCKET_CREATE_FAILED);
        return FE_NET_SOCKET_CREATE_FAILED;
    }

    // Soketi porta bağla
    fe_net_error_t bind_err = fe_net_socket_bind(server->listen_socket, port);
    if (bind_err != FE_NET_SUCCESS) {
        FE_LOG_ERROR("Failed to bind socket to port %hu. Error: %d", port, bind_err);
        fe_net_server_set_error_state(server, bind_err);
        return bind_err;
    }

    // Dinlemeye başla
    fe_net_error_t listen_err = fe_net_socket_listen(server->listen_socket, backlog);
    if (listen_err != FE_NET_SUCCESS) {
        FE_LOG_ERROR("Failed to listen on socket. Error: %d", listen_err);
        fe_net_server_set_error_state(server, listen_err);
        return listen_err;
    }

    server->state = FE_SERVER_STATE_RUNNING;
    FE_LOG_INFO("Network server started and listening on port %hu.", port);
    if (server->on_started_callback) {
        server->on_started_callback(server->user_data);
    }
    return FE_NET_SUCCESS;
}

fe_net_error_t fe_net_server_stop(fe_net_server_t* server) {
    if (!server) {
        return FE_NET_INVALID_ARGUMENT;
    }

    if (server->state == FE_SERVER_STATE_STOPPED) {
        FE_LOG_DEBUG("Server already stopped.");
        return FE_NET_SUCCESS;
    }

    if (server->state == FE_SERVER_STATE_SHUTTING_DOWN) {
        FE_LOG_DEBUG("Server already in shutting down state.");
        return FE_NET_SUCCESS;
    }

    FE_LOG_INFO("Initiating server stop.");
    server->state = FE_SERVER_STATE_SHUTTING_DOWN;

    // Dinleme soketini kapat
    if (server->listen_socket) {
        fe_net_socket_destroy(server->listen_socket);
        server->listen_socket = NULL;
    }

    // Tüm bağlı istemcileri bağlantıyı kesmek üzere işaretle
    for (size_t i = 0; i < fe_array_get_size(&server->connected_clients); ++i) {
        fe_connected_client_t* client = (fe_connected_client_t*)fe_array_get_at(&server->connected_clients, i);
        client->is_pending_disconnect = true; // Update döngüsü bunları kesecektir
    }
    
    // Eğer hiç bağlı istemci yoksa, hemen durdurma callback'ini tetikle
    if (fe_array_get_size(&server->connected_clients) == 0) {
        server->state = FE_SERVER_STATE_STOPPED;
        if (server->on_stopped_callback) {
            server->on_stopped_callback(server->user_data, FE_NET_SUCCESS);
        }
    }
    // Aksi takdirde, update döngüsü istemcilerin kapanmasını bekleyecektir.

    return FE_NET_SUCCESS;
}

void fe_net_server_update(fe_net_server_t* server) {
    if (!server || server->state == FE_SERVER_STATE_STOPPED || server->state == FE_SERVER_STATE_ERROR) {
        return;
    }

    // 1. Yeni bağlantıları kabul et (Sadece RUNNING durumunda)
    if (server->state == FE_SERVER_STATE_RUNNING && server->listen_socket) {
        fe_socket_t* new_client_socket = NULL;
        fe_net_error_t accept_err = fe_net_socket_accept(server->listen_socket, &new_client_socket);

        if (accept_err == FE_NET_SUCCESS && new_client_socket) {
            if (fe_array_get_size(&server->connected_clients) >= fe_array_get_capacity(&server->connected_clients)) {
                FE_LOG_WARN("Max clients (%zu) reached. Rejecting new connection from %s:%hu.",
                            fe_array_get_capacity(&server->connected_clients),
                            new_client_socket->remote_addr.ip_string.data, new_client_socket->remote_addr.port);
                fe_net_socket_destroy(new_client_socket); // Yeni soketi hemen kapat
            } else {
                fe_connected_client_t new_client;
                memset(&new_client, 0, sizeof(fe_connected_client_t));
                new_client.id = server->next_client_id++;
                new_client.socket = new_client_socket;
                new_client.is_pending_disconnect = false;

                if (!fe_buffer_init(&new_client.send_buffer, server->client_send_buffer_capacity, FE_MEM_TYPE_NETWORK_BUFFER) ||
                    !fe_buffer_init(&new_client.recv_buffer, server->client_recv_buffer_capacity, FE_MEM_TYPE_NETWORK_BUFFER)) {
                    FE_LOG_ERROR("Failed to allocate buffers for new client (ID: %u). Disconnecting.", new_client.id);
                    fe_net_socket_destroy(new_client_socket);
                    fe_buffer_destroy(&new_client.send_buffer);
                    fe_buffer_destroy(&new_client.recv_buffer);
                } else {
                    if (fe_array_add_element(&server->connected_clients, &new_client)) {
                        FE_LOG_INFO("New client connected (ID: %u) from %s:%hu. Total clients: %zu.",
                                    new_client.id, new_client_socket->remote_addr.ip_string.data,
                                    new_client_socket->remote_addr.port, fe_array_get_size(&server->connected_clients));
                        if (server->on_client_connected_callback) {
                            server->on_client_connected_callback(server->user_data, new_client.id, &new_client_socket->remote_addr);
                        }
                    } else {
                        FE_LOG_ERROR("Failed to add new client (ID: %u) to client list. Disconnecting.", new_client.id);
                        fe_net_server_cleanup_client(&new_client); // Temizle
                    }
                }
            }
        } else if (accept_err != FE_NET_NO_DATA && accept_err != FE_NET_WOULD_BLOCK) {
            FE_LOG_ERROR("Failed to accept new connection. Error: %d", accept_err);
            fe_net_server_set_error_state(server, accept_err);
        }
    }

    // 2. Bağlı istemcileri işle (veri gönder, veri al, bağlantı kesme)
    // Ters döngü, çünkü elemanlar diziden silinebilir
    for (int i = (int)fe_array_get_size(&server->connected_clients) - 1; i >= 0; --i) {
        fe_connected_client_t* client = (fe_connected_client_t*)fe_array_get_at(&server->connected_clients, i);

        // Bağlantıyı kesme isteği varsa veya soket geçersizse
        if (client->is_pending_disconnect || !client->socket || !client->socket->is_connected) {
            fe_net_error_t disconnect_reason = FE_NET_SUCCESS;
            if (client->is_pending_disconnect && client->socket && client->socket->is_connected) {
                FE_LOG_INFO("Disconnecting client (ID: %u) as requested.", client->id);
            } else if (!client->socket || !client->socket->is_connected) {
                FE_LOG_INFO("Client (ID: %u) connection lost/closed by peer.", client->id);
                disconnect_reason = FE_NET_CONNECTION_RESET; // veya uygun bir hata kodu
            }

            if (server->on_client_disconnected_callback) {
                server->on_client_disconnected_callback(server->user_data, client->id, disconnect_reason);
            }
            fe_net_server_cleanup_client(client);
            fe_array_remove_at(&server->connected_clients, i);
            continue; // Bir sonraki istemciye geç
        }

        // a. Giden verileri gönder
        if (fe_buffer_get_used_size(&client->send_buffer) > 0) {
            size_t bytes_to_send = fe_buffer_get_used_size(&client->send_buffer);
            size_t sent_bytes = 0;
            fe_net_error_t send_err = fe_net_socket_send(client->socket, fe_buffer_get_read_ptr(&client->send_buffer), bytes_to_send, &sent_bytes);

            if (send_err == FE_NET_SUCCESS && sent_bytes > 0) {
                fe_buffer_read_advance(&client->send_buffer, sent_bytes);
                // FE_LOG_TRACE("Sent %zu bytes to client %u. %zu bytes remaining in send buffer.", sent_bytes, client->id, fe_buffer_get_used_size(&client->send_buffer));
            } else if (send_err != FE_NET_WOULD_BLOCK) {
                // Kritik gönderme hatası (bağlantı koptu vb.)
                FE_LOG_ERROR("Failed to send data to client %u. Error: %d. Marking for disconnect.", client->id, send_err);
                client->is_pending_disconnect = true; // Bu istemcinin bağlantısını kes
                continue; // Bu karede başka işlem yapma, bir sonraki döngüde kesilecek
            }
        }

        // b. Gelen verileri al
        if (fe_buffer_get_free_size(&client->recv_buffer) > 0) {
            size_t bytes_to_recv = fe_buffer_get_free_size(&client->recv_buffer);
            size_t received_bytes = 0;
            fe_net_error_t recv_err = fe_net_socket_receive(client->socket, fe_buffer_get_write_ptr(&client->recv_buffer), bytes_to_recv, &received_bytes);

            if (recv_err == FE_NET_SUCCESS && received_bytes > 0) {
                fe_buffer_write_advance(&client->recv_buffer, received_bytes);
                // FE_LOG_TRACE("Received %zu bytes from client %u. %zu bytes in recv buffer.", received_bytes, client->id, fe_buffer_get_used_size(&client->recv_buffer));

                // Veri alındı callback'ini tetikle
                if (server->on_data_received_callback) {
                    // Callback'in alınan veriyi işlemesi ve fe_buffer_read_advance ile tüketmesi beklenir.
                    // Bu örnekte, tüm alınan veriyi hemen tüketiyoruz.
                    server->on_data_received_callback(server->user_data, client->id, fe_buffer_get_read_ptr(&client->recv_buffer), received_bytes);
                    fe_buffer_read_advance(&client->recv_buffer, received_bytes); // Veriyi tampondan temizle
                }
            } else if (recv_err == FE_NET_SUCCESS && received_bytes == 0) {
                // İstemci bağlantıyı kapattı (graceful shutdown)
                FE_LOG_INFO("Client (ID: %u) gracefully disconnected.", client->id);
                client->is_pending_disconnect = true;
                continue; // Bu karede başka işlem yapma, bir sonraki döngüde kesilecek
            } else if (recv_err != FE_NET_NO_DATA && recv_err != FE_NET_WOULD_BLOCK) {
                // Kritik alma hatası (bağlantı koptu vb.)
                FE_LOG_ERROR("Failed to receive data from client %u. Error: %d. Marking for disconnect.", client->id, recv_err);
                client->is_pending_disconnect = true;
                continue; // Bu karede başka işlem yapma, bir sonraki döngüde kesilecek
            }
        }
    } // for (connected_clients)

    // 3. Kapanma durumunda tüm istemciler ayrıldıysa sunucuyu tamamen durdur
    if (server->state == FE_SERVER_STATE_SHUTTING_DOWN && fe_array_get_size(&server->connected_clients) == 0) {
        server->state = FE_SERVER_STATE_STOPPED;
        FE_LOG_INFO("Server shutdown complete.");
        if (server->on_stopped_callback) {
            server->on_stopped_callback(server->user_data, FE_NET_SUCCESS);
        }
    }
}

bool fe_net_server_send_data(fe_net_server_t* server, fe_client_id_t client_id, const void* data, size_t size) {
    if (!server || !data || size == 0 || client_id == FE_INVALID_CLIENT_ID) {
        FE_LOG_ERROR("Invalid arguments for fe_net_server_send_data.");
        return false;
    }
    if (server->state != FE_SERVER_STATE_RUNNING) {
        FE_LOG_WARN("Cannot send data: server is not running (State: %d).", server->state);
        return false;
    }

    fe_connected_client_t* client = fe_net_server_get_client_by_id(server, client_id);
    if (!client || client->is_pending_disconnect) {
        FE_LOG_WARN("Client ID %u not found or pending disconnect. Cannot send data.", client_id);
        return false;
    }

    if (fe_buffer_get_free_size(&client->send_buffer) < size) {
        FE_LOG_WARN("Send buffer full for client %u. Failed to queue %zu bytes.", client_id, size);
        return false;
    }

    fe_buffer_write(&client->send_buffer, data, size);
    // FE_LOG_DEBUG("Queued %zu bytes for client %u. Send buffer used: %zu.", size, client_id, fe_buffer_get_used_size(&client->send_buffer));
    return true;
}

bool fe_net_server_disconnect_client(fe_net_server_t* server, fe_client_id_t client_id) {
    if (!server || client_id == FE_INVALID_CLIENT_ID) {
        FE_LOG_ERROR("Invalid arguments for fe_net_server_disconnect_client.");
        return false;
    }
    if (server->state != FE_SERVER_STATE_RUNNING && server->state != FE_SERVER_STATE_SHUTTING_DOWN) {
        FE_LOG_WARN("Server is not running or shutting down. Cannot disconnect client (ID: %u).", client_id);
        return false;
    }

    fe_connected_client_t* client = fe_net_server_get_client_by_id(server, client_id);
    if (!client) {
        FE_LOG_WARN("Client ID %u not found. Cannot disconnect.", client_id);
        return false;
    }

    if (client->is_pending_disconnect) {
        FE_LOG_DEBUG("Client ID %u is already pending disconnect.", client_id);
        return true; // Zaten işaretlenmiş
    }

    FE_LOG_INFO("Marking client ID %u for disconnect.", client_id);
    client->is_pending_disconnect = true; // Update döngüsü bunu işleyecek
    return true;
}

fe_server_state_t fe_net_server_get_state(const fe_net_server_t* server) {
    if (!server) {
        return FE_SERVER_STATE_ERROR; // Hatalı kullanım
    }
    return server->state;
}

void* fe_net_server_get_client_user_data(const fe_net_server_t* server, fe_client_id_t client_id) {
    if (!server || client_id == FE_INVALID_CLIENT_ID) return NULL;
    
    // fe_array_get_at ile doğrudan erişemeyiz, client_id bir indeks değil.
    // O(N) arama yapmak zorundayız.
    fe_connected_client_t* client = fe_net_server_get_client_by_id((fe_net_server_t*)server, client_id);
    if (client) {
        return client->user_data;
    }
    return NULL;
}

bool fe_net_server_set_client_user_data(fe_net_server_t* server, fe_client_id_t client_id, void* user_data) {
    if (!server || client_id == FE_INVALID_CLIENT_ID) return false;

    fe_connected_client_t* client = fe_net_server_get_client_by_id(server, client_id);
    if (client) {
        client->user_data = user_data;
        return true;
    }
    return false;
}

size_t fe_net_server_get_connected_client_ids(const fe_net_server_t* server, fe_client_id_t* client_ids_out, size_t max_count) {
    if (!server) return 0;

    size_t count = fe_array_get_size(&server->connected_clients);
    if (client_ids_out && max_count > 0) {
        size_t copy_count = (count < max_count) ? count : max_count;
        for (size_t i = 0; i < copy_count; ++i) {
            fe_connected_client_t* client = (fe_connected_client_t*)fe_array_get_at(&server->connected_clients, i);
            client_ids_out[i] = client->id;
        }
        return copy_count;
    }
    return count; // Sadece toplam sayıyı döndür
}

// --- Geri Çağırma Ayarlayıcıları ---
void fe_net_server_set_on_started_callback(fe_net_server_t* server, PFN_fe_server_on_started callback) {
    if (server) server->on_started_callback = callback;
}
void fe_net_server_set_on_stopped_callback(fe_net_server_t* server, PFN_fe_server_on_stopped callback) {
    if (server) server->on_stopped_callback = callback;
}
void fe_net_server_set_on_client_connected_callback(fe_net_server_t* server, PFN_fe_server_on_client_connected callback) {
    if (server) server->on_client_connected_callback = callback;
}
void fe_net_server_set_on_client_disconnected_callback(fe_net_server_t* server, PFN_fe_server_on_client_disconnected callback) {
    if (server) server->on_client_disconnected_callback = callback;
}
void fe_net_server_set_on_data_received_callback(fe_net_server_t* server, PFN_fe_server_on_data_received callback) {
    if (server) server->on_data_received_callback = callback;
}
void fe_net_server_set_on_error_callback(fe_net_server_t* server, PFN_fe_server_on_error callback) {
    if (server) server->on_error_callback = callback;
}
