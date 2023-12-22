# UCT AM Hello World

This example specifically includes the use of Active Message API and CUDA memory.

## Compile this example
```bash
# First, edit the makefile to set the correct path of `UCX_PATH` and `CUDA_PATH`
make
```

## Run the example

### UCP Server Sample Output

The server (.163) exchange the address and receive the message (store to the host memory) from client.

```bash
$ ./uct_hello_world -p 17788 -z -t rc_mlx5 -d mlx5_0:1 -m cuda

INFO: UCT_HELLO_WORLD AM function = uct_ep_am_zcopy server = (null) port = 17788
Using rc_mlx5/mlx5_0:1
Waiting for connection...


----- UCT TEST SUCCESS ----

[callback] uct_ep_am_zcopy sent ABCDEFGHIJKLMNO (16 bytes)

```

### UCP Client Sample Output

The client (.164) send the active message (from CUDA Memory or host memory) to the server by short/bcopy/zcopy. 

```bash
$ ./uct_hello_world -p 17788 -n 192.168.0.208 -z -t rc_mlx5 -d mlx5_3:1 -m cuda

INFO: UCT_HELLO_WORLD AM function = uct_ep_am_zcopy server = 192.168.0.208 port = 17788
Using rc_mlx5/mlx5_3:1
```

## Reference

- [ucx / example / uct_hello_world](https://github.com/openucx/ucx/blob/master/examples/uct_hello_world.c)
