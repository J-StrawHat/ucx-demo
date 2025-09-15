// Standalone UCX RMA demo server (no nixl dependency)
// - Creates UCP context/worker
// - Registers a buffer, packs rkey
// - Sends handshake (worker address, rkey, remote addr, size) over TCP
// - Accepts multiple client handshake connections
// - Progresses worker and periodically prints first bytes for verification

#include "ucx_util.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "Usage: %s <port> <size_bytes>\n", argv[0]);
        return 1;
    }
    uint16_t port = static_cast<uint16_t>(std::strtoul(argv[1], nullptr, 10));
    size_t size = static_cast<size_t>(std::strtoull(argv[2], nullptr, 10));

    // Allocate data buffer
    std::vector<char> buf(size);
    for (size_t i = 0; i < size; ++i) buf[i] = static_cast<char>(i & 0xFF);

    // Init UCX env
    UcxEnv env;

    // Worker address
    std::vector<char> waddr_copy = env.worker_address_bytes();

    // Register memory and pack rkey
    UcxMem mem(env.ctx(), buf.data(), buf.size());

    std::vector<char> rkey_copy = mem.pack_rkey(env.ctx());

    uint64_t raddr = reinterpret_cast<uint64_t>(buf.data());

    // Handshake over TCP
    int lfd = tcp::listen(port);
    std::printf("[server] Listening on port %u\n", (unsigned)port);

    // Event loop: accept with poll, progress UCX, and periodically print buffer head
    auto last_print = std::chrono::steady_clock::now();
    while (true) {
        // Progress UCX
        env.progress();

        // Poll for incoming connection with short timeout
        int cfd = tcp::accept_nonblock(lfd);
        if (cfd >= 0) {
            Handshake hs;
            hs.worker_addr = waddr_copy;
            hs.rkey = rkey_copy;
            hs.remote_addr = reinterpret_cast<uint64_t>(buf.data());
            hs.size = static_cast<uint64_t>(size);
            hs.send_fd(cfd);
            ::close(cfd);
            std::printf("[server] Handshake sent. RMA buffer at %p size=%zu\n", buf.data(), size);
        }

        // Periodic print of first 16 bytes for visibility
        auto now = std::chrono::steady_clock::now();
        if (now - last_print > std::chrono::seconds(1)) {
            last_print = now;
            std::printf("[server] head: ");
            size_t n = std::min<size_t>(16, buf.size());
            for (size_t i = 0; i < n; ++i) std::printf("%02x ", (unsigned char)buf[i]);
            std::printf("\n");
        }
    }

    // Cleanup (unreachable in this loop)
    return 0;
}
