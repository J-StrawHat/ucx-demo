
# UCX Perf Test

## UCT

### AM API

尚无法准确测试 RDMA (put/get)、Active Messages，因为两侧需要使用[相同的设备名](https://github.com/openucx/ucx/issues/4966#issuecomment-607375392) （目前已解决，报错如下所示），同时消息大小限制在 2048 

- For server (.163)
```bash
$ ucx_perftest -p 17788
[1702987735.055130] [oem-PowerEdge-R750xa:2595163:0]         libperf.c:1580 UCX  ERROR Cannot use rc_mlx5/mlx5_3:1
[1702987735.055184] [oem-PowerEdge-R750xa:2595163:0]    perftest_run.c:342  UCX  ERROR Failed to run test: No such device
```
- For client (.164)
```bash
$ ucx_perftest 192.168.0.208 -t am_bw -n 1000 -w 10 -x rc_mlx5 -d mlx5_3:1
[1702988061.912566] [oem-PowerEdge-R750xa:406578:0]        perftest.c:131  UCX  ERROR recv() remote closed connection
[1702988061.912596] [oem-PowerEdge-R750xa:406578:0]        perftest.c:291  UCX  ERROR sock: rte recv: remote peer failure

$ ucx_perftest 192.168.0.208 -t am_bw -n 10000 -s 2046 -x rc_mlx5 -d mlx5_0:1 -p 17788
[1702990873.587506] [oem-PowerEdge-R750xa:481524:0]        perftest.c:808  UCX  WARN  CPU affinity is not set (bound to 80 cpus). Performance may be impacted.
+--------------+--------------+------------------------------+---------------------+-----------------------+
|              |              |       overhead (usec)        |   bandwidth (MB/s)  |  message rate (msg/s) |
+--------------+--------------+----------+---------+---------+----------+----------+-----------+-----------+
|    Stage     | # iterations | 50.0%ile | average | overall |  average |  overall |  average  |  overall  |
+--------------+--------------+----------+---------+---------+----------+----------+-----------+-----------+
Final:                 10000      0.716     0.741     0.741     2633.63    2633.63     1349871     1349871
```

### Tag-Matching API

- For client (.164)
```bash
ucx_perftest 192.168.0.208 -t tag_bw -n 10000 -s 10000000 -p 12345
[1702988299.763181] [oem-PowerEdge-R750xa:412883:0]        perftest.c:808  UCX  WARN  CPU affinity is not set (bound to 80 cpus). Performance may be impacted.
+--------------+--------------+------------------------------+---------------------+-----------------------+
|              |              |       overhead (usec)        |   bandwidth (MB/s)  |  message rate (msg/s) |
+--------------+--------------+----------+---------+---------+----------+----------+-----------+-----------+
|    Stage     | # iterations | 50.0%ile | average | overall |  average |  overall |  average  |  overall  |
+--------------+--------------+----------+---------+---------+----------+----------+-----------+-----------+
[thread 0]               881      0.249  1150.947  1150.947     8286.00    8286.00         869         869
[thread 0]              1745      0.232  1171.168  1160.959     8142.93    8214.54         854         861
[thread 0]              2625      0.243  1148.732  1156.860     8301.97    8243.65         871         864
[thread 0]              3505      0.203  1146.197  1154.183     8320.34    8262.77         872         866
[thread 0]              4385      0.190  1142.121  1151.762     8350.03    8280.13         876         868
[thread 0]              5265      0.192  1140.399  1149.863     8362.64    8293.81         877         870
[thread 0]              6145      0.175  1140.831  1148.569     8359.47    8303.15         877         871
[thread 0]              7025      0.178  1141.628  1147.700     8353.63    8309.44         876         871
[thread 0]              7895      0.175  1163.262  1149.415     8198.28    8297.04         860         870
[thread 0]              8791      0.176  1137.905  1148.242     8380.96    8305.52         879         871
[thread 0]              9687      0.176  1134.054  1146.929     8409.42    8315.02         882         872
Final:                 10000      0.175  1167.236  1147.565     8170.36    8310.42         857         871
```

## UCP

### ucx_perftest

- For server (.163):
```bash
$ ucx_perftest -p 12345
```
- For client (.164):
```bash
$ ucx_perftest 192.168.0.208 -t ucp_am_bw -n 10000 -s 10000000 -p 12345
+--------------+--------------+------------------------------+---------------------+-----------------------+
|              |              |       overhead (usec)        |   bandwidth (MB/s)  |  message rate (msg/s) |
+--------------+--------------+----------+---------+---------+----------+----------+-----------+-----------+
|    Stage     | # iterations | 50.0%ile | average | overall |  average |  overall |  average  |  overall  |
+--------------+--------------+----------+---------+---------+----------+----------+-----------+-----------+
[thread 0]               879      0.357  1155.184  1155.184     8255.60    8255.60         866         866
[thread 0]              1759      0.324  1143.577  1149.377     8339.40    8297.31         874         870
[thread 0]              2639      0.336  1140.741  1146.498     8360.13    8318.15         877         872
[thread 0]              3535      0.318  1132.804  1143.027     8418.71    8343.41         883         875
[thread 0]              4431      0.314  1135.052  1141.414     8402.03    8355.20         881         876
[thread 0]              5327      0.321  1146.336  1142.242     8319.33    8349.14         872         875
[thread 0]              6207      0.311  1145.575  1142.715     8324.85    8345.69         873         875
[thread 0]              7103      0.317  1143.815  1142.853     8337.66    8344.68         874         875
[thread 0]              7983      0.312  1144.925  1143.082     8329.58    8343.01         873         875
[thread 0]              8879      0.314  1133.149  1142.079     8416.14    8350.33         882         876
[thread 0]              9759      0.312  1145.803  1142.415     8323.19    8347.88         873         875
Final:                 10000      0.311  1219.411  1144.271     7820.78    8334.34         820         874

$ ucx_perftest 192.168.0.208 -t ucp_am_bw -n 10000 -s 10000000 -p 12345 -m cuda
# -m 选项在 1.15.0 会出现报错，在 1.17.0 可以正常使用
+--------------+--------------+------------------------------+---------------------+-----------------------+
|              |              |       overhead (usec)        |   bandwidth (MB/s)  |  message rate (msg/s) |
+--------------+--------------+----------+---------+---------+----------+----------+-----------+-----------+
|    Stage     | # iterations | 50.0%ile | average | overall |  average |  overall |  average  |  overall  |
+--------------+--------------+----------+---------+---------+----------+----------+-----------+-----------+
[thread 0]               925    874.796  1083.686  1083.686     8800.28    8800.28         923         923
[thread 0]              2068    875.080   877.540   969.748    10867.59    9834.25        1140        1031
[thread 0]              3212    876.039   876.781   936.636    10877.00   10181.91        1141        1068
[thread 0]              4355    875.300   877.198   921.036    10871.83   10354.36        1140        1086
[thread 0]              5498    875.711   877.034   911.888    10873.86   10458.24        1140        1097
[thread 0]              6642    875.216   876.642   905.818    10878.73   10528.33        1141        1104
[thread 0]              7785    874.799   877.155   901.609    10872.36   10577.47        1140        1109
[thread 0]              8929    875.163   876.748   898.424    10877.40   10614.97        1141        1113
Final:                 10000    874.368   902.843   898.897    10563.01   10609.38        1108        1112

$ ucx_perftest 192.168.0.208 -t ucp_put_bw -n 10000 -s 10000000 -p 12345
+--------------+--------------+------------------------------+---------------------+-----------------------+
|              |              |       overhead (usec)        |   bandwidth (MB/s)  |  message rate (msg/s) |
+--------------+--------------+----------+---------+---------+----------+----------+-----------+-----------+
|    Stage     | # iterations | 50.0%ile | average | overall |  average |  overall |  average  |  overall  |
+--------------+--------------+----------+---------+---------+----------+----------+-----------+-----------+
[thread 0]              1185    867.781   846.492   846.492    11266.19   11266.19        1181        1181
[thread 0]              2338    867.987   869.939   858.055    10962.54   11114.37        1150        1165
[thread 0]              3491    868.212   869.988   861.996    10961.93   11063.56        1149        1160
[thread 0]              4644    867.805   869.477   863.854    10968.37   11039.77        1150        1158
[thread 0]              5797    867.114   869.446   864.966    10968.76   11025.57        1150        1156
[thread 0]              6950    867.535   869.556   865.727    10967.37   11015.87        1150        1155
[thread 0]              8103    867.024   869.446   866.256    10968.76   11009.15        1150        1154
[thread 0]              9256    867.509   869.420   866.651    10969.09   11004.14        1150        1154
Final:                 10000    867.145   908.122   869.736    10501.61   10965.10        1101        1150

```

## Appendix


### uct_hello_world

Build:
```bash
$ gcc examples/uct_hello_world.c -DHAVE_CUDA -luct -lucs -lcudart -o uct_hello_world \
      -Iinstall/include -I/usr/local/cuda-11.4/include  -Linstall/lib -L/usr/local/cuda-11.4/lib64
```

- For server (.163):
```bash
$ ./uct_hello_world -p 17788 -z -t rc_mlx5 -d mlx5_0:1 -m cuda
```

- For client (.164):
```bash
$ ./uct_hello_world -p 17788 -n 192.168.0.208 -z -t rc_mlx5 -d mlx5_3:1 -m cuda
```

### ucp_hello_world

Build:
```bash
$ gcc examples/ucp_hello_world.c -DHAVE_CUDA -lucp -lucs -lcudart -o ucp_hello_world \
      -Iinstall/include -I/usr/local/cuda-11.4/include  -Linstall/lib -L/usr/local/cuda-11.4/lib64
```

- For server (.163):
```bash
$ ./ucp_hello_world -p 17788 -m cuda
INFO: UCP_HELLO_WORLD mode = 0 server = (null) port = 17788, pid = 2488978
[0xc6f53000] local address length: 496
Waiting for connection...
[0xc6f53000] receive handler called with status 0 (Success), length 819
finish to receive UCX address message
[0xc6f53000] send handler called for "UCX data message" with status 0 (Success)
finish to send UCX data message
flush_ep completed with status 0 (Success)
```

- For client (.164):
```bash
$ ./ucp_hello_world -n 192.168.0.208 -p 17788 -m cuda
INFO: UCP_HELLO_WORLD mode = 0 server = 192.168.0.208 port = 17788, pid = 4168226
[0x5303000] local address length: 811
[0x5303000] send handler called for "UCX address message" with status 0 (Success)
finish to send UCX address message
[0x5303000] receive handler called with status 0 (Success), length 24
finish to receive UCX data message


----- UCP TEST SUCCESS ----

ABCDEFGHIJKLMNO

---------------------------
```

### ucp_client_server

虽然数据收发成功，但是报了

- For server (.163):
```bash
$ ./ucp_client_server -p 17788
server is listening on IP 0.0.0.0 port 17788
Waiting for connection...
Server received a connection request from client at address 192.168.0.210:52828
Server: iteration #1
UCX data message was received


----- UCP TEST SUCCESS -------

ABCDEFGHIJKLMNO.


------------------------------

sent FIN message
error handling callback was invoked with status -25 (Connection reset by remote peer)
Waiting for connection...
```

- For client (.164):
```bash
$ ./ucp_client_server -a 192.168.0.208 -p 17788
Client: iteration #1


------------------------------

ABCDEFGHIJKLMNO.


------------------------------

received FIN message
```
