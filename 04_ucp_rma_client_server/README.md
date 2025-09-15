# UCX RMA Client/Server Demo

This is a minimal UCX-only RMA client/server demo that uses UCP `ucp_put_nbx` and `ucp_get_nbx` for remote write/read between processes. A small TCP handshake is used to exchange the UCX worker address, remote virtual address, and rkey.

## Features
- Remote write (PUT): client writes its local buffer into the server’s registered memory, followed by a flush for remote visibility.
- Remote read (GET): client reads from the server’s registered memory into a local buffer.
- Multi‑client handshake: the server accepts multiple TCP connections and returns the same worker address and rkey to each client.
- Visibility: the server prints the first 16 bytes of its buffer every second so you can see PUT effects live.

## Layout
- `server.cpp`: the server main.
- `client.cpp`: the client main (`put`/`get`).
- `ucx_util.h/.cpp`: shared utilities (UCX RAII, TCP helpers, handshake packing).
- `CMakeLists.txt`: build configuration (UCX path via `INSTALL_UCX_PATH`).

## Prerequisites
- UCX installed (from source or packages).
- POSIX sockets (Linux/macOS).
- CMake 3.14+, C11/C++14 compiler.

Environment setup example:
```bash
export INSTALL_UCX_PATH=/path/to/your/ucx/install
# Make sure UCX shared libs are discoverable at runtime
export LD_LIBRARY_PATH=$INSTALL_UCX_PATH/lib:$LD_LIBRARY_PATH
```

## Build
```bash
mkdir build && cd build
cmake .. && make -j
```
Binaries: `build/ucx_rma_server`, `build/ucx_rma_client`.

## Run
1) Start the server on the target host:
```bash
./ucx_rma_server 12345 4096
```
- `12345`: TCP port used for the handshake.
- `4096`: size of the registered RMA buffer in bytes.
- The server prints the head of the buffer every second.

2) Run the client (same or different host):
```bash
./ucx_rma_client <server_ip> 12345 get
```
- Remote write (PUT):
```bash
./ucx_rma_client <server_ip> 12345 put
```

Typical verification: run GET first (you should see 00 01 02 …), then PUT (pattern 00 03 06 …), then GET again (should reflect the PUT pattern).

## Protocol and Key Details
- Handshake payload (host endianness for homogeneous setups):
  - `[u32 waddr_len][waddr_bytes][u32 rkey_len][rkey_bytes][u64 remote_addr][u64 size]`
- Memory registration:
  - Server maps its buffer with `ucp_mem_map` and sends the packed rkey.
  - Client maps its local buffer and provides `UCP_OP_ATTR_FIELD_MEMH` in NBX params for efficient paths.
- RMA operations:
  - PUT: `ucp_put_nbx` + `ucp_ep_flush_nbx` to ensure remote visibility.
  - GET: `ucp_get_nbx` then immediate use.
- Progress/completion:
  - Both sides use `ucp_worker_progress` and `ucp_request_check_status`; see `UcxEnv::wait`.
