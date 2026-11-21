// Win32 Networking & Internet: Winsock2 (TCP, UDP, DNS) and WinHTTP (HTTP/HTTPS).
namespace win32;

import types;

extern fn net_init() -> bool = "w32_net_init";
extern fn net_cleanup() = "w32_net_cleanup";

extern fn shim_tcp_listen(host: const u8[:], port: u16, backlog: i32) -> u64 = "w32_tcp_listen";
extern fn shim_socket_local_port(sock: u64) -> u16 = "w32_socket_local_port";
extern fn shim_tcp_accept(listen_sock: u64) -> u64 = "w32_tcp_accept";
extern fn shim_tcp_connect(host: const u8[:], port: u16, timeout_ms: u32) -> u64 = "w32_tcp_connect";
extern fn shim_socket_send(sock: u64, data: const u8[:]) -> i64 = "w32_socket_send";
extern fn shim_socket_recv(sock: u64, buf: u8[:]) -> i64 = "w32_socket_recv";
extern fn shim_socket_set_timeout(sock: u64, recv_ms: u32, send_ms: u32) -> bool = "w32_socket_set_timeout";
extern fn shim_socket_set_nonblocking(sock: u64, nb: bool) -> bool = "w32_socket_set_nonblocking";
extern fn shim_socket_shutdown(sock: u64) -> bool = "w32_socket_shutdown";
extern fn shim_socket_close(sock: u64) -> bool = "w32_socket_close";

extern fn shim_udp_bind(host: const u8[:], port: u16) -> u64 = "w32_udp_bind";
extern fn shim_udp_send_to(sock: u64, host: const u8[:], port: u16, data: const u8[:]) -> i64 = "w32_udp_send_to";
extern fn shim_udp_recv_from(sock: u64, buf: u8[:], out_host: u8[>..]&, out_port: u16&) -> i64 = "w32_udp_recv_from";
extern fn dns_resolve_ipv4(host: const u8[:], out_ip: u8[>..]&) -> bool = "w32_dns_resolve_ipv4";

extern fn http_request(
    method: const u8[:],
    url: const u8[:],
    body: const u8[:],
    content_type: const u8[:],
    out_body: u8[>..]&,
    out_status: u32&,
    timeout_ms: u32 = 10000
) -> bool = "w32_http_request";

extern fn shim_socket_poll(sock: u64, write_check: bool, timeout_ms: i32) -> i32 = "w32_socket_poll";

extern fn http_request_ex(
    method: const u8[:],
    url: const u8[:],
    headers_crlf: const u8[:],
    body: const u8[:],
    out_body: u8[>..]&,
    out_status: u32&,
    out_resp_headers: u8[>..]&,
    timeout_ms: u32 = 10000
) -> bool = "w32_http_request_ex";

// --- Winsock2 wrappers (Hsocket) --------------------------------------------

fn tcp_listen(host: const u8[:], port: u16, backlog: i32 = 16) -> Hsocket {
    return Hsocket { raw: shim_tcp_listen(host, port, backlog) };
}

fn tcp_connect(host: const u8[:], port: u16, timeout_ms: u32 = 5000) -> Hsocket {
    return Hsocket { raw: shim_tcp_connect(host, port, timeout_ms) };
}

fn udp_bind(host: const u8[:], port: u16) -> Hsocket {
    return Hsocket { raw: shim_udp_bind(host, port) };
}

fn local_port(s: Hsocket) -> u16 { shim_socket_local_port(s.raw) }
fn accept(s: Hsocket) -> Hsocket { Hsocket { raw: shim_tcp_accept(s.raw) } }
fn send(s: Hsocket, data: const u8[:]) -> i64 { shim_socket_send(s.raw, data) }

fn send_all(s: Hsocket, data: const u8[:]) -> bool {
    var off: i64 = 0;
    while off < data.len {
        let n = shim_socket_send(s.raw, data[off..]);
        if n <= 0 { return false; }
        off += n;
    }
    return true;
}

fn recv(s: Hsocket, buf: u8[:]) -> i64 { shim_socket_recv(s.raw, buf) }
fn set_timeout(s: Hsocket, recv_ms: u32, send_ms: u32) -> bool { shim_socket_set_timeout(s.raw, recv_ms, send_ms) }
fn set_nonblocking(s: Hsocket, nb: bool) -> bool { shim_socket_set_nonblocking(s.raw, nb) }
fn shutdown(s: Hsocket) -> bool { s.is_valid() && shim_socket_shutdown(s.raw) }
fn close(s: Hsocket) -> bool { s.is_valid() && shim_socket_close(s.raw) }

// Returns >0 if ready, 0 on timeout, <0 on error (WSAPoll).
fn poll_readable(s: Hsocket, timeout_ms: i32 = 0) -> i32 {
    return shim_socket_poll(s.raw, false, timeout_ms);
}

fn poll_writable(s: Hsocket, timeout_ms: i32 = 0) -> i32 {
    return shim_socket_poll(s.raw, true, timeout_ms);
}

fn send_to(s: Hsocket, host: const u8[:], port: u16, data: const u8[:]) -> i64 {
    return shim_udp_send_to(s.raw, host, port, data);
}

fn recv_from(s: Hsocket, buf: u8[:], out_host: u8[>..]&, out_port: u16&) -> i64 {
    return shim_udp_recv_from(s.raw, buf, out_host, out_port);
}

// --- WinHTTP convenience wrappers -------------------------------------------

fn http_get(url: const u8[:], out_body: u8[>..]&, out_status: u32&, timeout_ms: u32 = 10000) -> bool {
    return http_request("GET", url, "", "", out_body, out_status, timeout_ms);
}

fn http_post(
    url: const u8[:],
    body: const u8[:],
    content_type: const u8[:],
    out_body: u8[>..]&,
    out_status: u32&,
    timeout_ms: u32 = 10000
) -> bool {
    return http_request("POST", url, body, content_type, out_body, out_status, timeout_ms);
}
