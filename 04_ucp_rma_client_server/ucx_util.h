// Minimal C++ utilities to share UCX and TCP handshake logic between client and server.
// Standalone: depends only on UCX and POSIX sockets.

#pragma once

#include <ucp/api/ucp.h>

#include <cstdint>
#include <vector>
#include <string>

// TCP helpers
namespace tcp {
int listen(uint16_t port);
int accept_nonblock(int lfd); // returns -1 if no pending
int connect(const char* ip, uint16_t port);
void send_all(int fd, const void* buf, size_t len);
void recv_all(int fd, void* buf, size_t len);
}

struct Handshake {
    std::vector<char> worker_addr; // bytes of ucp_address_t
    std::vector<char> rkey;        // bytes from ucp_rkey_pack
    uint64_t remote_addr{0};       // server buffer virtual address
    uint64_t size{0};              // bytes in buffer

    // Wire format (native endianness for simplicity on homogeneous envs):
    // [u32 wlen][waddr][u32 rlen][rkey][u64 raddr][u64 size]
    void send_fd(int fd) const;
    static Handshake recv_fd(int fd);
};

// UcxEnv:
// - RAII wrapper that initializes and owns a UCP context and worker.
// - Provides helper utilities for obtaining worker address bytes, progressing
//   the worker, and waiting for NBX requests to complete.
class UcxEnv {
public:
    UcxEnv();
    ~UcxEnv();
    UcxEnv(const UcxEnv&) = delete;
    UcxEnv& operator=(const UcxEnv&) = delete;

    ucp_context_h ctx() const { return ctx_; }
    ucp_worker_h worker() const { return worker_; }

    // Returns a copy of worker address bytes and releases UCX's internal buffer.
    std::vector<char> worker_address_bytes() const;

    // Progress and wait helpers
    void progress() const;
    ucs_status_t wait(void* req) const; // polls until complete and frees

private:
    ucp_context_h ctx_{nullptr};
    ucp_worker_h worker_{nullptr};
};

// UcxMem:
// - RAII wrapper for ucp_mem_map/unmap on a user-provided host buffer.
// - Exposes packed rkey bytes via pack_rkey for sending to a remote peer.
class UcxMem {
public:
    UcxMem() = default;
    UcxMem(ucp_context_h ctx, void* base, size_t len);
    ~UcxMem();
    UcxMem(const UcxMem&) = delete;
    UcxMem& operator=(const UcxMem&) = delete;
    UcxMem(UcxMem&& other) noexcept;
    UcxMem& operator=(UcxMem&& other) noexcept;

    bool valid() const { return memh_ != nullptr; }
    ucp_mem_h memh() const { return memh_; }

    // Pack rkey into bytes (caller own the returned vector)
    std::vector<char> pack_rkey(ucp_context_h ctx) const;

private:
    void* base_{nullptr};
    size_t len_{0};
    ucp_mem_h memh_{nullptr};
    ucp_context_h ctx_{nullptr};
};

// UcxEndpoint:
// - RAII wrapper for a UCP endpoint to a remote worker address.
// - Provides rkey import/destroy helpers and thin wrappers around NBX
//   RMA operations (put/get) and endpoint flush.
class UcxEndpoint {
public:
    UcxEndpoint() = default;
    UcxEndpoint(ucp_worker_h worker, const std::vector<char>& remote_addr_bytes);
    ~UcxEndpoint();
    UcxEndpoint(const UcxEndpoint&) = delete;
    UcxEndpoint& operator=(const UcxEndpoint&) = delete;
    UcxEndpoint(UcxEndpoint&& other) noexcept;
    UcxEndpoint& operator=(UcxEndpoint&& other) noexcept;

    bool valid() const { return ep_ != nullptr; }
    ucp_ep_h ep() const { return ep_; }

    // Rkey import/destroy
    ucp_rkey_h import_rkey(const std::vector<char>& rkey_bytes) const;
    static void destroy_rkey(ucp_rkey_h rkey);

    // RMA ops using NBX; param.memh must be set by caller if needed
    void* put_nbx(const void* laddr, size_t len, uint64_t raddr, ucp_rkey_h rkey, const ucp_request_param_t* param) const;
    void* get_nbx(void* laddr, size_t len, uint64_t raddr, ucp_rkey_h rkey, const ucp_request_param_t* param) const;
    void* flush_nbx(const ucp_request_param_t* param) const;

private:
    ucp_ep_h ep_{nullptr};
};
