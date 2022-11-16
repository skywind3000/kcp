#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#include "test.h"
#include "ikcp.c"

typedef struct
{
    int                socket;
    struct sockaddr_in server_sa;
    int                channel;
} test_ctx_t;


int
kcp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    test_ctx_t *ctx = (test_ctx_t *)user;

    sendto(ctx->socket, buf, len, 0, (struct sockaddr *)&ctx->server_sa,
           sizeof(ctx->server_sa));

    return 0;
}

int
main(int argc, char **argv)
{
    test_ctx_t ctx;
    char      *server_addr = "127.0.0.1";
    int        server_port = 50000;
    int        channel;

    switch (argc)
    {
    case 4:
        server_port = atoi(argv[3]);
        if (server_port <= 0 || 65535 < server_port)
        {
            printf("invalid port: %s\n", argv[3]);
            return 1;
        }
    case 3: server_addr = argv[2];
    case 2:
        channel = atoi(argv[1]);
        if (channel <= 0)
        {
            printf("invalid channel number: %s\n", argv[1]);
            return 1;
        }
        break;
    default:
        printf("Usage: %s <channel> [server_ip] [server_port]\n", argv[0]);
        return 1;
    }

    // init ctx
    bzero(&ctx, sizeof(ctx));

    ctx.channel = channel;

    ctx.server_sa.sin_family = AF_INET;
    ctx.server_sa.sin_port   = htons(server_port);
    if (inet_pton(AF_INET, server_addr, &ctx.server_sa.sin_addr) < 0)
    {
        printf("invalid server address: %s\n", server_addr);
        return 1;
    }

    // create udp
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == -1) return 1;

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
    IINT64  sumrtt = 0;
    int     count  = 0;
    int     maxrtt = 0;
    IUINT32 ts1    = iclock();

    printf("inited\n");

    while (1)
    {
        isleep(1);
        current = iclock();
        ikcp_update(kcp, iclock());

        // 每隔 20ms，kcp1发送数据
        for (; current >= slap; slap += 20)
        {
            ((IUINT32 *)buffer)[0] = index++;
            ((IUINT32 *)buffer)[1] = current;

            // 发送上层协议包
            ikcp_send(kcp, buffer, 8);
        }

        // 处理底层数据
        while (1)
        {
            n = recvfrom(s, buffer, sizeof(buffer), 0, NULL, NULL);
            if (n < 0) break;

            ikcp_input(kcp, buffer, n);
        }

        // kcp收到应用据
        while (1)
        {
            n = ikcp_recv(kcp, buffer, sizeof(buffer));

            // 没有收到包就退出
            if (n < 0) break;

            char *p    = buffer;
            char *last = p + n;
            while (p + 8 <= last)
            {
                IUINT32 sn  = *(IUINT32 *)(p + 0);
                IUINT32 ts  = *(IUINT32 *)(p + 4);
                IUINT32 rtt = current - ts;

                if (sn != next)
                {
                    // 如果收到的包不连续
                    printf("ERROR sn %d<->%d\n", (int)count, (int)next);
                    return 1;
                }

                p += 8;
                next++;
                sumrtt += rtt;
                count++;
                if (rtt > (IUINT32)maxrtt) maxrtt = rtt;

                printf("[RECV] sn=%d rtt=%d\n", (int)sn, (int)rtt);

                if (next > 1000) goto done;
            }

            if (p != last)
            {
                // 收到不完整的包
                printf("receiving a incomplete package\n");
                return 1;
            }
        }
    }

done:

    ts1 = iclock() - ts1;

    ikcp_release(kcp);

    printf("result (%dms):\n", (int)ts1);
    printf("avgrtt=%d maxrtt=%d\n", (int)(sumrtt / count), (int)maxrtt);

    return 0;
}