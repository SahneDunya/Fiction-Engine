#include "network/fe_net_socket.h"
#include "core/utils/fe_logger.h"
#include "core/memory/fe_memory_manager.h" // FE_MALLOC, FE_FREE
#include "core/utils/fe_string.h"          // fe_string_init, fe_string_set, fe_string_destroy

// --- Platforma Özgü Yardımcı Fonksiyonlar ---

#ifdef _WIN32
// Winsock başlatma bayrağı
static bool s_winsock_initialized = false;

// Winsock hata kodunu Fiction Engine hata koduna çevirir
static fe_net_error_t fe_net_get_last_error() {
    int error_code = WSAGetLastError();
    switch (error_code) {
        case 0:
            return FE_NET_SUCCESS;
        case WSAEWOULDBLOCK:
            return FE_NET_WOULD_BLOCK;
        case WSAEINPROGRESS: // connect non-blocking in progress
            return FE_NET_WOULD_BLOCK;
        case WSAECONNREFUSED:
        case WSAENETUNREACH:
        case WSAETIMEDOUT:
        case WSAHOST_NOT_FOUND:
        case WSATRY_AGAIN:
        case WSANO_RECOVERY:
        case WSANO_DATA:
            return FE_NET_CONNECT_FAILED; // Bağlantı kurulamadı için genel
        case WSAEINVAL: // Invalid argument
            return FE_NET_INVALID_ARGUMENT;
        case WSAEMFILE: // Too many open sockets
        case WSAENOBUFS: // No buffer space available
            return FE_NET_OUT_OF_MEMORY;
        case WSAESOCKTNOSUPPORT: // Socket type not supported
        case WSAEPROTONOSUPPORT: // Protocol not supported
            return FE_NET_SOCKET_CREATE_FAILED;
        case WSAEADDRINUSE: // Address already in use
            return FE_NET_BIND_FAILED;
        case WSAEACCES: // Permission denied
            return FE_NET_BIND_FAILED;
        case WSAENOTSOCK: // Not a socket
            return FE_NET_INVALID_ARGUMENT;
        case WSAEADDRNOTAVAIL: // Cannot assign requested address
            return FE_NET_BIND_FAILED;
        case WSAEISCONN: // Socket is already connected (non-blocking connect completed)
            return FE_NET_SUCCESS; // Connect already succeeded
        case WSAECONNRESET: // Connection reset by peer
        case WSAEDISCON: // Graceful shutdown in progress
            return FE_NET_RECV_FAILED; // Generally means connection is broken for TCP
        default:
            FE_LOG_ERROR("Winsock Error: %d", error_code);
            return FE_NET_UNKNOWN_ERROR;
    }
}

// Bloklama modunu ayarlar (Windows)
static bool fe_net_set_socket_non_blocking_platform(fe_socket_handle_t handle, bool non_blocking) {
    u_long mode = non_blocking ? 1 : 0;
    if (ioctlsocket(handle, FIONBIO, &mode) != 0) {
        FE_LOG_ERROR("ioctlsocket failed with error: %d", WSAGetLastError());
        return false;
    }
    return true;
}

#else // POSIX
// POSIX hata kodunu Fiction Engine hata koduna çevirir
static fe_net_error_t fe_net_get_last_error() {
    switch (errno) {
        case 0:
            return FE_NET_SUCCESS;
        case EWOULDBLOCK:
        case EAGAIN: // Same as EWOULDBLOCK on some systems
            return FE_NET_WOULD_BLOCK;
        case EINPROGRESS: // connect non-blocking in progress
            return FE_NET_WOULD_BLOCK;
        case ECONNREFUSED:
        case ENETUNREACH:
        case ETIMEDOUT:
        case EAI_AGAIN: // Non-recoverable failure in getaddrinfo
        case EAI_NODATA: // No address associated with hostname
        case EAI_NONAME: // Host name lookup failure
            return FE_NET_CONNECT_FAILED; // General connection failure
        case EINVAL: // Invalid argument
            return FE_NET_INVALID_ARGUMENT;
        case EMFILE: // Too many open files
        case ENOMEM: // Out of memory
        case ENOBUFS: // No buffer space available
            return FE_NET_OUT_OF_MEMORY;
        case EPROTOTYPE: // Protocol wrong type for socket
        case EPROTONOSUPPORT: // Protocol not supported
            return FE_NET_SOCKET_CREATE_FAILED;
        case EADDRINUSE: // Address already in use
            return FE_NET_BIND_FAILED;
        case EACCES: // Permission denied
            return FE_NET_BIND_FAILED;
        case ENOTSOCK: // Descriptor is not a socket
            return FE_NET_INVALID_ARGUMENT;
        case EADDRNOTAVAIL: // Cannot assign requested address
            return FE_NET_BIND_FAILED;
        case EISCONN: // Socket is already connected (non-blocking connect completed)
            return FE_NET_SUCCESS; // Connect already succeeded
        case ECONNRESET: // Connection reset by peer
        case ENOTCONN: // Socket not connected (for send/recv on TCP before connect)
            return FE_NET_RECV_FAILED; // Generally means connection is broken for TCP
        default:
            FE_LOG_ERROR("POSIX Error: %d (%s)", errno, strerror(errno));
            return FE_NET_UNKNOWN_ERROR;
    }
}

// Bloklama modunu ayarlar (POSIX)
static bool fe_net_set_socket_non_blocking_platform(fe_socket_handle_t handle, bool non_blocking) {
    int flags = fcntl(handle, F_GETFL, 0);
    if (flags < 0) {
        FE_LOG_ERROR("fcntl(F_GETFL) failed: %d", errno);
        return false;
    }
    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    if (fcntl(handle, F_SETFL, flags) < 0) {
        FE_LOG_ERROR("fcntl(F_SETFL) failed: %d", errno);
        return false;
    }
    return true;
}

#endif // _WIN32

// --- Genel Ağ Başlatma/Kapatma ---

fe_net_error_t fe_net_init() {
#ifdef _WIN32
    if (s_winsock_initialized) {
        FE_LOG_WARN("Winsock already initialized.");
        return FE_NET_SUCCESS;
    }
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        FE_LOG_CRITICAL("WSAStartup failed with error: %d", result);
        return FE_NET_INIT_FAILED;
    }
    s_winsock_initialized = true;
    FE_LOG_INFO("Winsock initialized.");
#else
    FE_LOG_INFO("Network init (POSIX) - no specific startup required.");
#endif
    return FE_NET_SUCCESS;
}

void fe_net_shutdown() {
#ifdef _WIN32
    if (!s_winsock_initialized) {
        FE_LOG_WARN("Winsock not initialized, nothing to shutdown.");
        return;
    }
    if (WSACleanup() != 0) {
        FE_LOG_ERROR("WSACleanup failed with error: %d", WSAGetLastError());
    } else {
        s_winsock_initialized = false;
        FE_LOG_INFO("Winsock shut down.");
    }
#else
    FE_LOG_INFO("Network shutdown (POSIX) - no specific cleanup required.");
#endif
}

// --- IP Adresi Fonksiyonları ---

void fe_ip_address_destroy(fe_ip_address_t* addr) {
    if (!addr) return;
    fe_string_destroy(&addr->ip_string);
    // memset(&addr->addr, 0, sizeof(struct sockaddr_storage)); // Gerek yok, sadece string önemli
    FE_LOG_DEBUG("fe_ip_address_t destroyed.");
}

fe_net_error_t fe_net_resolve_address(const char* host_name, uint16_t port, fe_array_t* out_addresses, fe_socket_type_t socket_type) {
    if (!host_name || !out_addresses || !fe_array_is_initialized(out_addresses) || port == 0) {
        FE_LOG_ERROR("Invalid arguments for fe_net_resolve_address.");
        return FE_NET_INVALID_ARGUMENT;
    }

#ifdef _WIN32
    if (!s_winsock_initialized) {
        return FE_NET_NOT_INITIALIZED;
    }
#endif

    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // IPv4 veya IPv6
    hints.ai_socktype = (socket_type == FE_SOCKET_TYPE_TCP) ? SOCK_STREAM : SOCK_DGRAM;
    hints.ai_protocol = (socket_type == FE_SOCKET_TYPE_TCP) ? IPPROTO_TCP : IPPROTO_UDP;

    char port_str[6]; // Max 65535
    sprintf(port_str, "%hu", port);

    int status = getaddrinfo(host_name, port_str, &hints, &res);
    if (status != 0) {
        FE_LOG_ERROR("getaddrinfo failed for %s:%hu. Error: %s", host_name, port, gai_strerror(status));
        return FE_NET_RESOLVE_FAILED;
    }

    fe_net_error_t result = FE_NET_RESOLVE_FAILED; // Varsayılan olarak başarısız

    for (p = res; p != NULL; p = p->ai_next) {
        fe_ip_address_t new_addr;
        fe_string_init(&new_addr.ip_string, ""); // Initialize string
        new_addr.port = port;
        memcpy(&new_addr.addr, p->ai_addr, p->ai_addrlen);
        new_addr.addr_len = p->ai_addrlen;

        char ipstr[INET6_ADDRSTRLEN];
        void* addr_ptr;

        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
            addr_ptr = &(ipv4->sin_addr);
            new_addr.is_ipv6 = false;
        } else { // IPv6
            struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)p->ai_addr;
            addr_ptr = &(ipv6->sin6_addr);
            new_addr.is_ipv6 = true;
        }

        if (inet_ntop(p->ai_family, addr_ptr, ipstr, sizeof(ipstr)) == NULL) {
            FE_LOG_WARN("inet_ntop failed for an address.");
            fe_string_destroy(&new_addr.ip_string);
            continue;
        }

        fe_string_set(&new_addr.ip_string, ipstr);
        
        if (fe_array_add(out_addresses, &new_addr) == -1) {
            FE_LOG_ERROR("Failed to add resolved address to array.");
            fe_string_destroy(&new_addr.ip_string);
            result = FE_NET_OUT_OF_MEMORY;
            break; // Hata durumunda döngüden çık
        }
        FE_LOG_DEBUG("Resolved %s:%hu to %s (IPv%d)", host_name, port, ipstr, p->ai_family == AF_INET ? 4 : 6);
        result = FE_NET_SUCCESS; // En az bir adres çözüldüyse başarılı say
    }

    freeaddrinfo(res);
    return result;
}

// --- Soket Oluşturma/Kapatma ---

fe_socket_t* fe_net_socket_create(fe_socket_type_t type, bool is_blocking) {
#ifdef _WIN32
    if (!s_winsock_initialized) {
        FE_LOG_ERROR("Winsock not initialized. Call fe_net_init() first.");
        return NULL;
    }
#endif

    fe_socket_t* sock = FE_MALLOC(sizeof(fe_socket_t), FE_MEM_TYPE_NETWORK);
    if (!sock) {
        FE_LOG_CRITICAL("Failed to allocate memory for fe_socket_t.");
        return NULL;
    }
    memset(sock, 0, sizeof(fe_socket_t));

    sock->type = type;
    sock->is_blocking = is_blocking;
    sock->handle = FE_INVALID_SOCKET;
    sock->is_bound = false;
    sock->is_listening = false;
    sock->is_connected = false;
    fe_string_init(&sock->local_addr.ip_string, "");
    fe_string_init(&sock->remote_addr.ip_string, "");


    int sock_type_os = (type == FE_SOCKET_TYPE_TCP) ? SOCK_STREAM : SOCK_DGRAM;
    int protocol_os = (type == FE_SOCKET_TYPE_TCP) ? IPPROTO_TCP : IPPROTO_UDP;

    // AF_INET6 (IPv6) veya AF_INET (IPv4) seçimi
    // Genellikle IPv6 soketleri IPv4 bağlantılarını da kabul edebilir (dual-stack)
    // Basitlik için varsayılan olarak AF_INET kullanıyoruz, daha sonra AF_UNSPEC ile genişletilebilir.
    sock->handle = socket(AF_INET, sock_type_os, protocol_os); // Varsayılan IPv4
    if (sock->handle == FE_INVALID_SOCKET) {
        FE_LOG_ERROR("Failed to create socket. Error: %d", fe_net_get_last_error());
        fe_string_destroy(&sock->local_addr.ip_string);
        fe_string_destroy(&sock->remote_addr.ip_string);
        FE_FREE(sock, FE_MEM_TYPE_NETWORK);
        return NULL;
    }

    // Bloklama modunu ayarla
    if (!fe_net_set_socket_non_blocking_platform(sock->handle, !is_blocking)) {
        FE_LOG_ERROR("Failed to set socket blocking mode.");
        fe_net_socket_destroy(sock); // Temizle
        return NULL;
    }

    FE_LOG_DEBUG("Socket created (Type: %s, Blocking: %s, Handle: %d)",
                 type == FE_SOCKET_TYPE_TCP ? "TCP" : "UDP",
                 is_blocking ? "Yes" : "No",
                 (int)sock->handle);
    return sock;
}

void fe_net_socket_destroy(fe_socket_t* sock) {
    if (!sock) return;

    if (sock->handle != FE_INVALID_SOCKET) {
#ifdef _WIN32
        closesocket(sock->handle);
#else
        close(sock->handle);
#endif
        FE_LOG_DEBUG("Socket handle %d closed.", (int)sock->handle);
    }
    fe_ip_address_destroy(&sock->local_addr);
    fe_ip_address_destroy(&sock->remote_addr);
    FE_FREE(sock, FE_MEM_TYPE_NETWORK);
    FE_LOG_DEBUG("fe_socket_t destroyed.");
}

// --- Soket Bağlama/Dinleme/Bağlantı Kurma ---

fe_net_error_t fe_net_socket_bind(fe_socket_t* sock, const fe_ip_address_t* address) {
    if (!sock || sock->handle == FE_INVALID_SOCKET || !address) {
        FE_LOG_ERROR("Invalid arguments for fe_net_socket_bind.");
        return FE_NET_INVALID_ARGUMENT;
    }

    if (sock->is_bound) {
        FE_LOG_WARN("Socket is already bound.");
        return FE_NET_SUCCESS;
    }

    int result = bind(sock->handle, (struct sockaddr*)&address->addr, address->addr_len);
    if (result == FE_SOCKET_ERROR) {
        fe_net_error_t err = fe_net_get_last_error();
        FE_LOG_ERROR("Failed to bind socket to %s:%hu. Error: %d", address->ip_string.data, address->port, err);
        return err;
    }

    sock->is_bound = true;
    sock->local_addr = *address; // Kendi adresimizi kopyala (deep copy string'i gerektirir)
    fe_string_init(&sock->local_addr.ip_string, address->ip_string.data); // String'i yeniden kopyala
    
    FE_LOG_INFO("Socket bound to %s:%hu.", address->ip_string.data, address->port);
    return FE_NET_SUCCESS;
}

fe_net_error_t fe_net_socket_listen(fe_socket_t* sock, int backlog) {
    if (!sock || sock->type != FE_SOCKET_TYPE_TCP || sock->handle == FE_INVALID_SOCKET) {
        FE_LOG_ERROR("Invalid arguments or socket type for fe_net_socket_listen. Only TCP sockets can listen.");
        return FE_NET_INVALID_ARGUMENT;
    }
    if (!sock->is_bound) {
        FE_LOG_ERROR("Socket must be bound before listening.");
        return FE_NET_INVALID_STATE;
    }
    if (sock->is_listening) {
        FE_LOG_WARN("Socket is already listening.");
        return FE_NET_SUCCESS;
    }

    int result = listen(sock->handle, backlog);
    if (result == FE_SOCKET_ERROR) {
        fe_net_error_t err = fe_net_get_last_error();
        FE_LOG_ERROR("Failed to listen on socket. Error: %d", err);
        return err;
    }

    sock->is_listening = true;
    FE_LOG_INFO("Socket listening on port %hu with backlog %d.", sock->local_addr.port, backlog);
    return FE_NET_SUCCESS;
}

fe_socket_t* fe_net_socket_accept(fe_socket_t* listener_sock, fe_ip_address_t* out_accepted_addr) {
    if (!listener_sock || listener_sock->type != FE_SOCKET_TYPE_TCP || !listener_sock->is_listening || listener_sock->handle == FE_INVALID_SOCKET) {
        FE_LOG_ERROR("Invalid arguments or listener socket state for fe_net_socket_accept.");
        return NULL;
    }

    fe_socket_t* new_sock = NULL;
    struct sockaddr_storage remote_addr_storage;
    socklen_t addr_len = sizeof(remote_addr_storage);
    
    fe_socket_handle_t client_handle = accept(listener_sock->handle, (struct sockaddr*)&remote_addr_storage, &addr_len);

    if (client_handle == FE_INVALID_SOCKET) {
        fe_net_error_t err = fe_net_get_last_error();
        if (err == FE_NET_WOULD_BLOCK) {
            // FE_LOG_DEBUG("No incoming connections (would block).");
        } else {
            FE_LOG_ERROR("Failed to accept connection. Error: %d", err);
        }
        return NULL;
    }

    new_sock = fe_net_socket_create(FE_SOCKET_TYPE_TCP, listener_sock->is_blocking); // Kabul edilen soket de aynı bloklama modunda olsun
    if (!new_sock) {
        FE_LOG_CRITICAL("Failed to create new socket for accepted connection.");
#ifdef _WIN32
        closesocket(client_handle);
#else
        close(client_handle);
#endif
        return NULL;
    }
    new_sock->handle = client_handle;
    new_sock->is_connected = true; // Kabul edilen soket zaten bağlı
    new_sock->is_bound = true; // Kabul edilen soket yerel bir porta bağlıdır.

    // Remote adresi doldur
    if (out_accepted_addr) {
        memset(out_accepted_addr, 0, sizeof(fe_ip_address_t)); // Yeni adres yapısını temizle
        fe_string_init(&out_accepted_addr->ip_string, ""); // String'i başlat
        
        out_accepted_addr->addr = remote_addr_storage;
        out_accepted_addr->addr_len = addr_len;

        char ipstr[INET6_ADDRSTRLEN];
        void* addr_ptr;

        if (remote_addr_storage.ss_family == AF_INET) {
            struct sockaddr_in* ipv4 = (struct sockaddr_in*)&remote_addr_storage;
            addr_ptr = &(ipv4->sin_addr);
            out_accepted_addr->port = ntohs(ipv4->sin_port);
            out_accepted_addr->is_ipv6 = false;
        } else { // AF_INET6
            struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)&remote_addr_storage;
            addr_ptr = &(ipv6->sin6_addr);
            out_accepted_addr->port = ntohs(ipv6->sin6_port);
            out_accepted_addr->is_ipv6 = true;
        }

        if (inet_ntop(remote_addr_storage.ss_family, addr_ptr, ipstr, sizeof(ipstr)) == NULL) {
            FE_LOG_WARN("inet_ntop failed for accepted address.");
            fe_string_set(&out_accepted_addr->ip_string, "Unknown IP");
        } else {
            fe_string_set(&out_accepted_addr->ip_string, ipstr);
        }
    }
    
    new_sock->remote_addr = *out_accepted_addr; // Bağlantı için remote_addr'ı kaydet
    fe_string_init(&new_sock->remote_addr.ip_string, out_accepted_addr->ip_string.data);

    FE_LOG_INFO("Accepted connection from %s:%hu. New socket handle: %d",
                new_sock->remote_addr.ip_string.data, new_sock->remote_addr.port, (int)new_sock->handle);
    return new_sock;
}


fe_net_error_t fe_net_socket_connect(fe_socket_t* sock, const fe_ip_address_t* remote_address) {
    if (!sock || sock->type != FE_SOCKET_TYPE_TCP || sock->handle == FE_INVALID_SOCKET || !remote_address) {
        FE_LOG_ERROR("Invalid arguments or socket type for fe_net_socket_connect. Only TCP sockets can connect.");
        return FE_NET_INVALID_ARGUMENT;
    }
    if (sock->is_connected) {
        FE_LOG_WARN("Socket is already connected.");
        return FE_NET_SUCCESS;
    }

    int result = connect(sock->handle, (struct sockaddr*)&remote_address->addr, remote_address->addr_len);
    if (result == FE_SOCKET_ERROR) {
        fe_net_error_t err = fe_net_get_last_error();
#ifdef _WIN32
        if (err == FE_NET_WOULD_BLOCK || WSAGetLastError() == WSAEINPROGRESS) {
#else
        if (err == FE_NET_WOULD_BLOCK || errno == EINPROGRESS) {
#endif
            FE_LOG_DEBUG("Non-blocking connect in progress to %s:%hu.", remote_address->ip_string.data, remote_address->port);
            sock->remote_addr = *remote_address; // Geçici olarak uzak adresi kaydet
            fe_string_init(&sock->remote_addr.ip_string, remote_address->ip_string.data);
            return FE_NET_WOULD_BLOCK; // Bağlantı devam ediyor
        } else if (err == FE_NET_SUCCESS) {
            // Winsock'ta bazen non-blocking connect anında başarılı olabilir
            sock->is_connected = true;
            sock->remote_addr = *remote_address;
            fe_string_init(&sock->remote_addr.ip_string, remote_address->ip_string.data);
            FE_LOG_INFO("Socket connected to %s:%hu immediately.", remote_address->ip_string.data, remote_address->port);
            return FE_NET_SUCCESS;
        } else {
            FE_LOG_ERROR("Failed to connect socket to %s:%hu. Error: %d", remote_address->ip_string.data, remote_address->port, err);
            return err;
        }
    }

    // Bloklayan modda veya anında başarılı non-blocking connect
    sock->is_connected = true;
    sock->remote_addr = *remote_address;
    fe_string_init(&sock->remote_addr.ip_string, remote_address->ip_string.data);
    FE_LOG_INFO("Socket connected to %s:%hu.", remote_address->ip_string.data, remote_address->port);
    return FE_NET_SUCCESS;
}

fe_net_error_t fe_net_socket_check_connect(fe_socket_t* sock) {
    if (!sock || sock->type != FE_SOCKET_TYPE_TCP || sock->handle == FE_INVALID_SOCKET) {
        return FE_NET_INVALID_ARGUMENT;
    }
    if (sock->is_connected) {
        return FE_NET_SUCCESS; // Zaten bağlı
    }

    if (sock->is_blocking) {
        // Bloklayan sokette bu fonksiyonun çağrılması anlamsız.
        // Bağlantı zaten fe_net_socket_connect çağrıldığında tamamlanmış olmalıydı.
        // Hata kabul etmeyiz, zaten bağlı olması beklenir.
        FE_LOG_WARN("fe_net_socket_check_connect called on a blocking socket. Assumed connected.");
        return FE_NET_SUCCESS;
    }

    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(sock->handle, &write_fds);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0; // Hemen kontrol et

    int select_res = select((int)sock->handle + 1, NULL, &write_fds, NULL, &tv);

    if (select_res == FE_SOCKET_ERROR) {
        return fe_net_get_last_error();
    }
    if (select_res == 0) {
        return FE_NET_WOULD_BLOCK; // Hala yazmaya hazır değil, yani bağlantı devam ediyor
    }

    // Eğer select başarılı olduysa ve soket yazmaya hazırsa, hata kontrolü yap
    int optval;
    socklen_t optlen = sizeof(optval);
    if (getsockopt(sock->handle, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen) == FE_SOCKET_ERROR) {
        return fe_net_get_last_error();
    }

    if (optval == 0) {
        sock->is_connected = true;
        FE_LOG_INFO("Non-blocking connect to %s:%hu completed successfully.", sock->remote_addr.ip_string.data, sock->remote_addr.port);
        return FE_NET_SUCCESS;
    } else {
        // Bağlantı hatası oluştu
#ifdef _WIN32
        WSASetLastError(optval); // Getsockopt'tan gelen hatayı Winsock'un son hatası yap
#else
        errno = optval; // Getsockopt'tan gelen hatayı errno yap
#endif
        FE_LOG_ERROR("Non-blocking connect to %s:%hu failed with error: %d", sock->remote_addr.ip_string.data, sock->remote_addr.port, fe_net_get_last_error());
        return fe_net_get_last_error();
    }
}


// --- Veri Gönderme/Alma ---

fe_net_error_t fe_net_socket_send(fe_socket_t* sock, const void* data, size_t size, size_t* out_sent_bytes) {
    if (!sock || sock->type != FE_SOCKET_TYPE_TCP || sock->handle == FE_INVALID_SOCKET || !data || size == 0) {
        FE_LOG_ERROR("Invalid arguments for fe_net_socket_send.");
        return FE_NET_INVALID_ARGUMENT;
    }
    if (!sock->is_connected) {
        FE_LOG_ERROR("Cannot send data, TCP socket is not connected.");
        return FE_NET_INVALID_STATE;
    }

    ssize_t sent_bytes = send(sock->handle, (const char*)data, (int)size, 0);

    if (sent_bytes == FE_SOCKET_ERROR) {
        fe_net_error_t err = fe_net_get_last_error();
        if (err == FE_NET_WOULD_BLOCK) {
            if (out_sent_bytes) *out_sent_bytes = 0;
            return FE_NET_WOULD_BLOCK;
        }
        FE_LOG_ERROR("Failed to send data on TCP socket %d. Error: %d", (int)sock->handle, err);
        return err;
    }

    if (out_sent_bytes) *out_sent_bytes = (size_t)sent_bytes;
    // FE_LOG_TRACE("Sent %zu bytes on TCP socket %d.", (size_t)sent_bytes, (int)sock->handle);
    return FE_NET_SUCCESS;
}

fe_net_error_t fe_net_socket_receive(fe_socket_t* sock, void* buffer, size_t buffer_size, size_t* out_received_bytes) {
    if (!sock || sock->type != FE_SOCKET_TYPE_TCP || sock->handle == FE_INVALID_SOCKET || !buffer || buffer_size == 0) {
        FE_LOG_ERROR("Invalid arguments for fe_net_socket_receive.");
        return FE_NET_INVALID_ARGUMENT;
    }
    if (!sock->is_connected) {
        // Bloklamayan modda her zaman çağrılabilir, bağlantı henüz kurulmamış olabilir.
        // FE_LOG_WARN("Cannot receive data, TCP socket is not connected.");
        // return FE_NET_INVALID_STATE; // Bu durumda FE_NET_NO_DATA veya WOULDBLOCK dönebiliriz.
        // Şimdilik, sadece henüz bağlı değilse veri almayız.
        if (out_received_bytes) *out_received_bytes = 0;
        return FE_NET_NO_DATA;
    }

    ssize_t received_bytes = recv(sock->handle, (char*)buffer, (int)buffer_size, 0);

    if (received_bytes == FE_SOCKET_ERROR) {
        fe_net_error_t err = fe_net_get_last_error();
        if (err == FE_NET_WOULD_BLOCK) {
            if (out_received_bytes) *out_received_bytes = 0;
            return FE_NET_NO_DATA; // Veri yok
        }
        FE_LOG_ERROR("Failed to receive data on TCP socket %d. Error: %d", (int)sock->handle, err);
        return err;
    }
    
    if (received_bytes == 0) {
        // Bağlantı kapatıldı (graceful shutdown)
        FE_LOG_INFO("TCP socket %d connection closed by remote peer.", (int)sock->handle);
        sock->is_connected = false; // Bağlantı durumunu güncelle
        if (out_received_bytes) *out_received_bytes = 0;
        return FE_NET_SUCCESS; // Başarılı, ancak 0 byte alındı
    }

    if (out_received_bytes) *out_received_bytes = (size_t)received_bytes;
    // FE_LOG_TRACE("Received %zu bytes on TCP socket %d.", (size_t)received_bytes, (int)sock->handle);
    return FE_NET_SUCCESS;
}

fe_net_error_t fe_net_socket_send_to(fe_socket_t* sock, const void* data, size_t size, const fe_ip_address_t* remote_address, size_t* out_sent_bytes) {
    if (!sock || sock->type != FE_SOCKET_TYPE_UDP || sock->handle == FE_INVALID_SOCKET || !data || size == 0 || !remote_address) {
        FE_LOG_ERROR("Invalid arguments for fe_net_socket_send_to.");
        return FE_NET_INVALID_ARGUMENT;
    }

    ssize_t sent_bytes = sendto(sock->handle, (const char*)data, (int)size, 0, (const struct sockaddr*)&remote_address->addr, remote_address->addr_len);

    if (sent_bytes == FE_SOCKET_ERROR) {
        fe_net_error_t err = fe_net_get_last_error();
        if (err == FE_NET_WOULD_BLOCK) {
             if (out_sent_bytes) *out_sent_bytes = 0;
            return FE_NET_WOULD_BLOCK;
        }
        FE_LOG_ERROR("Failed to sendto data on UDP socket %d to %s:%hu. Error: %d", (int)sock->handle, remote_address->ip_string.data, remote_address->port, err);
        return err;
    }

    if (out_sent_bytes) *out_sent_bytes = (size_t)sent_bytes;
    // FE_LOG_TRACE("Sent %zu bytes on UDP socket %d to %s:%hu.", (size_t)sent_bytes, (int)sock->handle, remote_address->ip_string.data, remote_address->port);
    return FE_NET_SUCCESS;
}

fe_net_error_t fe_net_socket_receive_from(fe_socket_t* sock, void* buffer, size_t buffer_size, fe_ip_address_t* out_sender_address, size_t* out_received_bytes) {
    if (!sock || sock->type != FE_SOCKET_TYPE_UDP || sock->handle == FE_INVALID_SOCKET || !buffer || buffer_size == 0) {
        FE_LOG_ERROR("Invalid arguments for fe_net_socket_receive_from.");
        return FE_NET_INVALID_ARGUMENT;
    }

    struct sockaddr_storage sender_addr_storage;
    socklen_t addr_len = sizeof(sender_addr_storage);
    memset(&sender_addr_storage, 0, addr_len); // Temizle

    ssize_t received_bytes = recvfrom(sock->handle, (char*)buffer, (int)buffer_size, 0, (struct sockaddr*)&sender_addr_storage, &addr_len);

    if (received_bytes == FE_SOCKET_ERROR) {
        fe_net_error_t err = fe_net_get_last_error();
        if (err == FE_NET_WOULD_BLOCK) {
            if (out_received_bytes) *out_received_bytes = 0;
            return FE_NET_NO_DATA; // Veri yok
        }
        FE_LOG_ERROR("Failed to receivefrom data on UDP socket %d. Error: %d", (int)sock->handle, err);
        return err;
    }
    
    if (out_received_bytes) *out_received_bytes = (size_t)received_bytes;

    if (out_sender_address) {
        // out_sender_address'i temizle ve doldur
        fe_ip_address_destroy(out_sender_address); // Önceki string'i serbest bırak
        memset(out_sender_address, 0, sizeof(fe_ip_address_t)); // Yapıyı sıfırla
        fe_string_init(&out_sender_address->ip_string, ""); // String'i yeniden başlat

        out_sender_address->addr = sender_addr_storage;
        out_sender_address->addr_len = addr_len;

        char ipstr[INET6_ADDRSTRLEN];
        void* addr_ptr;

        if (sender_addr_storage.ss_family == AF_INET) {
            struct sockaddr_in* ipv4 = (struct sockaddr_in*)&sender_addr_storage;
            addr_ptr = &(ipv4->sin_addr);
            out_sender_address->port = ntohs(ipv4->sin_port);
            out_sender_address->is_ipv6 = false;
        } else { // AF_INET6
            struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)&sender_addr_storage;
            addr_ptr = &(ipv6->sin6_addr);
            out_sender_address->port = ntohs(ipv6->sin6_port);
            out_sender_address->is_ipv6 = true;
        }

        if (inet_ntop(sender_addr_storage.ss_family, addr_ptr, ipstr, sizeof(ipstr)) == NULL) {
            FE_LOG_WARN("inet_ntop failed for sender address.");
            fe_string_set(&out_sender_address->ip_string, "Unknown IP");
        } else {
            fe_string_set(&out_sender_address->ip_string, ipstr);
        }
        // FE_LOG_TRACE("Received %zu bytes on UDP socket %d from %s:%hu.", (size_t)received_bytes, (int)sock->handle, out_sender_address->ip_string.data, out_sender_address->port);
    } else {
        // FE_LOG_TRACE("Received %zu bytes on UDP socket %d (sender address not requested).", (size_t)received_bytes, (int)sock->handle);
    }

    return FE_NET_SUCCESS;
}

// --- Soket Seçenekleri ---

fe_net_error_t fe_net_socket_set_blocking(fe_socket_t* sock, bool blocking) {
    if (!sock || sock->handle == FE_INVALID_SOCKET) {
        return FE_NET_INVALID_ARGUMENT;
    }
    if (sock->is_blocking == blocking) {
        FE_LOG_DEBUG("Socket already in desired blocking mode.");
        return FE_NET_SUCCESS;
    }

    if (fe_net_set_socket_non_blocking_platform(sock->handle, !blocking)) {
        sock->is_blocking = blocking;
        FE_LOG_DEBUG("Socket %d blocking mode set to %s.", (int)sock->handle, blocking ? "blocking" : "non-blocking");
        return FE_NET_SUCCESS;
    } else {
        FE_LOG_ERROR("Failed to set socket %d blocking mode.", (int)sock->handle);
        return FE_NET_SET_OPT_FAILED;
    }
}

fe_net_error_t fe_net_socket_set_reuse_address(fe_socket_t* sock, bool enable) {
    if (!sock || sock->handle == FE_INVALID_SOCKET) {
        return FE_NET_INVALID_ARGUMENT;
    }

    int optval = enable ? 1 : 0;
    int result = setsockopt(sock->handle, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval));
    if (result == FE_SOCKET_ERROR) {
        fe_net_error_t err = fe_net_get_last_error();
        FE_LOG_ERROR("Failed to set SO_REUSEADDR on socket %d. Error: %d", (int)sock->handle, err);
        return err;
    }
    FE_LOG_DEBUG("Set SO_REUSEADDR on socket %d to %s.", (int)sock->handle, enable ? "true" : "false");
    return FE_NET_SUCCESS;
}

fe_net_error_t fe_net_socket_get_local_address(fe_socket_t* sock, fe_ip_address_t* out_address) {
    if (!sock || sock->handle == FE_INVALID_SOCKET || !out_address) {
        FE_LOG_ERROR("Invalid arguments for fe_net_socket_get_local_address.");
        return FE_NET_INVALID_ARGUMENT;
    }

    struct sockaddr_storage addr_storage;
    socklen_t addr_len = sizeof(addr_storage);

    if (getsockname(sock->handle, (struct sockaddr*)&addr_storage, &addr_len) == FE_SOCKET_ERROR) {
        fe_net_error_t err = fe_net_get_last_error();
        FE_LOG_ERROR("Failed to get local address for socket %d. Error: %d", (int)sock->handle, err);
        return err;
    }
    
    // out_address'i temizle ve doldur
    fe_ip_address_destroy(out_address); // Önceki string'i serbest bırak
    memset(out_address, 0, sizeof(fe_ip_address_t)); // Yapıyı sıfırla
    fe_string_init(&out_address->ip_string, ""); // String'i yeniden başlat

    out_address->addr = addr_storage;
    out_address->addr_len = addr_len;

    char ipstr[INET6_ADDRSTRLEN];
    void* addr_ptr;

    if (addr_storage.ss_family == AF_INET) {
        struct sockaddr_in* ipv4 = (struct sockaddr_in*)&addr_storage;
        addr_ptr = &(ipv4->sin_addr);
        out_address->port = ntohs(ipv4->sin_port);
        out_address->is_ipv6 = false;
    } else { // AF_INET6
        struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)&addr_storage;
        addr_ptr = &(ipv6->sin6_addr);
        out_address->port = ntohs(ipv6->sin6_port);
        out_address->is_ipv6 = true;
    }

    if (inet_ntop(addr_storage.ss_family, addr_ptr, ipstr, sizeof(ipstr)) == NULL) {
        FE_LOG_WARN("inet_ntop failed for local address.");
        fe_string_set(&out_address->ip_string, "Unknown IP");
    } else {
        fe_string_set(&out_address->ip_string, ipstr);
    }
    FE_LOG_DEBUG("Got local address for socket %d: %s:%hu", (int)sock->handle, out_address->ip_string.data, out_address->port);
    return FE_NET_SUCCESS;
}

fe_net_error_t fe_net_socket_get_remote_address(fe_socket_t* sock, fe_ip_address_t* out_address) {
    if (!sock || sock->handle == FE_INVALID_SOCKET || !out_address || sock->type != FE_SOCKET_TYPE_TCP) {
        FE_LOG_ERROR("Invalid arguments or socket type for fe_net_socket_get_remote_address. Only TCP sockets can have a remote address.");
        return FE_NET_INVALID_ARGUMENT;
    }
    if (!sock->is_connected) {
        FE_LOG_WARN("TCP socket %d is not connected, no remote address available.", (int)sock->handle);
        return FE_NET_INVALID_STATE;
    }

    struct sockaddr_storage addr_storage;
    socklen_t addr_len = sizeof(addr_storage);

    if (getpeername(sock->handle, (struct sockaddr*)&addr_storage, &addr_len) == FE_SOCKET_ERROR) {
        fe_net_error_t err = fe_net_get_last_error();
        FE_LOG_ERROR("Failed to get remote address for socket %d. Error: %d", (int)sock->handle, err);
        return err;
    }
    
    // out_address'i temizle ve doldur
    fe_ip_address_destroy(out_address); // Önceki string'i serbest bırak
    memset(out_address, 0, sizeof(fe_ip_address_t)); // Yapıyı sıfırla
    fe_string_init(&out_address->ip_string, ""); // String'i yeniden başlat

    out_address->addr = addr_storage;
    out_address->addr_len = addr_len;

    char ipstr[INET6_ADDRSTRLEN];
    void* addr_ptr;

    if (addr_storage.ss_family == AF_INET) {
        struct sockaddr_in* ipv4 = (struct sockaddr_in*)&addr_storage;
        addr_ptr = &(ipv4->sin_addr);
        out_address->port = ntohs(ipv4->sin_port);
        out_address->is_ipv6 = false;
    } else { // AF_INET6
        struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)&addr_storage;
        addr_ptr = &(ipv6->sin6_addr);
        out_address->port = ntohs(ipv6->sin6_port);
        out_address->is_ipv6 = true;
    }

    if (inet_ntop(addr_storage.ss_family, addr_ptr, ipstr, sizeof(ipstr)) == NULL) {
        FE_LOG_WARN("inet_ntop failed for remote address.");
        fe_string_set(&out_address->ip_string, "Unknown IP");
    } else {
        fe_string_set(&out_address->ip_string, ipstr);
    }
    FE_LOG_DEBUG("Got remote address for socket %d: %s:%hu", (int)sock->handle, out_address->ip_string.data, out_address->port);
    return FE_NET_SUCCESS;
}
