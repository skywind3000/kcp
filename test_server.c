#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "test.h"
#include "ikcp.c"

typedef struct
{
    int                socket;
    struct sockaddr_in sa;
    struct sockaddr_in peer_sa;
    socklen_t          peer_len;
    int                channel;
} test_ctx_t;


int
kcp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    test_ctx_t *ctx = (test_ctx_t *)user;


    ssize_t n = sendto(ctx->socket, buf, len, 0,
                       (struct sockaddr *)&ctx->peer_sa, sizeof(ctx->peer_sa));

    printf("output: %d/%d bytes\n", n, len);

    return 0;
}

int
main(int argc, char **argv)
{
    test_ctx_t ctx;
    char      *listen_addr = "127.0.0.1";
    int        listen_port = 50001;
    int        channel;

    switch (argc)
    {
    case 4:
        listen_port = atoi(argv[3]);
        if (listen_port <= 0 || 65535 < listen_port)
        {
            printf("invalid port: %s\n", argv[3]);
            return 1;
        }
    case 3: listen_addr = argv[2];
    case 2:
        channel = atoi(argv[1]);
        if (channel <= 0)
        {
            printf("invalid channel number: %s\n", argv[1]);
            return 1;
        }
        break;
    default:
        printf("Usage: %s <channel> [listen_ip] [listen_port]\n", argv[0]);
        return 1;
    }

    // init ctx
    bzero(&ctx, sizeof(ctx));

    ctx.channel = channel;

    ctx.sa.sin_family = AF_INET;
    ctx.sa.sin_port   = htons(listen_port);
    if (inet_pton(AF_INET, listen_addr, &ctx.sa.sin_addr) < 0)
    {
        printf("invalid server address: %s\n", listen_addr);
        return 1;
    }

    // create udp
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == -1) return 1;

    if (bind(s, (struct sockaddr *)&ctx.sa, sizeof(ctx.sa)) == -1)
    {
        printf("bind failed\n");
        return 1;
    }

    int nb;
    nb = 1;
    ioctl(s, FIONBIO, &nb);

    ctx.socket = s;

    ikcpcb *kcp;

    kcp = ikcp_create(ctx.channel, &ctx);
    if (kcp == NULL) return 1;

    ikcp_setoutput(kcp, kcp_output);
    ikcp_nodelay(kcp, 1, 1, 2, 1);
    ikcp_wndsize(kcp, 1024, 1024);

    IUINT32 current = iclock();
    IUINT32 slap    = current + 20;
    IUINT32 index   = 0;
    IUINT32 next    = 0;
    ssize_t n;
    char    buffer[65536];

    printf("inited\n");

    while (1)
    {
        isleep(1);
        current = iclock();
        ikcp_update(kcp, iclock());

        // 处理底层数据
        while (1)
        {
            bzero(&ctx.peer_sa.sin_addr, sizeof(ctx.peer_sa.sin_addr));
            ctx.peer_len = sizeof(ctx.peer_sa);

            n = recvfrom(s, buffer, sizeof(buffer), 0,
                         (struct sockaddr *)&ctx.peer_sa, &ctx.peer_len);
            if (n < 0)
            {
                if (errno != EAGAIN)
                {
                    printf("recvfrom error: %d\n", n);
                    return 1;
                }

                break;
            }

            char ip[INET_ADDRSTRLEN] = {0};
            inet_ntop(ctx.peer_sa.sin_family, &ctx.peer_sa.sin_addr, ip,
                      INET_ADDRSTRLEN);

            printf("recv: %d bytes from %s:%d\n", n, ip,
                   ntohs(ctx.peer_sa.sin_port));

            ikcp_input(kcp, buffer, n);
        }

        // kcp回显数据
        while (1)
        {
            n = ikcp_recv(kcp, buffer, 10);

            // 没有收到包就退出
            if (n < 0)
            {
                break;
            }

            ikcp_send(kcp, buffer, n);
        }
    }

    ikcp_release(kcp);

    return 0;
}