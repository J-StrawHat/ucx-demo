/**
* Copyright (C) Mellanox Technologies Ltd. 2001-2016.  ALL RIGHTS RESERVED.
* Copyright (C) Advanced Micro Devices, Inc. 2018. ALL RIGHTS RESERVED.
*
* See file LICENSE for terms.
*/

/*
 * UCP hello world client / server example utility
 * -----------------------------------------------
 *
 * Server side:
 *
 *    ./ucp_hello_world
 *
 * Client side:
 *
 *    ./ucp_hello_world <server host name>
 *
 * Notes:
 *
 *    - Client acquires Server UCX address via TCP socket
 *
 *
 * Author:
 *
 *    Ilya Nelkenbaum <ilya@nelkenbaum.com>
 *    Sergey Shalnov <sergeysh@mellanox.com> 7-June-2016
 *
 * 2021: Modified for use in UCX HOTI tutorial
 *    Matthew Baker <bakermb@ornl.gov>
 */

#include "hello_world_util.h"

#include <ucp/api/ucp.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>   /* isprint */
#include <pthread.h> /* pthread_self */
#include <errno.h>   /* errno */
#include <time.h>
#include <cuda.h>
#include <cuda_runtime.h>

struct msg {
    uint64_t        data_len;
};

struct ucx_context {
    int             completed;
};

static ucs_status_t client_status = UCS_OK;
static uint16_t server_port = 13337;
static long test_string_length = 16;
static const ucp_tag_t tag  = 0x1337a880u;
static const ucp_tag_t tag_mask = UINT64_MAX;
static ucp_address_t *local_addr;
static ucp_address_t *peer_addr;

static size_t local_addr_len;
static size_t peer_addr_len;

#define CUDA_FUNC(_func)                                   \
    do {                                                   \
        cudaError_t _result = (_func);                     \
        if (cudaSuccess != _result) {                      \
            fprintf(stderr, "%s failed: %s\n",             \
                    #_func, cudaGetErrorString(_result));  \
        }                                                  \
    } while(0)

static void set_msg_data_len(struct msg *msg, uint64_t data_len)
{
    CUDA_FUNC(cudaMemcpy(&msg->data_len, &data_len, sizeof(data_len), cudaMemcpyDefault));
}

static void request_init(void *request)
{
    struct ucx_context *ctx = (struct ucx_context *) request; // 因为参数大小已指定为 sizeof(struct ucx_context)，所以可以直接强制转换
    ctx->completed = 0; // 初始化为未完成
}

static void send_handler(void *request, ucs_status_t status, void *ctx)
{
    struct ucx_context *context = (struct ucx_context *) ctx;

    context->completed = 1;

    printf("[0x%x] send handler called with status %d (%s)\n",
           (unsigned int)pthread_self(), status, ucs_status_string(status));
}

static void recv_handler(void *request, ucs_status_t status,
                        ucp_tag_recv_info_t *info)
{
    struct ucx_context *context = (struct ucx_context *) request; // 因为参数大小已指定为 sizeof(struct ucx_context)，所以可以直接强制转换

    context->completed = 1; // 标记为已完成

    printf("[0x%x] receive handler called with status %d (%s), length %lu\n",
           (unsigned int)pthread_self(), status, ucs_status_string(status),
           info->length);
}

static void ucx_wait(ucp_worker_h ucp_worker, struct ucx_context *context)
{
    while (context->completed == 0) {
        ucp_worker_progress(ucp_worker);
    }
}


static int run_ucx_client(ucp_worker_h ucp_worker)
{
    ucp_request_param_t send_param;
    ucp_tag_recv_info_t info_tag;
    ucp_tag_message_h msg_tag;
    ucs_status_t status;
    ucp_ep_h server_ep;
    ucp_ep_params_t ep_params;
    struct msg *msg = 0;
    struct ucx_context *request;
    struct ucx_context ctx;
    size_t msg_len = 0;
    int ret = -1;
    char *str;

    /* Send client UCX address to server */
    ep_params.field_mask      = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS;
    ep_params.address         = peer_addr; // 服务端发来的local addr，即 remote address

    status = ucp_ep_create(ucp_worker, &ep_params, &server_ep);  // 创建并连接端点
    CHKERR_JUMP(status != UCS_OK, "ucp_ep_create\n", err);

    msg_len = sizeof(*msg) + local_addr_len; 
    msg     = malloc(msg_len); // msg = len + local_addr
    CHKERR_JUMP(msg == NULL, "allocate memory\n", err_ep);
    memset(msg, 0, msg_len);

    msg->data_len = local_addr_len;
    memcpy(msg + 1, local_addr, local_addr_len);

    send_param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                              UCP_OP_ATTR_FIELD_USER_DATA;
    send_param.cb.send      = send_handler; // 指定回调函数
    send_param.user_data    = &ctx; // 传递给回调函数的用户数据指针
    ctx.completed           = 0;
    request                 = ucp_tag_send_nbx(server_ep, msg, msg_len, tag,
                                               &send_param);
    if (UCS_PTR_IS_ERR(request)) {
        fprintf(stderr, "unable to send UCX address message\n");
        free(msg);
        goto err_ep;
    } else if (UCS_PTR_IS_PTR(request)) {
        ucx_wait(ucp_worker, &ctx); // 自定义的wait函数，等待ctx->completed变为1
        ucp_request_release(request);
    }

    free(msg);

    /* Receive test string from server */
    for (;;) {

        /* Probing incoming events in non-block mode */
         // 非阻塞的意义：等待某个操作完成的同时继续执行其他任务，并且允许程序在等待特定事件时才保持活跃
         // 非阻塞探查worker中是否有符合条件（由标记和标记掩码描述）的消息，tag_mask表示必须匹配标记的哪些bit
         // 传入 true(1)，表示返回的消息句柄将从 UCP 库中删除，用于调用ucp_tag_msg_recv_nb()，以接收数据并释放与报文句柄相关的资源。
        msg_tag = ucp_tag_probe_nb(ucp_worker, tag, tag_mask, 1, &info_tag);
        if (msg_tag != NULL) {
            /* Message arrived */
            break;
        } else if (ucp_worker_progress(ucp_worker)) {
            /* Some events were polled; try again without going to sleep */
            continue;
        }

    }

    //msg = malloc(info_tag.length);
    CUDA_FUNC(cudaMallocManaged((void **)&msg, info_tag.length, cudaMemAttachGlobal));
    CHKERR_JUMP(msg == NULL, "allocate memory\n", err_ep);

    // 开始接收数据，但不等待其完成。
    request = ucp_tag_msg_recv_nb(ucp_worker, msg, info_tag.length, // 服务端发来的消息直接存到驻存在CUDA Memory的msg
                                  ucp_dt_make_contig(1), msg_tag,
                                  recv_handler); // 接收数据完毕后，调用recv_handler()函数，设置request->completed为1

    if (UCS_PTR_IS_ERR(request)) {
        fprintf(stderr, "unable to receive UCX data message (%u)\n",
                UCS_PTR_STATUS(request));
        free(msg);
        goto err_ep;
    } else {
        /* ucp_tag_msg_recv_nb() cannot return NULL */
        assert(UCS_PTR_IS_PTR(request));
        ucx_wait(ucp_worker, request); // 自定义的wait函数，等待request->completed变为1
        request->completed = 0; // 重置为未完成
        ucp_request_release(request);
        printf("UCX data message was received\n");
    }

    str = calloc(1, test_string_length); 
    if (str != NULL) {
      //memcpy(str, msg + 1, test_string_length);
        CUDA_FUNC(cudaMemcpy(str, msg + 1, test_string_length, cudaMemcpyDefault)); // 将驻存在CUDA Memory的msg搬到主存当中
        printf("\n\n----- UCP TEST SUCCESS ----\n\n");
        printf("%s", str);
        printf("\n\n---------------------------\n\n");
        free(str);
    } else {
        fprintf(stderr, "Memory allocation failed\n");
        goto err_ep;
    }

    CUDA_FUNC(cudaFree(msg));

    ret = 0;

err_ep:
    ucp_ep_destroy(server_ep);

err:
    return ret;
}

static void flush_callback(void *request, ucs_status_t status, void *user_data)
{
}

static ucs_status_t flush_ep(ucp_worker_h worker, ucp_ep_h ep)
{
    ucp_request_param_t param;
    void *request;

    param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK;
    param.cb.send      = flush_callback;
    request            = ucp_ep_flush_nbx(ep, &param);
    // 非阻塞地刷新（flush）指定端点（endpoint）上的所有未完成的原子内存操作（AMO，Atomic Memory Operations）和远程内存访问（RMA）操作
    // 通过刷新端点，可以确保之前所有的远程写入和更新都已经在远程节点上可见，从而确保数据一致性
    if (request == NULL) {
        return UCS_OK;
    } else if (UCS_PTR_IS_ERR(request)) {
        return UCS_PTR_STATUS(request);
    } else {
        ucs_status_t status;
        do {
            ucp_worker_progress(worker);
            status = ucp_request_check_status(request);
        } while (status == UCS_INPROGRESS);
        ucp_request_release(request);
        return status;
    }
}

static int generate_test_string(char *str, int size)
{
    char *tmp_str;
    int i;

    tmp_str = calloc(1, size);
    CHKERR_ACTION(tmp_str == NULL, "allocate memory\n", return -1);

    for (i = 0; i < (size - 1); ++i) {
        tmp_str[i] = 'A' + (i % 26);
    }

    CUDA_FUNC(cudaMemcpy(str, tmp_str, size, cudaMemcpyDefault));

    free(tmp_str);
    return 0;
}

static int run_ucx_server(ucp_worker_h ucp_worker)
{
    ucp_request_param_t send_param;
    ucp_tag_recv_info_t info_tag;
    ucp_tag_message_h msg_tag;
    ucs_status_t status;
    ucp_ep_h client_ep;
    ucp_ep_params_t ep_params;
    struct msg *msg = 0;
    struct ucx_context *request = 0;
    struct ucx_context ctx;
    size_t msg_len = 0;
    int ret;

    /* Receive client UCX address */
    do {
        /* Progressing before probe to update the state */
        ucp_worker_progress(ucp_worker);

        /* Probing incoming events in non-block mode */
        msg_tag = ucp_tag_probe_nb(ucp_worker, tag, tag_mask, 1, &info_tag); // 探查worker中是否有符合条件（由标记和标记掩码描述）的消息
    } while (msg_tag == NULL);

    msg = malloc(info_tag.length);
    CHKERR_ACTION(msg == NULL, "allocate memory\n", ret = -1; goto err);
    request = ucp_tag_msg_recv_nb(ucp_worker, msg, info_tag.length,
                                  ucp_dt_make_contig(1), msg_tag, recv_handler);

    if (UCS_PTR_IS_ERR(request)) {
        fprintf(stderr, "unable to receive UCX address message (%s)\n",
                ucs_status_string(UCS_PTR_STATUS(request)));
        free(msg);
        ret = -1;
        goto err;
    } else {
        /* ucp_tag_msg_recv_nb() cannot return NULL */
        assert(UCS_PTR_IS_PTR(request));
        ucx_wait(ucp_worker, request); // 等待request->completed变为1，即客户端发来的地址信息已接收完毕
        request->completed = 0;
        ucp_request_release(request);
        printf("UCX address message was received\n");
    }

    peer_addr_len = msg->data_len;
    peer_addr     = malloc(peer_addr_len); 
    if (peer_addr == NULL) {
        fprintf(stderr, "unable to allocate memory for peer address\n");
        free(msg);
        ret = -1;
        goto err;
    }

    memcpy(peer_addr, msg + 1, peer_addr_len); // 得到客户端的地址信息

    free(msg);

    /* Send test string to client */
    ep_params.field_mask      = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS |
                                UCP_EP_PARAM_FIELD_ERR_HANDLER |
                                UCP_EP_PARAM_FIELD_USER_DATA;
    ep_params.address         = peer_addr;
    ep_params.user_data       = &client_status;

    status = ucp_ep_create(ucp_worker, &ep_params, &client_ep);
    CHKERR_ACTION(status != UCS_OK, "ucp_ep_create\n", ret = -1; goto err);

    msg_len = sizeof(*msg) + test_string_length;
    //msg = malloc(msg_len);
    CUDA_FUNC(cudaMallocManaged((void **)&msg, msg_len, cudaMemAttachGlobal));
    CHKERR_ACTION(msg == NULL, "allocate memory\n", ret = -1; goto err_ep);
    //memset(msg, 0, msg_len);
    CUDA_FUNC(cudaMemset(msg, 0, msg_len));

    set_msg_data_len(msg, msg_len - sizeof(*msg));
    ret = generate_test_string((char *)(msg + 1), test_string_length); // 生成发送的字符串
    CHKERR_JUMP(ret < 0, "generate test string", err_free_mem_type_msg);

    send_param.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK |
                              UCP_OP_ATTR_FIELD_USER_DATA |
                              UCP_OP_ATTR_FIELD_MEMORY_TYPE;
    send_param.cb.send      = send_handler; // 指定发送完毕后的回调函数
    send_param.user_data    = &ctx;
    send_param.memory_type  = UCS_MEMORY_TYPE_CUDA; // !: 指定了发送的数据类型
    ctx.completed           = 0;
    request                 = ucp_tag_send_nbx(client_ep, msg, msg_len, tag, // 传入对应的数据
                                               &send_param);
    if (UCS_PTR_IS_ERR(request)) {
        fprintf(stderr, "unable to send UCX data message\n");
        ret = -1;
        goto err_free_mem_type_msg;
    } else if (UCS_PTR_IS_PTR(request)) {
        printf("UCX data message was scheduled for send\n");
        ucx_wait(ucp_worker, &ctx);
        ucp_request_release(request);
    }

    status = flush_ep(ucp_worker, client_ep);
    printf("flush_ep completed with status %d (%s)\n",
            status, ucs_status_string(status));

    ret = 0;

err_free_mem_type_msg:
    cudaFree(msg);
err_ep:
    ucp_ep_destroy(client_ep);
err:
    return ret;
}

static int run_test(const char *client_target_name, ucp_worker_h ucp_worker)
{
    if (client_target_name != NULL) {
        return run_ucx_client(ucp_worker);
    } else {
        return run_ucx_server(ucp_worker);
    }
}

int main(int argc, char **argv)
{
    /* UCP temporary vars */
    ucp_params_t ucp_params;
    ucp_worker_params_t worker_params;
    ucp_config_t *config;
    ucs_status_t status;

    /* UCP handler objects */
    ucp_context_h ucp_context;
    ucp_worker_h ucp_worker;

    /* OOB(Out-of-Band) connection vars */
    uint64_t addr_len = 0;
    char *client_target_name = NULL;
    int oob_sock = -1;
    int ret = -1;

    memset(&ucp_params, 0, sizeof(ucp_params));
    memset(&worker_params, 0, sizeof(worker_params));

    if(argc == 2) {
        client_target_name = argv[1];
    }

    if(argc > 2) {
        printf("Usage: ucp_hello_world [server_address]\n"
               "\tUCX hello world example using ucp. Start server by providing no args, or connect to a running server at server_address\n");
	return 0;
    }

    /* UCP initialization */
    status = ucp_config_read(NULL, NULL, &config); // 从运行时环境中获取有关UCP库配置的信息，获取到的描述符可用于ucp_init函数
    CHKERR_JUMP(status != UCS_OK, "ucp_config_read\n", err);

    // 指定有效的参数字段
    ucp_params.field_mask   = UCP_PARAM_FIELD_FEATURES |
                              UCP_PARAM_FIELD_REQUEST_SIZE |
                              UCP_PARAM_FIELD_REQUEST_INIT;
    ucp_params.features     = UCP_FEATURE_TAG; // 设置 UCP 特性（传输模式）
    ucp_params.request_size    = sizeof(struct ucx_context); // 保留的空间大小，应用程序通常使用这个空间来缓存自己的结构。
    ucp_params.request_init    = request_init; // 只在请求内存首次初始化时调用，request_init 为自定义的函数指针

    status = ucp_init(&ucp_params, config, &ucp_context);

    ucp_config_print(config, stdout, NULL, UCS_CONFIG_PRINT_CONFIG);

    ucp_config_release(config);
    CHKERR_JUMP(status != UCS_OK, "ucp_init\n", err);

    worker_params.field_mask  = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
    worker_params.thread_mode = UCS_THREAD_MODE_SINGLE; // 只有主线程可以访问（即初始化上下文的线程）

    status = ucp_worker_create(ucp_context, &worker_params, &ucp_worker);
    CHKERR_JUMP(status != UCS_OK, "ucp_worker_create\n", err_cleanup);

    status = ucp_worker_get_address(ucp_worker, &local_addr, &local_addr_len);
    CHKERR_JUMP(status != UCS_OK, "ucp_worker_get_address\n", err_worker);

    printf("[0x%x] local address length: %lu\n",
           (unsigned int)pthread_self(), local_addr_len);

    /* OOB connection establishment */
    if (client_target_name) {
        peer_addr_len = local_addr_len;

        oob_sock = client_connect(client_target_name, server_port);
        CHKERR_JUMP(oob_sock < 0, "client_connect\n", err_addr);

        ret = recv(oob_sock, &addr_len, sizeof(addr_len), MSG_WAITALL); // 获取地址长度
        CHKERR_JUMP_RETVAL(ret != (int)sizeof(addr_len),
                           "receive address length\n", err_addr, ret);

        peer_addr_len = addr_len;
        peer_addr = malloc(peer_addr_len);
        CHKERR_JUMP(!peer_addr, "allocate memory\n", err_addr);

        ret = recv(oob_sock, peer_addr, peer_addr_len, MSG_WAITALL); // 获取地址并存到 peer_addr
        CHKERR_JUMP_RETVAL(ret != (int)peer_addr_len,
                           "receive address\n", err_peer_addr, ret);
    } else {
        oob_sock = server_connect(server_port);
        CHKERR_JUMP(oob_sock < 0, "server_connect\n", err_peer_addr);

        addr_len = local_addr_len;
        ret = send(oob_sock, &addr_len, sizeof(addr_len), 0); // 发送地址长度
        CHKERR_JUMP_RETVAL(ret != (int)sizeof(addr_len),
                           "send address length\n", err_peer_addr, ret);

        ret = send(oob_sock, local_addr, local_addr_len, 0); // 发送本地地址
        CHKERR_JUMP_RETVAL(ret != (int)local_addr_len, "send address\n",
                           err_peer_addr, ret);
    }

    ret = run_test(client_target_name, ucp_worker);

    if (!ret) {
        /* Make sure remote is disconnected before destroying local worker */
        ret = barrier(oob_sock);
    }
    close(oob_sock);

err_peer_addr:
    free(peer_addr);

err_addr:
    ucp_worker_release_address(ucp_worker, local_addr);

err_worker:
    ucp_worker_destroy(ucp_worker);

err_cleanup:
    ucp_cleanup(ucp_context);

err:
    return ret;
}
