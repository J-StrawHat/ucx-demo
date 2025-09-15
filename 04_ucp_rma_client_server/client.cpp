#include "ucx_util.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <stdexcept>

static inline void die(const char* msg) {
    std::fprintf(stderr, "%s\n", msg);
    std::exit(1);
}


int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "Usage: %s <server_ip> <port> <put|get>\n", argv[0]);
        return 1;
    }
    const char* ip = argv[1];
    uint16_t port = static_cast<uint16_t>(std::strtoul(argv[2], nullptr, 10));
    std::string mode = argv[3];
    bool do_put = (mode == "put");
    bool do_get = (mode == "get");
    if (!do_put && !do_get) die("mode must be put or get");

    // Fetch handshake
    int fd = tcp::connect(ip, port);
    Handshake hs = Handshake::recv_fd(fd);
    ::close(fd);
    size_t size = static_cast<size_t>(hs.size);

    // UCX init
    UcxEnv env;

    // Endpoint to server
    UcxEndpoint ep(env.worker(), hs.worker_addr);

    // Unpack rkey
    ucp_rkey_h rkey = ep.import_rkey(hs.rkey);

    // Local buffer and optional registration
    std::vector<char> lbuf(size);
    if (do_put) {
        for (size_t i = 0; i < size; ++i) lbuf[i] = static_cast<char>((i * 3) & 0xFF);
    }

    UcxMem lmem(env.ctx(), lbuf.data(), lbuf.size());

    // RMA op
    ucp_request_param_t param{};
    param.op_attr_mask = UCP_OP_ATTR_FIELD_MEMH; // pass local memh
    param.memh = lmem.memh();

    void* req = nullptr;
    ucs_status_t st = UCS_OK;
    if (do_put) {
        req = ep.put_nbx(lbuf.data(), lbuf.size(), hs.remote_addr, rkey, &param);
        if (UCS_PTR_IS_ERR(req)) { UcxEndpoint::destroy_rkey(rkey); throw std::runtime_error("ucp_put_nbx failed"); }
        st = env.wait(req);
        if (st != UCS_OK) die("put completion error");
        // Ensure remote visibility
        req = ep.flush_nbx(&param);
        if (UCS_PTR_IS_ERR(req)) { UcxEndpoint::destroy_rkey(rkey); throw std::runtime_error("flush failed"); }
        st = env.wait(req);
        if (st != UCS_OK) die("flush completion error");
    } else {
        req = ep.get_nbx(lbuf.data(), lbuf.size(), hs.remote_addr, rkey, &param);
        if (UCS_PTR_IS_ERR(req)) { UcxEndpoint::destroy_rkey(rkey); throw std::runtime_error("ucp_get_nbx failed"); }
        st = env.wait(req);
        if (st != UCS_OK) die("get completion error");
    }

    // Print first 16 bytes for verification
    std::printf("[client] %s done. First 16 bytes: ", do_put ? "PUT" : "GET");
    for (size_t i = 0; i < std::min<size_t>(16, lbuf.size()); ++i)
        std::printf("%02x ", (unsigned char)lbuf[i]);
    std::printf("\n");

    // Cleanup
    UcxEndpoint::destroy_rkey(rkey);
    return 0;
}
