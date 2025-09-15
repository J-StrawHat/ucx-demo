#include "ucx_util.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>

namespace tcp {

static void die(const char* msg) {
    throw std::runtime_error(msg);
}

int listen(uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) die("socket()");
    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (::bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) die("bind()");
    if (::listen(fd, 16) < 0) die("listen()");
    return fd;
}

int accept_nonblock(int lfd) {
    struct pollfd pfd{};
    pfd.fd = lfd;
    pfd.events = POLLIN;
    int pret = ::poll(&pfd, 1, 0);
    if (pret <= 0 || !(pfd.revents & POLLIN)) return -1;
    sockaddr_in cli{}; socklen_t clilen = sizeof(cli);
    return ::accept(lfd, (sockaddr*)&cli, &clilen);
}

int connect(const char* ip, uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) die("socket()");
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, ip, &addr.sin_addr) != 1) die("inet_pton()");
    if (::connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) die("connect()");
    return fd;
}

void send_all(int fd, const void* buf, size_t len) {
    const char* p = static_cast<const char*>(buf);
    size_t off = 0;
    while (off < len) {
        ssize_t n = ::send(fd, p + off, len - off, 0);
        if (n <= 0) die("send()");
        off += static_cast<size_t>(n);
    }
}

void recv_all(int fd, void* buf, size_t len) {
    char* p = static_cast<char*>(buf);
    size_t off = 0;
    while (off < len) {
        ssize_t n = ::recv(fd, p + off, len - off, 0);
        if (n <= 0) die("recv()");
        off += static_cast<size_t>(n);
    }
}

} // namespace tcp

void Handshake::send_fd(int fd) const {
    uint32_t wlen32 = static_cast<uint32_t>(worker_addr.size());
    uint32_t rlen32 = static_cast<uint32_t>(rkey.size());
    tcp::send_all(fd, &wlen32, sizeof(wlen32));
    if (!worker_addr.empty()) tcp::send_all(fd, worker_addr.data(), worker_addr.size());
    tcp::send_all(fd, &rlen32, sizeof(rlen32));
    if (!rkey.empty()) tcp::send_all(fd, rkey.data(), rkey.size());
    tcp::send_all(fd, &remote_addr, sizeof(remote_addr));
    tcp::send_all(fd, &size, sizeof(size));
}

Handshake Handshake::recv_fd(int fd) {
    Handshake hs;
    uint32_t wlen32 = 0, rlen32 = 0;
    tcp::recv_all(fd, &wlen32, sizeof(wlen32));
    hs.worker_addr.resize(wlen32);
    if (wlen32) tcp::recv_all(fd, hs.worker_addr.data(), hs.worker_addr.size());
    tcp::recv_all(fd, &rlen32, sizeof(rlen32));
    hs.rkey.resize(rlen32);
    if (rlen32) tcp::recv_all(fd, hs.rkey.data(), hs.rkey.size());
    tcp::recv_all(fd, &hs.remote_addr, sizeof(hs.remote_addr));
    tcp::recv_all(fd, &hs.size, sizeof(hs.size));
    return hs;
}

UcxEnv::UcxEnv() {
    ucp_params_t ucp_params{};
    ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
    ucp_params.features = UCP_FEATURE_RMA | UCP_FEATURE_AM;
    ucp_config_t* config = nullptr;
    if (ucp_config_read(nullptr, nullptr, &config) != UCS_OK) throw std::runtime_error("ucp_config_read failed");
    if (ucp_init(&ucp_params, config, &ctx_) != UCS_OK) {
        ucp_config_release(config);
        throw std::runtime_error("ucp_init failed");
    }
    ucp_config_release(config);

    ucp_worker_params_t worker_params{};
    worker_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
    worker_params.thread_mode = UCS_THREAD_MODE_MULTI;
    if (ucp_worker_create(ctx_, &worker_params, &worker_) != UCS_OK) {
        ucp_cleanup(ctx_);
        ctx_ = nullptr;
        throw std::runtime_error("ucp_worker_create failed");
    }
}

UcxEnv::~UcxEnv() {
    if (worker_) ucp_worker_destroy(worker_);
    if (ctx_) ucp_cleanup(ctx_);
}

std::vector<char> UcxEnv::worker_address_bytes() const {
    ucp_worker_attr_t wattr{};
    wattr.field_mask = UCP_WORKER_ATTR_FIELD_ADDRESS;
    if (ucp_worker_query(worker_, &wattr) != UCS_OK) throw std::runtime_error("ucp_worker_query failed");
    auto* addr = static_cast<ucp_address_t*>(wattr.address);
    size_t len = wattr.address_length;
    std::vector<char> out(len);
    if (len) std::memcpy(out.data(), addr, len);
    ucp_worker_release_address(worker_, addr);
    return out;
}

void UcxEnv::progress() const {
    ucp_worker_progress(worker_);
}

ucs_status_t UcxEnv::wait(void* req) const {
    if (req == nullptr) return UCS_OK;
    ucs_status_t st;
    do {
        ucp_worker_progress(worker_);
        st = ucp_request_check_status(req);
    } while (st == UCS_INPROGRESS);
    ucp_request_free(req);
    return st;
}

UcxMem::UcxMem(ucp_context_h ctx, void* base, size_t len)
    : base_(base), len_(len), ctx_(ctx) {
    ucp_mem_map_params_t mpar{};
    mpar.field_mask = UCP_MEM_MAP_PARAM_FIELD_ADDRESS | UCP_MEM_MAP_PARAM_FIELD_LENGTH;
    mpar.address = base_;
    mpar.length = len_;
    if (ucp_mem_map(ctx_, &mpar, &memh_) != UCS_OK) throw std::runtime_error("ucp_mem_map failed");
}

UcxMem::~UcxMem() {
    if (memh_) ucp_mem_unmap(ctx_, memh_);
}

UcxMem::UcxMem(UcxMem&& other) noexcept {
    *this = std::move(other);
}

UcxMem& UcxMem::operator=(UcxMem&& other) noexcept {
    if (this != &other) {
        base_ = other.base_;
        len_ = other.len_;
        memh_ = other.memh_;
        ctx_ = other.ctx_;
        other.base_ = nullptr;
        other.len_ = 0;
        other.memh_ = nullptr;
        other.ctx_ = nullptr;
    }
    return *this;
}

std::vector<char> UcxMem::pack_rkey(ucp_context_h ctx) const {
    void* rkey_buf = nullptr;
    size_t rkey_len = 0;
    if (ucp_rkey_pack(ctx, memh_, &rkey_buf, &rkey_len) != UCS_OK) throw std::runtime_error("ucp_rkey_pack failed");
    std::vector<char> out(rkey_len);
    if (rkey_len) std::memcpy(out.data(), rkey_buf, rkey_len);
    ucp_rkey_buffer_release(rkey_buf);
    return out;
}

UcxEndpoint::UcxEndpoint(ucp_worker_h worker, const std::vector<char>& remote_addr_bytes) {
    ucp_ep_params_t ep_params{};
    ep_params.field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS;
    ep_params.address = (ucp_address_t*)remote_addr_bytes.data();
    if (ucp_ep_create(worker, &ep_params, &ep_) != UCS_OK) throw std::runtime_error("ucp_ep_create failed");
}

UcxEndpoint::~UcxEndpoint() {
    if (ep_) ucp_ep_destroy(ep_);
}

UcxEndpoint::UcxEndpoint(UcxEndpoint&& other) noexcept { *this = std::move(other); }
UcxEndpoint& UcxEndpoint::operator=(UcxEndpoint&& other) noexcept {
    if (this != &other) {
        ep_ = other.ep_;
        other.ep_ = nullptr;
    }
    return *this;
}

ucp_rkey_h UcxEndpoint::import_rkey(const std::vector<char>& rkey_bytes) const {
    ucp_rkey_h rkey = nullptr;
    if (ucp_ep_rkey_unpack(ep_, (void*)rkey_bytes.data(), &rkey) != UCS_OK) throw std::runtime_error("ucp_ep_rkey_unpack failed");
    return rkey;
}

void UcxEndpoint::destroy_rkey(ucp_rkey_h rkey) {
    if (rkey) ucp_rkey_destroy(rkey);
}

void* UcxEndpoint::put_nbx(const void* laddr, size_t len, uint64_t raddr, ucp_rkey_h rkey, const ucp_request_param_t* param) const {
    return ucp_put_nbx(ep_, laddr, len, raddr, rkey, param);
}

void* UcxEndpoint::get_nbx(void* laddr, size_t len, uint64_t raddr, ucp_rkey_h rkey, const ucp_request_param_t* param) const {
    return ucp_get_nbx(ep_, laddr, len, raddr, rkey, param);
}

void* UcxEndpoint::flush_nbx(const ucp_request_param_t* param) const {
    return ucp_ep_flush_nbx(ep_, param);
}
