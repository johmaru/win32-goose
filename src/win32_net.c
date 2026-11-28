/* winsock2.h must precede windows.h. */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "win32_internal.h"

/* ---- Byte-order & fd_set helpers (avoid ws2_32 imports) ------------------ */

static uint16_t w32_bswap16(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}

static uint32_t w32_bswap32(uint32_t v) {
    return ((v & 0x000000FFu) << 24) |
           ((v & 0x0000FF00u) << 8)  |
           ((v & 0x00FF0000u) >> 8)  |
           ((v & 0xFF000000u) >> 24);
}

static int w32_fd_isset(SOCKET s, const fd_set *set) {
    for (u_int i = 0; i < set->fd_count; i++) {
        if (set->fd_array[i] == s) return 1;
    }
    return 0;
}

/* ---- Lazy Winsock2 table (ws2_32.dll) ------------------------------------ */

typedef int    (WSAAPI *pfn_WSAStartup)(WORD, LPWSADATA);
typedef int    (WSAAPI *pfn_WSACleanup)(void);
typedef int    (WSAAPI *pfn_inet_pton)(INT, PCSTR, PVOID);
typedef PCSTR  (WSAAPI *pfn_inet_ntop)(INT, const void *, PSTR, size_t);
typedef int    (WSAAPI *pfn_getaddrinfo)(PCSTR, PCSTR, const struct addrinfo *, struct addrinfo **);
typedef void   (WSAAPI *pfn_freeaddrinfo)(struct addrinfo *);
typedef SOCKET (WSAAPI *pfn_socket)(int, int, int);
typedef int    (WSAAPI *pfn_setsockopt)(SOCKET, int, int, const char *, int);
typedef int    (WSAAPI *pfn_getsockopt)(SOCKET, int, int, char *, int *);
typedef int    (WSAAPI *pfn_bind)(SOCKET, const struct sockaddr *, int);
typedef int    (WSAAPI *pfn_listen)(SOCKET, int);
typedef int    (WSAAPI *pfn_getsockname)(SOCKET, struct sockaddr *, int *);
typedef SOCKET (WSAAPI *pfn_accept)(SOCKET, struct sockaddr *, int *);
typedef int    (WSAAPI *pfn_ioctlsocket)(SOCKET, long, u_long *);
typedef int    (WSAAPI *pfn_connect)(SOCKET, const struct sockaddr *, int);
typedef int    (WSAAPI *pfn_WSAGetLastError)(void);
typedef int    (WSAAPI *pfn_select)(int, fd_set *, fd_set *, fd_set *, const struct timeval *);
typedef int    (WSAAPI *pfn_send)(SOCKET, const char *, int, int);
typedef int    (WSAAPI *pfn_recv)(SOCKET, char *, int, int);
typedef int    (WSAAPI *pfn_shutdown)(SOCKET, int);
typedef int    (WSAAPI *pfn_closesocket)(SOCKET);
typedef int    (WSAAPI *pfn_sendto)(SOCKET, const char *, int, int, const struct sockaddr *, int);
typedef int    (WSAAPI *pfn_recvfrom)(SOCKET, char *, int, int, struct sockaddr *, int *);
typedef int    (WSAAPI *pfn_WSAPoll)(LPWSAPOLLFD, ULONG, INT);

static struct {
    int tried;
    int loaded;
    HMODULE mod;
    pfn_WSAStartup      WSAStartup;
    pfn_WSACleanup      WSACleanup;
    pfn_inet_pton       inet_pton;
    pfn_inet_ntop       inet_ntop;
    pfn_getaddrinfo     getaddrinfo;
    pfn_freeaddrinfo    freeaddrinfo;
    pfn_socket          socket;
    pfn_setsockopt      setsockopt;
    pfn_getsockopt      getsockopt;
    pfn_bind            bind;
    pfn_listen          listen;
    pfn_getsockname     getsockname;
    pfn_accept          accept;
    pfn_ioctlsocket     ioctlsocket;
    pfn_connect         connect;
    pfn_WSAGetLastError WSAGetLastError;
    pfn_select          select;
    pfn_send            send;
    pfn_recv            recv;
    pfn_shutdown        shutdown;
    pfn_closesocket     closesocket;
    pfn_sendto          sendto;
    pfn_recvfrom        recvfrom;
    pfn_WSAPoll         WSAPoll;
} g_wsa;

static int ensure_ws2(void) {
    if (g_wsa.loaded) return 1;
    if (g_wsa.tried) return 0;
    g_wsa.tried = 1;
    g_wsa.mod = LoadLibraryW(L"ws2_32.dll");
    if (!g_wsa.mod) return 0;

    g_wsa.WSAStartup      = (pfn_WSAStartup)(uintptr_t)GetProcAddress(g_wsa.mod, "WSAStartup");
    g_wsa.WSACleanup      = (pfn_WSACleanup)(uintptr_t)GetProcAddress(g_wsa.mod, "WSACleanup");
    g_wsa.inet_pton       = (pfn_inet_pton)(uintptr_t)GetProcAddress(g_wsa.mod, "inet_pton");
    g_wsa.inet_ntop       = (pfn_inet_ntop)(uintptr_t)GetProcAddress(g_wsa.mod, "inet_ntop");
    g_wsa.getaddrinfo     = (pfn_getaddrinfo)(uintptr_t)GetProcAddress(g_wsa.mod, "getaddrinfo");
    g_wsa.freeaddrinfo    = (pfn_freeaddrinfo)(uintptr_t)GetProcAddress(g_wsa.mod, "freeaddrinfo");
    g_wsa.socket          = (pfn_socket)(uintptr_t)GetProcAddress(g_wsa.mod, "socket");
    g_wsa.setsockopt      = (pfn_setsockopt)(uintptr_t)GetProcAddress(g_wsa.mod, "setsockopt");
    g_wsa.getsockopt      = (pfn_getsockopt)(uintptr_t)GetProcAddress(g_wsa.mod, "getsockopt");
    g_wsa.bind            = (pfn_bind)(uintptr_t)GetProcAddress(g_wsa.mod, "bind");
    g_wsa.listen          = (pfn_listen)(uintptr_t)GetProcAddress(g_wsa.mod, "listen");
    g_wsa.getsockname     = (pfn_getsockname)(uintptr_t)GetProcAddress(g_wsa.mod, "getsockname");
    g_wsa.accept          = (pfn_accept)(uintptr_t)GetProcAddress(g_wsa.mod, "accept");
    g_wsa.ioctlsocket     = (pfn_ioctlsocket)(uintptr_t)GetProcAddress(g_wsa.mod, "ioctlsocket");
    g_wsa.connect         = (pfn_connect)(uintptr_t)GetProcAddress(g_wsa.mod, "connect");
    g_wsa.WSAGetLastError = (pfn_WSAGetLastError)(uintptr_t)GetProcAddress(g_wsa.mod, "WSAGetLastError");
    g_wsa.select          = (pfn_select)(uintptr_t)GetProcAddress(g_wsa.mod, "select");
    g_wsa.send            = (pfn_send)(uintptr_t)GetProcAddress(g_wsa.mod, "send");
    g_wsa.recv            = (pfn_recv)(uintptr_t)GetProcAddress(g_wsa.mod, "recv");
    g_wsa.shutdown        = (pfn_shutdown)(uintptr_t)GetProcAddress(g_wsa.mod, "shutdown");
    g_wsa.closesocket     = (pfn_closesocket)(uintptr_t)GetProcAddress(g_wsa.mod, "closesocket");
    g_wsa.sendto          = (pfn_sendto)(uintptr_t)GetProcAddress(g_wsa.mod, "sendto");
    g_wsa.recvfrom        = (pfn_recvfrom)(uintptr_t)GetProcAddress(g_wsa.mod, "recvfrom");
    g_wsa.WSAPoll         = (pfn_WSAPoll)(uintptr_t)GetProcAddress(g_wsa.mod, "WSAPoll");

    if (g_wsa.WSAStartup && g_wsa.WSACleanup && g_wsa.inet_pton && g_wsa.inet_ntop &&
        g_wsa.getaddrinfo && g_wsa.freeaddrinfo && g_wsa.socket && g_wsa.setsockopt &&
        g_wsa.getsockopt && g_wsa.bind && g_wsa.listen && g_wsa.getsockname &&
        g_wsa.accept && g_wsa.ioctlsocket && g_wsa.connect && g_wsa.WSAGetLastError &&
        g_wsa.select && g_wsa.send && g_wsa.recv && g_wsa.shutdown &&
        g_wsa.closesocket && g_wsa.sendto && g_wsa.recvfrom && g_wsa.WSAPoll) {
        g_wsa.loaded = 1;
        return 1;
    }
    return 0;
}

/* ---- Lazy WinHTTP table (winhttp.dll) ------------------------------------ */

typedef BOOL      (WINAPI *pfn_WinHttpCrackUrl)(LPCWSTR, DWORD, DWORD, LPURL_COMPONENTS);
typedef HINTERNET (WINAPI *pfn_WinHttpOpen)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
typedef BOOL      (WINAPI *pfn_WinHttpSetTimeouts)(HINTERNET, int, int, int, int);
typedef HINTERNET (WINAPI *pfn_WinHttpConnect)(HINTERNET, LPCWSTR, INTERNET_PORT, DWORD);
typedef HINTERNET (WINAPI *pfn_WinHttpOpenRequest)(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR *, DWORD);
typedef BOOL      (WINAPI *pfn_WinHttpSendRequest)(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR);
typedef BOOL      (WINAPI *pfn_WinHttpReceiveResponse)(HINTERNET, LPVOID);
typedef BOOL      (WINAPI *pfn_WinHttpQueryHeaders)(HINTERNET, DWORD, LPCWSTR, LPVOID, LPDWORD, LPDWORD);
typedef BOOL      (WINAPI *pfn_WinHttpQueryDataAvailable)(HINTERNET, LPDWORD);
typedef BOOL      (WINAPI *pfn_WinHttpReadData)(HINTERNET, LPVOID, DWORD, LPDWORD);
typedef BOOL      (WINAPI *pfn_WinHttpCloseHandle)(HINTERNET);
typedef BOOL      (WINAPI *pfn_WinHttpSetOption)(HINTERNET, DWORD, LPVOID, DWORD);
typedef BOOL      (WINAPI *pfn_WinHttpQueryOption)(HINTERNET, DWORD, LPVOID, LPDWORD);

#ifndef WINHTTP_OPTION_URL
#define WINHTTP_OPTION_URL 45
#endif
#ifndef WINHTTP_OPTION_DECOMPRESSION
#define WINHTTP_OPTION_DECOMPRESSION 118
#endif
#ifndef WINHTTP_DECOMPRESSION_FLAG_ALL
#define WINHTTP_DECOMPRESSION_FLAG_ALL 0x00000003
#endif
#ifndef WINHTTP_QUERY_CONTENT_TYPE
#define WINHTTP_QUERY_CONTENT_TYPE 1
#endif

static struct {
    int tried;
    int loaded;
    HMODULE mod;
    pfn_WinHttpCrackUrl           CrackUrl;
    pfn_WinHttpOpen               Open;
    pfn_WinHttpSetTimeouts        SetTimeouts;
    pfn_WinHttpConnect            Connect;
    pfn_WinHttpOpenRequest        OpenRequest;
    pfn_WinHttpSendRequest        SendRequest;
    pfn_WinHttpReceiveResponse    ReceiveResponse;
    pfn_WinHttpQueryHeaders       QueryHeaders;
    pfn_WinHttpQueryDataAvailable QueryDataAvailable;
    pfn_WinHttpReadData           ReadData;
    pfn_WinHttpCloseHandle        CloseHandle;
    pfn_WinHttpSetOption          SetOption;
    pfn_WinHttpQueryOption        QueryOption;
} g_http;

static int ensure_winhttp(void) {
    if (g_http.loaded) return 1;
    if (g_http.tried) return 0;
    g_http.tried = 1;
    g_http.mod = LoadLibraryW(L"winhttp.dll");
    if (!g_http.mod) return 0;

    g_http.CrackUrl           = (pfn_WinHttpCrackUrl)(uintptr_t)GetProcAddress(g_http.mod, "WinHttpCrackUrl");
    g_http.Open               = (pfn_WinHttpOpen)(uintptr_t)GetProcAddress(g_http.mod, "WinHttpOpen");
    g_http.SetTimeouts        = (pfn_WinHttpSetTimeouts)(uintptr_t)GetProcAddress(g_http.mod, "WinHttpSetTimeouts");
    g_http.Connect            = (pfn_WinHttpConnect)(uintptr_t)GetProcAddress(g_http.mod, "WinHttpConnect");
    g_http.OpenRequest        = (pfn_WinHttpOpenRequest)(uintptr_t)GetProcAddress(g_http.mod, "WinHttpOpenRequest");
    g_http.SendRequest        = (pfn_WinHttpSendRequest)(uintptr_t)GetProcAddress(g_http.mod, "WinHttpSendRequest");
    g_http.ReceiveResponse    = (pfn_WinHttpReceiveResponse)(uintptr_t)GetProcAddress(g_http.mod, "WinHttpReceiveResponse");
    g_http.QueryHeaders       = (pfn_WinHttpQueryHeaders)(uintptr_t)GetProcAddress(g_http.mod, "WinHttpQueryHeaders");
    g_http.QueryDataAvailable = (pfn_WinHttpQueryDataAvailable)(uintptr_t)GetProcAddress(g_http.mod, "WinHttpQueryDataAvailable");
    g_http.ReadData           = (pfn_WinHttpReadData)(uintptr_t)GetProcAddress(g_http.mod, "WinHttpReadData");
    g_http.CloseHandle        = (pfn_WinHttpCloseHandle)(uintptr_t)GetProcAddress(g_http.mod, "WinHttpCloseHandle");
    g_http.SetOption          = (pfn_WinHttpSetOption)(uintptr_t)GetProcAddress(g_http.mod, "WinHttpSetOption");
    g_http.QueryOption        = (pfn_WinHttpQueryOption)(uintptr_t)GetProcAddress(g_http.mod, "WinHttpQueryOption");

    if (g_http.CrackUrl && g_http.Open && g_http.SetTimeouts && g_http.Connect &&
        g_http.OpenRequest && g_http.SendRequest && g_http.ReceiveResponse &&
        g_http.QueryHeaders && g_http.QueryDataAvailable && g_http.ReadData &&
        g_http.CloseHandle) {
        g_http.loaded = 1;
        return 1;
    }
    return 0;
}

/* ---- Winsock2 lifecycle -------------------------------------------------- */

static int g_wsa_started = 0;

uint8_t w32_net_init(void) {
    if (g_wsa_started) return 1;
    if (!ensure_ws2()) return 0;
    WSADATA wsa;
    if (g_wsa.WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 0;
    g_wsa_started = 1;
    return 1;
}

void w32_net_cleanup(void) {
    if (!g_wsa_started) return;
    if (g_wsa.WSACleanup) g_wsa.WSACleanup();
    g_wsa_started = 0;
}

static char *net_cstr(w32_bytes s) {
    size_t n = s.len > 0 ? (size_t)s.len : 0;
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n > 0 && s.data) memcpy(p, s.data, n);
    p[n] = '\0';
    return p;
}

static int resolve_ipv4_addr(w32_bytes host, uint16_t port, struct sockaddr_in *out) {
    memset(out, 0, sizeof(*out));
    out->sin_family = AF_INET;
    out->sin_port = w32_bswap16(port);
    if (!host.data || host.len <= 0) {
        out->sin_addr.s_addr = w32_bswap32(INADDR_ANY);
        return 1;
    }
    if (!ensure_ws2()) return 0;
    char *chost = net_cstr(host);
    if (!chost) return 0;

    if (g_wsa.inet_pton(AF_INET, chost, &out->sin_addr) == 1) {
        free(chost);
        return 1;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    struct addrinfo *res = NULL;
    int rc = g_wsa.getaddrinfo(chost, NULL, &hints, &res);
    free(chost);
    if (rc != 0 || !res) return 0;

    struct sockaddr_in *sin = (struct sockaddr_in *)res->ai_addr;
    out->sin_addr = sin->sin_addr;
    g_wsa.freeaddrinfo(res);
    return 1;
}

/* ---- TCP & UDP sockets --------------------------------------------------- */

uint64_t w32_tcp_listen(w32_bytes host, uint16_t port, int32_t backlog) {
    if (!w32_net_init()) return 0;
    struct sockaddr_in addr;
    if (!resolve_ipv4_addr(host, port, &addr)) return 0;

    SOCKET s = g_wsa.socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return 0;

    BOOL reuse = TRUE;
    g_wsa.setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
    if (g_wsa.bind(s, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        g_wsa.closesocket(s);
        return 0;
    }
    if (g_wsa.listen(s, backlog > 0 ? (int)backlog : SOMAXCONN) == SOCKET_ERROR) {
        g_wsa.closesocket(s);
        return 0;
    }
    return (uint64_t)s;
}

uint16_t w32_socket_local_port(uint64_t sock) {
    if (!sock || !ensure_ws2()) return 0;
    struct sockaddr_in addr;
    int len = sizeof(addr);
    if (g_wsa.getsockname((SOCKET)sock, (struct sockaddr *)&addr, &len) == SOCKET_ERROR) return 0;
    return w32_bswap16(addr.sin_port);
}

uint64_t w32_tcp_accept(uint64_t listen_sock) {
    if (!listen_sock || !ensure_ws2()) return 0;
    SOCKET c = g_wsa.accept((SOCKET)listen_sock, NULL, NULL);
    return c == INVALID_SOCKET ? 0 : (uint64_t)c;
}

uint64_t w32_tcp_connect(w32_bytes host, uint16_t port, uint32_t timeout_ms) {
    if (!w32_net_init()) return 0;
    struct sockaddr_in addr;
    if (!resolve_ipv4_addr(host, port, &addr)) return 0;

    SOCKET s = g_wsa.socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return 0;

    if (timeout_ms > 0) {
        u_long nb = 1;
        g_wsa.ioctlsocket(s, FIONBIO, &nb);
        int rc = g_wsa.connect(s, (struct sockaddr *)&addr, sizeof(addr));
        if (rc == SOCKET_ERROR) {
            int err = g_wsa.WSAGetLastError();
            if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS) {
                g_wsa.closesocket(s);
                return 0;
            }
            fd_set wfds, efds;
            FD_ZERO(&wfds);
            FD_ZERO(&efds);
            FD_SET(s, &wfds);
            FD_SET(s, &efds);
            struct timeval tv;
            tv.tv_sec = (long)(timeout_ms / 1000);
            tv.tv_usec = (long)((timeout_ms % 1000) * 1000);
            int sel = g_wsa.select(0, NULL, &wfds, &efds, &tv);
            int so_err = 0;
            int so_len = sizeof(so_err);
            if (sel <= 0 || w32_fd_isset(s, &efds) ||
                g_wsa.getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&so_err, &so_len) != 0 || so_err != 0) {
                g_wsa.closesocket(s);
                return 0;
            }
        }
        nb = 0;
        g_wsa.ioctlsocket(s, FIONBIO, &nb);
    } else {
        if (g_wsa.connect(s, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
            g_wsa.closesocket(s);
            return 0;
        }
    }
    return (uint64_t)s;
}

int64_t w32_socket_send(uint64_t sock, w32_bytes data) {
    if (!sock || !ensure_ws2()) return -1;
    if (data.len <= 0) return 0;
    int chunk = data.len > 0x40000000LL ? 0x40000000 : (int)data.len;
    int n = g_wsa.send((SOCKET)sock, (const char *)data.data, chunk, 0);
    return n == SOCKET_ERROR ? -1 : (int64_t)n;
}

int64_t w32_socket_recv(uint64_t sock, w32_mut_bytes buf) {
    if (!sock || !ensure_ws2()) return -1;
    if (buf.len <= 0) return 0;
    int chunk = buf.len > 0x40000000LL ? 0x40000000 : (int)buf.len;
    int n = g_wsa.recv((SOCKET)sock, (char *)buf.data, chunk, 0);
    return n == SOCKET_ERROR ? -1 : (int64_t)n;
}

uint8_t w32_socket_set_timeout(uint64_t sock, uint32_t recv_ms, uint32_t send_ms) {
    if (!sock || !ensure_ws2()) return 0;
    DWORD r = (DWORD)recv_ms;
    DWORD s = (DWORD)send_ms;
    int ok1 = g_wsa.setsockopt((SOCKET)sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&r, sizeof(r)) == 0;
    int ok2 = g_wsa.setsockopt((SOCKET)sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&s, sizeof(s)) == 0;
    return (ok1 && ok2) ? 1 : 0;
}

uint8_t w32_socket_set_nonblocking(uint64_t sock, uint8_t nb) {
    if (!sock || !ensure_ws2()) return 0;
    u_long mode = nb ? 1 : 0;
    return g_wsa.ioctlsocket((SOCKET)sock, FIONBIO, &mode) == 0 ? 1 : 0;
}

uint8_t w32_socket_shutdown(uint64_t sock) {
    if (!sock || !ensure_ws2()) return 0;
    return g_wsa.shutdown((SOCKET)sock, SD_BOTH) == 0 ? 1 : 0;
}

uint8_t w32_socket_close(uint64_t sock) {
    if (!sock || !ensure_ws2()) return 0;
    return g_wsa.closesocket((SOCKET)sock) == 0 ? 1 : 0;
}

uint64_t w32_udp_bind(w32_bytes host, uint16_t port) {
    if (!w32_net_init()) return 0;
    struct sockaddr_in addr;
    if (!resolve_ipv4_addr(host, port, &addr)) return 0;

    SOCKET s = g_wsa.socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return 0;
    if (g_wsa.bind(s, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        g_wsa.closesocket(s);
        return 0;
    }
    return (uint64_t)s;
}

int64_t w32_udp_send_to(uint64_t sock, w32_bytes host, uint16_t port, w32_bytes data) {
    if (!sock || data.len < 0 || !ensure_ws2()) return -1;
    if (data.len > 0 && !data.data) return -1;
    struct sockaddr_in addr;
    if (!resolve_ipv4_addr(host, port, &addr)) return -1;
    int chunk = data.len > 0x40000000LL ? 0x40000000 : (int)data.len;
    const char *ptr = chunk > 0 ? (const char *)data.data : "";
    int n = g_wsa.sendto(
        (SOCKET)sock, ptr, chunk, 0,
        (struct sockaddr *)&addr, sizeof(addr)
    );
    return n == SOCKET_ERROR ? -1 : (int64_t)n;
}

int64_t w32_udp_recv_from(uint64_t sock, w32_mut_bytes buf, gs_rref out_host, uint16_t *out_port) {
    if (!sock || !buf.data || buf.len <= 0 || !ensure_ws2()) return -1;
    int chunk = buf.len > 0x40000000LL ? 0x40000000 : (int)buf.len;
    struct sockaddr_in from;
    int from_len = sizeof(from);
    int n = g_wsa.recvfrom((SOCKET)sock, (char *)buf.data, chunk, 0, (struct sockaddr *)&from, &from_len);
    if (n == SOCKET_ERROR) return -1;
    char ip[INET_ADDRSTRLEN];
    if (g_wsa.inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip))) {
        gs_bld_append(out_host, ip, (int64_t)strlen(ip));
    }
    if (out_port) *out_port = w32_bswap16(from.sin_port);
    return (int64_t)n;
}

uint8_t w32_dns_resolve_ipv4(w32_bytes host, gs_rref out_ip) {
    if (!w32_net_init()) return 0;
    struct sockaddr_in addr;
    if (!resolve_ipv4_addr(host, 0, &addr)) return 0;
    char ip[INET_ADDRSTRLEN];
    if (!g_wsa.inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip))) return 0;
    gs_bld_append(out_ip, ip, (int64_t)strlen(ip));
    return 1;
}

/* ---- WinHTTP (HTTP / HTTPS client via OS TLS stack) ---------------------- */

uint8_t w32_http_request(
    w32_bytes method,
    w32_bytes url,
    w32_bytes body,
    w32_bytes content_type,
    gs_rref out_body,
    uint32_t *out_status,
    uint32_t timeout_ms
) {
    if (!ensure_winhttp()) return 0;
    w32_wide_buf wmethod, wurl, wctype;
    if (!w32_wide_init(&wmethod, method) || wmethod.len == 0) {
        w32_wide_free(&wmethod);
        return 0;
    }
    if (!w32_wide_init(&wurl, url) || wurl.len == 0) {
        w32_wide_free(&wurl);
        w32_wide_free(&wmethod);
        return 0;
    }
    if (!w32_wide_init(&wctype, content_type)) {
        w32_wide_free(&wurl);
        w32_wide_free(&wmethod);
        return 0;
    }

    wchar_t host[256];
    wchar_t path[2048];
    host[0] = L'\0';
    path[0] = L'\0';
    URL_COMPONENTS uc;
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;

    uint8_t ok = 0;
    if (!g_http.CrackUrl(wurl.ptr, (DWORD)wurl.len, 0, &uc)) {
        goto cleanup_bufs;
    }
    /* Strip any URL fragment (#...) before sending the HTTP request target. */
    wchar_t *frag = wcschr(path, L'#');
    if (frag) *frag = L'\0';

    HINTERNET hSession = g_http.Open(
        L"win32-goose/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) goto cleanup_bufs;

    int tmo = timeout_ms > 0 ? (int)timeout_ms : 10000;
    g_http.SetTimeouts(hSession, tmo, tmo, tmo, tmo);

    HINTERNET hConnect = g_http.Connect(hSession, host, uc.nPort, 0);
    if (!hConnect) {
        g_http.CloseHandle(hSession);
        goto cleanup_bufs;
    }

    DWORD req_flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = g_http.OpenRequest(
        hConnect, wmethod.ptr, path[0] ? path : L"/", NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, req_flags
    );
    if (!hRequest) {
        g_http.CloseHandle(hConnect);
        g_http.CloseHandle(hSession);
        goto cleanup_bufs;
    }

    wchar_t *hdr_alloc = NULL;
    const wchar_t *headers = WINHTTP_NO_ADDITIONAL_HEADERS;
    DWORD hdr_len = 0;
    if (wctype.len > 0) {
        size_t hdr_cap = (size_t)wctype.len + 20;
        hdr_alloc = (wchar_t *)malloc(hdr_cap * sizeof(wchar_t));
        if (!hdr_alloc) {
            g_http.CloseHandle(hRequest);
            g_http.CloseHandle(hConnect);
            g_http.CloseHandle(hSession);
            goto cleanup_bufs;
        }
        int n = swprintf(hdr_alloc, hdr_cap, L"Content-Type: %ls\r\n", wctype.ptr);
        if (n > 0) {
            headers = hdr_alloc;
            hdr_len = (DWORD)n;
        }
    }

    LPVOID opt_body = (body.data && body.len > 0) ? (LPVOID)body.data : WINHTTP_NO_REQUEST_DATA;
    DWORD body_len = (body.data && body.len > 0) ? (DWORD)body.len : 0;

    if (g_http.SendRequest(hRequest, headers, hdr_len, opt_body, body_len, body_len, 0) &&
        g_http.ReceiveResponse(hRequest, NULL)) {
        DWORD status = 0;
        DWORD sz = sizeof(status);
        g_http.QueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status, &sz, WINHTTP_NO_HEADER_INDEX
        );
        if (out_status) *out_status = (uint32_t)status;

        uint8_t chunk[8192];
        uint8_t read_ok = 1;
        for (;;) {
            DWORD avail = 0;
            if (!g_http.QueryDataAvailable(hRequest, &avail)) {
                read_ok = 0;
                break;
            }
            if (avail == 0) break;
            while (avail > 0) {
                DWORD to_read = avail > sizeof(chunk) ? (DWORD)sizeof(chunk) : avail;
                DWORD got = 0;
                if (!g_http.ReadData(hRequest, chunk, to_read, &got)) {
                    read_ok = 0;
                    break;
                }
                if (got == 0) break;
                gs_bld_append(out_body, chunk, (int64_t)got);
                avail -= got;
            }
            if (!read_ok) break;
        }
        ok = read_ok;
    }

    free(hdr_alloc);
    g_http.CloseHandle(hRequest);
    g_http.CloseHandle(hConnect);
    g_http.CloseHandle(hSession);

cleanup_bufs:
    w32_wide_free(&wctype);
    w32_wide_free(&wurl);
    w32_wide_free(&wmethod);
    return ok;
}

/* ---- WSAPoll & WinHTTP custom/response headers --------------------------- */

int32_t w32_socket_poll(uint64_t sock, uint8_t write_check, int32_t timeout_ms) {
    if (!sock || !ensure_ws2()) return -1;
    WSAPOLLFD pfd;
    pfd.fd = (SOCKET)sock;
    pfd.events = write_check ? POLLWRNORM : POLLRDNORM;
    pfd.revents = 0;
    int r = g_wsa.WSAPoll(&pfd, 1, (INT)timeout_ms);
    if (r <= 0) return (int32_t)r;
    if (pfd.revents & (POLLNVAL | POLLERR)) return -1;
    if (pfd.revents & POLLHUP) return write_check ? -1 : 1;
    return (pfd.revents & pfd.events) ? 1 : 0;
}

uint8_t w32_http_request_ex(
    w32_bytes method,
    w32_bytes url,
    w32_bytes headers_crlf,
    w32_bytes body,
    gs_rref out_body,
    uint32_t *out_status,
    gs_rref out_resp_headers,
    uint32_t timeout_ms
) {
    if (!ensure_winhttp()) return 0;
    w32_wide_buf wmethod, wurl, whdrs;
    if (!w32_wide_init(&wmethod, method) || wmethod.len == 0) {
        w32_wide_free(&wmethod);
        return 0;
    }
    if (!w32_wide_init(&wurl, url) || wurl.len == 0) {
        w32_wide_free(&wurl);
        w32_wide_free(&wmethod);
        return 0;
    }
    if (!w32_wide_init(&whdrs, headers_crlf)) {
        w32_wide_free(&wurl);
        w32_wide_free(&wmethod);
        return 0;
    }

    wchar_t host[256];
    wchar_t path[2048];
    host[0] = L'\0';
    path[0] = L'\0';
    URL_COMPONENTS uc;
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;

    uint8_t ok = 0;
    if (!g_http.CrackUrl(wurl.ptr, (DWORD)wurl.len, 0, &uc)) goto done_ex;
    wchar_t *frag = wcschr(path, L'#');
    if (frag) *frag = L'\0';

    HINTERNET hSession = g_http.Open(
        L"win32-goose/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) goto done_ex;

    int tmo = timeout_ms > 0 ? (int)timeout_ms : 10000;
    g_http.SetTimeouts(hSession, tmo, tmo, tmo, tmo);

    HINTERNET hConnect = g_http.Connect(hSession, host, uc.nPort, 0);
    if (!hConnect) {
        g_http.CloseHandle(hSession);
        goto done_ex;
    }

    DWORD req_flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = g_http.OpenRequest(
        hConnect, wmethod.ptr, path[0] ? path : L"/", NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, req_flags
    );
    if (!hRequest) {
        g_http.CloseHandle(hConnect);
        g_http.CloseHandle(hSession);
        goto done_ex;
    }

    const wchar_t *hdr_ptr = whdrs.len > 0 ? whdrs.ptr : WINHTTP_NO_ADDITIONAL_HEADERS;
    DWORD hdr_len = whdrs.len > 0 ? (DWORD)whdrs.len : 0;
    LPVOID opt_body = (body.data && body.len > 0) ? (LPVOID)body.data : WINHTTP_NO_REQUEST_DATA;
    DWORD body_len = (body.data && body.len > 0) ? (DWORD)body.len : 0;

    if (g_http.SendRequest(hRequest, hdr_ptr, hdr_len, opt_body, body_len, body_len, 0) &&
        g_http.ReceiveResponse(hRequest, NULL)) {
        DWORD status = 0;
        DWORD sz = sizeof(status);
        g_http.QueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status, &sz, WINHTTP_NO_HEADER_INDEX
        );
        if (out_status) *out_status = (uint32_t)status;

        DWORD raw_hdr_bytes = 0;
        g_http.QueryHeaders(
            hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
            NULL, &raw_hdr_bytes, WINHTTP_NO_HEADER_INDEX
        );
        if (raw_hdr_bytes > 0) {
            wchar_t *rbuf = (wchar_t *)malloc((size_t)raw_hdr_bytes + sizeof(wchar_t));
            if (rbuf) {
                if (g_http.QueryHeaders(
                        hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
                        rbuf, &raw_hdr_bytes, WINHTTP_NO_HEADER_INDEX)) {
                    rbuf[raw_hdr_bytes / sizeof(wchar_t)] = L'\0';
                    w32_append_wide_as_utf8(rbuf, -1, out_resp_headers);
                }
                free(rbuf);
            }
        }

        uint8_t chunk[8192];
        uint8_t read_ok = 1;
        for (;;) {
            DWORD avail = 0;
            if (!g_http.QueryDataAvailable(hRequest, &avail)) {
                read_ok = 0;
                break;
            }
            if (avail == 0) break;
            while (avail > 0) {
                DWORD to_read = avail > sizeof(chunk) ? (DWORD)sizeof(chunk) : avail;
                DWORD got = 0;
                if (!g_http.ReadData(hRequest, chunk, to_read, &got)) {
                    read_ok = 0;
                    break;
                }
                if (got == 0) break;
                gs_bld_append(out_body, chunk, (int64_t)got);
                avail -= got;
            }
            if (!read_ok) break;
        }
        ok = read_ok;
    }

    g_http.CloseHandle(hRequest);
    g_http.CloseHandle(hConnect);
    g_http.CloseHandle(hSession);

done_ex:
    w32_wide_free(&whdrs);
    w32_wide_free(&wurl);
    w32_wide_free(&wmethod);
    return ok;
}

/* ---- Background Non-Blocking WinHTTP GET Queue --------------------------- */

#define W32_ASYNC_QCAP        64
#define W32_ASYNC_MAX_WORKERS 4

typedef struct {
    uint64_t req_id;
    LONG gen;
    wchar_t *wurl;
    wchar_t *whdrs;
    uint32_t max_bytes;
    uint32_t timeout_ms;
    HWND notify_hwnd;
} w32_http_job;

typedef struct {
    w32_http_async_meta meta;
    LONG gen;
    uint8_t *body;
    size_t body_len;
    char *content_type;
    char *final_url;
} w32_http_done;

static volatile LONG g_async_init_state = 0; /* 0=uninit, 1=initing, 2=ready */
static CRITICAL_SECTION g_async_cs;
static volatile LONG g_async_gen = 1;
static uint64_t g_async_next_id = 1;
static int g_async_active_workers = 0;

static w32_http_job g_async_pending[W32_ASYNC_QCAP];
static int g_async_phead = 0;
static int g_async_ptail = 0;
static int g_async_pcount = 0;

static w32_http_done g_async_done[W32_ASYNC_QCAP];
static int g_async_dhead = 0;
static int g_async_dtail = 0;
static int g_async_dcount = 0;

static void async_ensure_cs(void) {
    if (InterlockedCompareExchange(&g_async_init_state, 1, 0) == 0) {
        InitializeCriticalSection(&g_async_cs);
        InterlockedExchange(&g_async_init_state, 2);
        return;
    }
    while (InterlockedCompareExchange(&g_async_init_state, 2, 2) != 2) {
        Sleep(0);
    }
}

static wchar_t *async_dup_utf8_to_wide(w32_bytes s) {
    if (!s.data || s.len <= 0) return NULL;
    int n = MultiByteToWideChar(CP_UTF8, 0, (const char *)s.data, (int)s.len, NULL, 0);
    if (n <= 0) return NULL;
    wchar_t *out = (wchar_t *)malloc((size_t)(n + 1) * sizeof(wchar_t));
    if (!out) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, (const char *)s.data, (int)s.len, out, n);
    out[n] = L'\0';
    return out;
}

static char *async_wide_to_utf8_alloc(const wchar_t *ws) {
    if (!ws || !ws[0]) return NULL;
    int n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, NULL, 0, NULL, NULL);
    if (n <= 0) return NULL;
    char *out = (char *)malloc((size_t)n);
    if (!out) return NULL;
    if (WideCharToMultiByte(CP_UTF8, 0, ws, -1, out, n, NULL, NULL) <= 0) {
        free(out);
        return NULL;
    }
    return out;
}

static void async_free_job(w32_http_job *j) {
    free(j->wurl);
    free(j->whdrs);
    memset(j, 0, sizeof(*j));
}

static void async_free_done(w32_http_done *d) {
    free(d->body);
    free(d->content_type);
    free(d->final_url);
    memset(d, 0, sizeof(*d));
}

static void async_execute_job(const w32_http_job *job, w32_http_done *res) {
    memset(res, 0, sizeof(*res));
    res->meta.req_id = job->req_id;
    res->gen = job->gen;

    if (!job->wurl || !job->wurl[0] || !ensure_winhttp()) return;
    size_t url_len = wcslen(job->wurl);

    wchar_t host[256];
    wchar_t path[2048];
    host[0] = L'\0';
    path[0] = L'\0';
    URL_COMPONENTS uc;
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;

    if (!g_http.CrackUrl(job->wurl, (DWORD)url_len, 0, &uc)) return;
    wchar_t *frag = wcschr(path, L'#');
    if (frag) *frag = L'\0';

    HINTERNET hSession = g_http.Open(
        L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) YuLauncher/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) return;

    int tmo = job->timeout_ms > 0 ? (int)job->timeout_ms : 6000;
    g_http.SetTimeouts(hSession, tmo, tmo, tmo, tmo);

    HINTERNET hConnect = g_http.Connect(hSession, host, uc.nPort, 0);
    if (!hConnect) {
        g_http.CloseHandle(hSession);
        return;
    }

    DWORD req_flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = g_http.OpenRequest(
        hConnect, L"GET", path[0] ? path : L"/", NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, req_flags
    );
    if (!hRequest) {
        g_http.CloseHandle(hConnect);
        g_http.CloseHandle(hSession);
        return;
    }

    if (g_http.SetOption) {
        DWORD decomp = WINHTTP_DECOMPRESSION_FLAG_ALL;
        g_http.SetOption(hRequest, WINHTTP_OPTION_DECOMPRESSION, &decomp, sizeof(decomp));
    }

    const wchar_t *hdr_ptr = (job->whdrs && job->whdrs[0]) ? job->whdrs : WINHTTP_NO_ADDITIONAL_HEADERS;
    DWORD hdr_len = (job->whdrs && job->whdrs[0]) ? (DWORD)wcslen(job->whdrs) : 0;

    if (g_http.SendRequest(hRequest, hdr_ptr, hdr_len, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        g_http.ReceiveResponse(hRequest, NULL)) {
        DWORD status = 0;
        DWORD sz = sizeof(status);
        g_http.QueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status, &sz, WINHTTP_NO_HEADER_INDEX
        );
        res->meta.status = (uint32_t)status;

        wchar_t ct_w[256];
        DWORD ct_bytes = sizeof(ct_w);
        if (g_http.QueryHeaders(
                hRequest, WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX,
                ct_w, &ct_bytes, WINHTTP_NO_HEADER_INDEX)) {
            ct_w[(sizeof(ct_w) / sizeof(wchar_t)) - 1] = L'\0';
            res->content_type = async_wide_to_utf8_alloc(ct_w);
        }

        if (g_http.QueryOption) {
            DWORD furl_bytes = 0;
            g_http.QueryOption(hRequest, WINHTTP_OPTION_URL, NULL, &furl_bytes);
            if (furl_bytes > 0 && furl_bytes <= 16384) {
                wchar_t *furl_w = (wchar_t *)malloc((size_t)furl_bytes + sizeof(wchar_t));
                if (furl_w) {
                    if (g_http.QueryOption(hRequest, WINHTTP_OPTION_URL, furl_w, &furl_bytes)) {
                        furl_w[furl_bytes / sizeof(wchar_t)] = L'\0';
                        res->final_url = async_wide_to_utf8_alloc(furl_w);
                    }
                    free(furl_w);
                }
            }
        }

        size_t limit = job->max_bytes > 0 ? (size_t)job->max_bytes : (2u * 1024u * 1024u);
        size_t cap = 0;
        size_t len = 0;
        uint8_t *buf = NULL;
        uint8_t chunk[8192];
        uint8_t read_ok = 1;
        uint8_t truncated = 0;

        for (;;) {
            if (InterlockedCompareExchange(&g_async_gen, 0, 0) != job->gen) {
                read_ok = 0;
                break;
            }
            DWORD avail = 0;
            if (!g_http.QueryDataAvailable(hRequest, &avail)) {
                read_ok = 0;
                break;
            }
            if (avail == 0) break;
            while (avail > 0) {
                if (len >= limit) {
                    truncated = 1;
                    break;
                }
                DWORD to_read = avail > sizeof(chunk) ? (DWORD)sizeof(chunk) : avail;
                if ((size_t)to_read > limit - len) {
                    to_read = (DWORD)(limit - len);
                }
                DWORD got = 0;
                if (!g_http.ReadData(hRequest, chunk, to_read, &got)) {
                    read_ok = 0;
                    break;
                }
                if (got == 0) break;
                if (len + (size_t)got > cap) {
                    size_t ncap = cap == 0 ? 4096 : cap * 2;
                    while (ncap < len + (size_t)got) ncap *= 2;
                    if (ncap > limit) ncap = limit;
                    uint8_t *nbuf = (uint8_t *)realloc(buf, ncap);
                    if (!nbuf) {
                        read_ok = 0;
                        break;
                    }
                    buf = nbuf;
                    cap = ncap;
                }
                memcpy(buf + len, chunk, (size_t)got);
                len += (size_t)got;
                avail -= got;
            }
            if (!read_ok || truncated) break;
        }

        if (read_ok) {
            res->body = buf;
            res->body_len = len;
            res->meta.ok = 1;
            res->meta.truncated = truncated;
        } else {
            free(buf);
        }
    }

    g_http.CloseHandle(hRequest);
    g_http.CloseHandle(hConnect);
    g_http.CloseHandle(hSession);
}

static DWORD WINAPI w32_http_async_worker(LPVOID param) {
    (void)param;
    for (;;) {
        w32_http_job job;
        memset(&job, 0, sizeof(job));

        EnterCriticalSection(&g_async_cs);
        if (g_async_pcount <= 0) {
            g_async_active_workers--;
            LeaveCriticalSection(&g_async_cs);
            return 0;
        }
        job = g_async_pending[g_async_phead];
        memset(&g_async_pending[g_async_phead], 0, sizeof(w32_http_job));
        g_async_phead = (g_async_phead + 1) % W32_ASYNC_QCAP;
        g_async_pcount--;
        LeaveCriticalSection(&g_async_cs);

        if (InterlockedCompareExchange(&g_async_gen, 0, 0) != job.gen) {
            async_free_job(&job);
            continue;
        }

        w32_http_done res;
        async_execute_job(&job, &res);
        HWND notify = job.notify_hwnd;
        async_free_job(&job);

        EnterCriticalSection(&g_async_cs);
        int keep = (InterlockedCompareExchange(&g_async_gen, 0, 0) == res.gen);
        if (keep) {
            if (g_async_dcount >= W32_ASYNC_QCAP) {
                async_free_done(&g_async_done[g_async_dhead]);
                g_async_dhead = (g_async_dhead + 1) % W32_ASYNC_QCAP;
                g_async_dcount--;
            }
            g_async_done[g_async_dtail] = res;
            g_async_dtail = (g_async_dtail + 1) % W32_ASYNC_QCAP;
            g_async_dcount++;
        } else {
            async_free_done(&res);
        }
        LeaveCriticalSection(&g_async_cs);

        if (keep && notify && IsWindow(notify)) {
            PostMessageW(notify, W32_WM_HTTP_ASYNC, (WPARAM)(res.meta.req_id & 0xFFFFFFFFu), 0);
        }
    }
}

uint64_t w32_http_get_async(
    w32_bytes url,
    w32_bytes headers_crlf,
    uint32_t max_bytes,
    uint32_t timeout_ms,
    uint64_t notify_hwnd
) {
    if (!url.data || url.len <= 0 || !ensure_winhttp()) return 0;
    wchar_t *wurl = async_dup_utf8_to_wide(url);
    if (!wurl) return 0;
    wchar_t *whdrs = (headers_crlf.data && headers_crlf.len > 0) ? async_dup_utf8_to_wide(headers_crlf) : NULL;

    async_ensure_cs();
    EnterCriticalSection(&g_async_cs);
    if (g_async_pcount >= W32_ASYNC_QCAP) {
        LeaveCriticalSection(&g_async_cs);
        free(whdrs);
        free(wurl);
        return 0;
    }

    uint64_t rid = g_async_next_id++;
    if (rid == 0) rid = g_async_next_id++;
    w32_http_job *slot = &g_async_pending[g_async_ptail];
    slot->req_id = rid;
    slot->gen = InterlockedCompareExchange(&g_async_gen, 0, 0);
    slot->wurl = wurl;
    slot->whdrs = whdrs;
    slot->max_bytes = max_bytes;
    slot->timeout_ms = timeout_ms;
    slot->notify_hwnd = (HWND)(uintptr_t)notify_hwnd;
    g_async_ptail = (g_async_ptail + 1) % W32_ASYNC_QCAP;
    g_async_pcount++;

    if (g_async_active_workers < W32_ASYNC_MAX_WORKERS) {
        g_async_active_workers++;
        HANDLE th = CreateThread(NULL, 0, w32_http_async_worker, NULL, 0, NULL);
        if (th) {
            CloseHandle(th);
        } else {
            g_async_active_workers--;
        }
    }
    LeaveCriticalSection(&g_async_cs);
    return rid;
}

uint8_t w32_http_poll_async(
    w32_http_async_meta *out_meta,
    gs_rref out_body,
    gs_rref out_content_type,
    gs_rref out_final_url
) {
    if (InterlockedCompareExchange(&g_async_init_state, 2, 2) != 2) return 0;
    w32_http_done item;
    memset(&item, 0, sizeof(item));

    EnterCriticalSection(&g_async_cs);
    if (g_async_dcount <= 0) {
        LeaveCriticalSection(&g_async_cs);
        return 0;
    }
    item = g_async_done[g_async_dhead];
    memset(&g_async_done[g_async_dhead], 0, sizeof(w32_http_done));
    g_async_dhead = (g_async_dhead + 1) % W32_ASYNC_QCAP;
    g_async_dcount--;
    LeaveCriticalSection(&g_async_cs);

    if (out_meta) *out_meta = item.meta;
    if (item.body && item.body_len > 0) {
        gs_bld_append(out_body, item.body, (int64_t)item.body_len);
    }
    if (item.content_type && item.content_type[0]) {
        gs_bld_append(out_content_type, item.content_type, (int64_t)strlen(item.content_type));
    }
    if (item.final_url && item.final_url[0]) {
        gs_bld_append(out_final_url, item.final_url, (int64_t)strlen(item.final_url));
    }
    async_free_done(&item);
    return 1;
}

void w32_http_cancel_all_async(void) {
    if (InterlockedCompareExchange(&g_async_init_state, 2, 2) != 2) return;
    EnterCriticalSection(&g_async_cs);
    InterlockedIncrement(&g_async_gen);
    while (g_async_pcount > 0) {
        async_free_job(&g_async_pending[g_async_phead]);
        g_async_phead = (g_async_phead + 1) % W32_ASYNC_QCAP;
        g_async_pcount--;
    }
    while (g_async_dcount > 0) {
        async_free_done(&g_async_done[g_async_dhead]);
        g_async_dhead = (g_async_dhead + 1) % W32_ASYNC_QCAP;
        g_async_dcount--;
    }
    LeaveCriticalSection(&g_async_cs);
}
