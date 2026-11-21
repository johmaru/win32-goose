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

/* ---- Winsock2 lifecycle -------------------------------------------------- */

static int g_wsa_started = 0;

uint8_t w32_net_init(void) {
    if (g_wsa_started) return 1;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 0;
    g_wsa_started = 1;
    return 1;
}

void w32_net_cleanup(void) {
    if (!g_wsa_started) return;
    WSACleanup();
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
    out->sin_port = htons(port);
    if (!host.data || host.len <= 0) {
        out->sin_addr.s_addr = htonl(INADDR_ANY);
        return 1;
    }
    char *chost = net_cstr(host);
    if (!chost) return 0;

    if (inet_pton(AF_INET, chost, &out->sin_addr) == 1) {
        free(chost);
        return 1;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    struct addrinfo *res = NULL;
    int rc = getaddrinfo(chost, NULL, &hints, &res);
    free(chost);
    if (rc != 0 || !res) return 0;

    struct sockaddr_in *sin = (struct sockaddr_in *)res->ai_addr;
    out->sin_addr = sin->sin_addr;
    freeaddrinfo(res);
    return 1;
}

/* ---- TCP & UDP sockets --------------------------------------------------- */

uint64_t w32_tcp_listen(w32_bytes host, uint16_t port, int32_t backlog) {
    if (!w32_net_init()) return 0;
    struct sockaddr_in addr;
    if (!resolve_ipv4_addr(host, port, &addr)) return 0;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return 0;

    BOOL reuse = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s);
        return 0;
    }
    if (listen(s, backlog > 0 ? (int)backlog : SOMAXCONN) == SOCKET_ERROR) {
        closesocket(s);
        return 0;
    }
    return (uint64_t)s;
}

uint16_t w32_socket_local_port(uint64_t sock) {
    if (!sock) return 0;
    struct sockaddr_in addr;
    int len = sizeof(addr);
    if (getsockname((SOCKET)sock, (struct sockaddr *)&addr, &len) == SOCKET_ERROR) return 0;
    return ntohs(addr.sin_port);
}

uint64_t w32_tcp_accept(uint64_t listen_sock) {
    if (!listen_sock) return 0;
    SOCKET c = accept((SOCKET)listen_sock, NULL, NULL);
    return c == INVALID_SOCKET ? 0 : (uint64_t)c;
}

uint64_t w32_tcp_connect(w32_bytes host, uint16_t port, uint32_t timeout_ms) {
    if (!w32_net_init()) return 0;
    struct sockaddr_in addr;
    if (!resolve_ipv4_addr(host, port, &addr)) return 0;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return 0;

    if (timeout_ms > 0) {
        u_long nb = 1;
        ioctlsocket(s, FIONBIO, &nb);
        int rc = connect(s, (struct sockaddr *)&addr, sizeof(addr));
        if (rc == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS) {
                closesocket(s);
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
            int sel = select(0, NULL, &wfds, &efds, &tv);
            int so_err = 0;
            int so_len = sizeof(so_err);
            if (sel <= 0 || FD_ISSET(s, &efds) ||
                getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&so_err, &so_len) != 0 || so_err != 0) {
                closesocket(s);
                return 0;
            }
        }
        nb = 0;
        ioctlsocket(s, FIONBIO, &nb);
    } else {
        if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(s);
            return 0;
        }
    }
    return (uint64_t)s;
}

int64_t w32_socket_send(uint64_t sock, w32_bytes data) {
    if (!sock) return -1;
    if (data.len <= 0) return 0;
    int chunk = data.len > 0x40000000LL ? 0x40000000 : (int)data.len;
    int n = send((SOCKET)sock, (const char *)data.data, chunk, 0);
    return n == SOCKET_ERROR ? -1 : (int64_t)n;
}

int64_t w32_socket_recv(uint64_t sock, w32_mut_bytes buf) {
    if (!sock) return -1;
    if (buf.len <= 0) return 0;
    int chunk = buf.len > 0x40000000LL ? 0x40000000 : (int)buf.len;
    int n = recv((SOCKET)sock, (char *)buf.data, chunk, 0);
    return n == SOCKET_ERROR ? -1 : (int64_t)n;
}

uint8_t w32_socket_set_timeout(uint64_t sock, uint32_t recv_ms, uint32_t send_ms) {
    if (!sock) return 0;
    DWORD r = (DWORD)recv_ms;
    DWORD s = (DWORD)send_ms;
    int ok1 = setsockopt((SOCKET)sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&r, sizeof(r)) == 0;
    int ok2 = setsockopt((SOCKET)sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&s, sizeof(s)) == 0;
    return (ok1 && ok2) ? 1 : 0;
}

uint8_t w32_socket_set_nonblocking(uint64_t sock, uint8_t nb) {
    if (!sock) return 0;
    u_long mode = nb ? 1 : 0;
    return ioctlsocket((SOCKET)sock, FIONBIO, &mode) == 0 ? 1 : 0;
}

uint8_t w32_socket_shutdown(uint64_t sock) {
    if (!sock) return 0;
    return shutdown((SOCKET)sock, SD_BOTH) == 0 ? 1 : 0;
}

uint8_t w32_socket_close(uint64_t sock) {
    if (!sock) return 0;
    return closesocket((SOCKET)sock) == 0 ? 1 : 0;
}

uint64_t w32_udp_bind(w32_bytes host, uint16_t port) {
    if (!w32_net_init()) return 0;
    struct sockaddr_in addr;
    if (!resolve_ipv4_addr(host, port, &addr)) return 0;

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return 0;
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s);
        return 0;
    }
    return (uint64_t)s;
}

int64_t w32_udp_send_to(uint64_t sock, w32_bytes host, uint16_t port, w32_bytes data) {
    if (!sock) return -1;
    struct sockaddr_in addr;
    if (!resolve_ipv4_addr(host, port, &addr)) return -1;
    int n = sendto(
        (SOCKET)sock, (const char *)data.data, (int)data.len, 0,
        (struct sockaddr *)&addr, sizeof(addr)
    );
    return n == SOCKET_ERROR ? -1 : (int64_t)n;
}

int64_t w32_udp_recv_from(uint64_t sock, w32_mut_bytes buf, gs_rref out_host, uint16_t *out_port) {
    if (!sock || !buf.data || buf.len <= 0) return -1;
    struct sockaddr_in from;
    int from_len = sizeof(from);
    int n = recvfrom((SOCKET)sock, (char *)buf.data, (int)buf.len, 0, (struct sockaddr *)&from, &from_len);
    if (n == SOCKET_ERROR) return -1;
    char ip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip))) {
        gs_bld_append(out_host, ip, (int64_t)strlen(ip));
    }
    if (out_port) *out_port = ntohs(from.sin_port);
    return (int64_t)n;
}

uint8_t w32_dns_resolve_ipv4(w32_bytes host, gs_rref out_ip) {
    if (!w32_net_init()) return 0;
    struct sockaddr_in addr;
    if (!resolve_ipv4_addr(host, 0, &addr)) return 0;
    char ip[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip))) return 0;
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
    w32_wide_init(&wctype, content_type);

    wchar_t host[256];
    wchar_t path[2048];
    URL_COMPONENTS uc;
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;

    uint8_t ok = 0;
    if (!WinHttpCrackUrl(wurl.ptr, (DWORD)wurl.len, 0, &uc)) {
        goto cleanup_bufs;
    }

    HINTERNET hSession = WinHttpOpen(
        L"win32-goose/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) goto cleanup_bufs;

    int tmo = timeout_ms > 0 ? (int)timeout_ms : 10000;
    WinHttpSetTimeouts(hSession, tmo, tmo, tmo, tmo);

    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        goto cleanup_bufs;
    }

    DWORD req_flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, wmethod.ptr, path[0] ? path : L"/", NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, req_flags
    );
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        goto cleanup_bufs;
    }

    wchar_t hdr_buf[512];
    const wchar_t *headers = WINHTTP_NO_ADDITIONAL_HEADERS;
    DWORD hdr_len = 0;
    if (wctype.len > 0) {
        int n = _snwprintf(hdr_buf, 512, L"Content-Type: %s\r\n", wctype.ptr);
        if (n > 0 && n < 512) {
            headers = hdr_buf;
            hdr_len = (DWORD)n;
        }
    }

    LPVOID opt_body = (body.data && body.len > 0) ? (LPVOID)body.data : WINHTTP_NO_REQUEST_DATA;
    DWORD body_len = (body.data && body.len > 0) ? (DWORD)body.len : 0;

    if (WinHttpSendRequest(hRequest, headers, hdr_len, opt_body, body_len, body_len, 0) &&
        WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD status = 0;
        DWORD sz = sizeof(status);
        WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status, &sz, WINHTTP_NO_HEADER_INDEX
        );
        if (out_status) *out_status = (uint32_t)status;

        uint8_t chunk[8192];
        for (;;) {
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &avail) || avail == 0) break;
            while (avail > 0) {
                DWORD to_read = avail > sizeof(chunk) ? (DWORD)sizeof(chunk) : avail;
                DWORD got = 0;
                if (!WinHttpReadData(hRequest, chunk, to_read, &got) || got == 0) break;
                gs_bld_append(out_body, chunk, (int64_t)got);
                avail -= got;
            }
        }
        ok = 1;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

cleanup_bufs:
    w32_wide_free(&wctype);
    w32_wide_free(&wurl);
    w32_wide_free(&wmethod);
    return ok;
}

/* ---- WSAPoll & WinHTTP custom/response headers --------------------------- */

int32_t w32_socket_poll(uint64_t sock, uint8_t write_check, int32_t timeout_ms) {
    if (!sock) return -1;
    WSAPOLLFD pfd;
    pfd.fd = (SOCKET)sock;
    pfd.events = write_check ? POLLWRNORM : POLLRDNORM;
    pfd.revents = 0;
    int r = WSAPoll(&pfd, 1, (INT)timeout_ms);
    if (r <= 0) return (int32_t)r;
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) return 1;
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
    w32_wide_init(&whdrs, headers_crlf);

    wchar_t host[256];
    wchar_t path[2048];
    URL_COMPONENTS uc;
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;

    uint8_t ok = 0;
    if (!WinHttpCrackUrl(wurl.ptr, (DWORD)wurl.len, 0, &uc)) goto done_ex;

    HINTERNET hSession = WinHttpOpen(
        L"win32-goose/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) goto done_ex;

    int tmo = timeout_ms > 0 ? (int)timeout_ms : 10000;
    WinHttpSetTimeouts(hSession, tmo, tmo, tmo, tmo);

    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        goto done_ex;
    }

    DWORD req_flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, wmethod.ptr, path[0] ? path : L"/", NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, req_flags
    );
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        goto done_ex;
    }

    const wchar_t *hdr_ptr = whdrs.len > 0 ? whdrs.ptr : WINHTTP_NO_ADDITIONAL_HEADERS;
    DWORD hdr_len = whdrs.len > 0 ? (DWORD)whdrs.len : 0;
    LPVOID opt_body = (body.data && body.len > 0) ? (LPVOID)body.data : WINHTTP_NO_REQUEST_DATA;
    DWORD body_len = (body.data && body.len > 0) ? (DWORD)body.len : 0;

    if (WinHttpSendRequest(hRequest, hdr_ptr, hdr_len, opt_body, body_len, body_len, 0) &&
        WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD status = 0;
        DWORD sz = sizeof(status);
        WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status, &sz, WINHTTP_NO_HEADER_INDEX
        );
        if (out_status) *out_status = (uint32_t)status;

        DWORD raw_hdr_bytes = 0;
        WinHttpQueryHeaders(
            hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
            NULL, &raw_hdr_bytes, WINHTTP_NO_HEADER_INDEX
        );
        if (raw_hdr_bytes > 0) {
            wchar_t *rbuf = (wchar_t *)malloc((size_t)raw_hdr_bytes + sizeof(wchar_t));
            if (rbuf) {
                if (WinHttpQueryHeaders(
                        hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
                        rbuf, &raw_hdr_bytes, WINHTTP_NO_HEADER_INDEX)) {
                    rbuf[raw_hdr_bytes / sizeof(wchar_t)] = L'\0';
                    w32_append_wide_as_utf8(rbuf, -1, out_resp_headers);
                }
                free(rbuf);
            }
        }

        uint8_t chunk[8192];
        for (;;) {
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &avail) || avail == 0) break;
            while (avail > 0) {
                DWORD to_read = avail > sizeof(chunk) ? (DWORD)sizeof(chunk) : avail;
                DWORD got = 0;
                if (!WinHttpReadData(hRequest, chunk, to_read, &got) || got == 0) break;
                gs_bld_append(out_body, chunk, (int64_t)got);
                avail -= got;
            }
        }
        ok = 1;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

done_ex:
    w32_wide_free(&whdrs);
    w32_wide_free(&wurl);
    w32_wide_free(&wmethod);
    return ok;
}
