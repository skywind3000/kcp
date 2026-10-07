//=====================================================================
//
// test.cpp - kcp 测试用例
//
// 说明：
// gcc test.cpp -o test -lstdc++
//
//=====================================================================

#include <stdio.h>
#include <stdlib.h>

#include "test.h"
#include "ikcp.c"


// Capture complete UDP datagrams; each data message fills one MSS.
typedef std::vector<std::vector<char> > TestPackets;

static int capture_packet(const char *buf, int len, ikcpcb *, void *user)
{
	TestPackets *packets = (TestPackets *)user;
	packets->push_back(std::vector<char>(buf, buf + len));
	return 0;
}

static void deliver_packets(TestPackets &packets, ikcpcb *peer,
	int drop, bool reverse)
{
	for (int i = 0; i < (int)packets.size(); i++) {
		int index = reverse ? (int)packets.size() - i - 1 : i;
		if (index == drop) continue;
		const std::vector<char> &packet = packets[index];
		int result = ikcp_input(peer, &packet[0], (long)packet.size());
		assert(result == 0);
	}
	packets.clear();
}

// Equal-timestamp ACKs must advance maxack to the highest sequence number.
// Two receiver-generated ACK batches should recover a single lost datagram
// with resend=2, before the original 200 ms retransmission timeout.
static bool test_ack_timestamps(bool reverse, IUINT32 start)
{
	TestPackets data, acks;
	ikcpcb *sender = ikcp_create(1, &data);
	ikcpcb *receiver = ikcp_create(1, &acks);
	assert(sender != NULL && receiver != NULL);
	ikcp_setoutput(sender, capture_packet);
	ikcp_setoutput(receiver, capture_packet);
	ikcp_nodelay(sender, 1, 10, 2, 1);
	ikcp_nodelay(receiver, 1, 10, 2, 1);
	std::vector<char> message(sender->mss);

	for (int batch = 0; batch < 2; batch++) {
		int first = batch == 0 ? 0 : 4;
		int last = batch == 0 ? 4 : 7;
		for (int i = first; i < last; i++) {
			memset(&message[0], i, message.size());
			int result = ikcp_send(sender, &message[0], (int)message.size());
			assert(result == (int)message.size());
		}
		ikcp_update(sender, start + batch * 20);
		assert(data.size() == (size_t)(last - first));
		for (size_t i = 0; i < data.size(); i++)
			assert(data[i].size() == sender->mtu);
		// Drop the entire datagram containing sequence number 1 only once.
		deliver_packets(data, receiver, batch == 0 ? 1 : -1, reverse);
		ikcp_update(receiver, start + batch * 20);
		assert(acks.size() == 1 && acks[0].size() == 3 * IKCP_OVERHEAD);
		deliver_packets(acks, sender, -1, false);
	}

	ikcp_update(sender, start + 30);
	deliver_packets(data, receiver, -1, false);
	int received = 0;
	bool contents_ok = true;
	for (;;) {
		int result = ikcp_recv(receiver, &message[0], (int)message.size());
		if (result < 0) break;
		if (result != (int)message.size()) contents_ok = false;
		for (int i = 0; i < result; i++)
			if (message[i] != (char)received) contents_ok = false;
		received++;
	}
	printf("ACK timestamps: reverse=%d start=%u delivered=%d/7 at 30ms\n",
		(int)reverse, (unsigned)start, received);
	bool passed = received == 7 && contents_ok && sender->xmit == 0;
	ikcp_release(sender);
	ikcp_release(receiver);
	return passed;
}

// 模拟网络
LatencySimulator *vnet;

// 模拟网络：模拟发送一个 udp包
int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	union { int id; void *ptr; } parameter;
	parameter.ptr = user;
	vnet->send(parameter.id, buf, len);
	return 0;
}

// 测试用例
void test(int mode)
{
	// 创建模拟网络：丢包率10%，Rtt 60ms~125ms
	vnet = new LatencySimulator(10, 60, 125);

	// 创建两个端点的 kcp对象，第一个参数 conv是会话编号，同一个会话需要相同
	// 最后一个是 user参数，用来传递标识
	ikcpcb *kcp1 = ikcp_create(0x11223344, (void*)0);
	ikcpcb *kcp2 = ikcp_create(0x11223344, (void*)1);

	// 设置kcp的下层输出，这里为 udp_output，模拟udp网络输出函数
	kcp1->output = udp_output;
	kcp2->output = udp_output;

	IUINT32 current = iclock();
	IUINT32 slap = current + 20;
	IUINT32 index = 0;
	IUINT32 next = 0;
	IINT64 sumrtt = 0;
	int count = 0;
	int maxrtt = 0;

	// 配置窗口大小：平均延迟200ms，每20ms发送一个包，
	// 而考虑到丢包重发，设置最大收发窗口为128
	ikcp_wndsize(kcp1, 128, 128);
	ikcp_wndsize(kcp2, 128, 128);

	// 判断测试用例的模式
	if (mode == 0) {
		// 默认模式
		ikcp_nodelay(kcp1, 0, 10, 0, 0);
		ikcp_nodelay(kcp2, 0, 10, 0, 0);
	}
	else if (mode == 1) {
		// 普通模式，关闭流控等
		ikcp_nodelay(kcp1, 0, 10, 0, 1);
		ikcp_nodelay(kcp2, 0, 10, 0, 1);
	}	else {
		// 启动快速模式
		// 第二个参数 nodelay-启用以后若干常规加速将启动
		// 第三个参数 interval为内部处理时钟，默认设置为 10ms
		// 第四个参数 resend为快速重传指标，设置为2
		// 第五个参数 为是否禁用常规流控，这里禁止
		ikcp_nodelay(kcp1, 2, 10, 2, 1);
		ikcp_nodelay(kcp2, 2, 10, 2, 1);
		kcp1->rx_minrto = 10;
		kcp1->fastresend = 1;
	}


	char buffer[2000];
	int hr;

	IUINT32 ts1 = iclock();

	while (1) {
		isleep(1);
		current = iclock();
		ikcp_update(kcp1, iclock());
		ikcp_update(kcp2, iclock());

		// 每隔 20ms，kcp1发送数据
		for (; current >= slap; slap += 20) {
			((IUINT32*)buffer)[0] = index++;
			((IUINT32*)buffer)[1] = current;

			// 发送上层协议包
			ikcp_send(kcp1, buffer, 8);
		}

		// 处理虚拟网络：检测是否有udp包从p1->p2
		while (1) {
			hr = vnet->recv(1, buffer, 2000);
			if (hr < 0) break;
			// 如果 p2收到udp，则作为下层协议输入到kcp2
			ikcp_input(kcp2, buffer, hr);
		}

		// 处理虚拟网络：检测是否有udp包从p2->p1
		while (1) {
			hr = vnet->recv(0, buffer, 2000);
			if (hr < 0) break;
			// 如果 p1收到udp，则作为下层协议输入到kcp1
			ikcp_input(kcp1, buffer, hr);
		}

		// kcp2接收到任何包都返回回去
		while (1) {
			hr = ikcp_recv(kcp2, buffer, 10);
			// 没有收到包就退出
			if (hr < 0) break;
			// 如果收到包就回射
			ikcp_send(kcp2, buffer, hr);
		}

		// kcp1收到kcp2的回射数据
		while (1) {
			hr = ikcp_recv(kcp1, buffer, 10);
			// 没有收到包就退出
			if (hr < 0) break;
			IUINT32 sn = *(IUINT32*)(buffer + 0);
			IUINT32 ts = *(IUINT32*)(buffer + 4);
			IUINT32 rtt = current - ts;
			
			if (sn != next) {
				// 如果收到的包不连续
				printf("ERROR sn %d<->%d\n", (int)count, (int)next);
				return;
			}

			next++;
			sumrtt += rtt;
			count++;
			if (rtt > (IUINT32)maxrtt) maxrtt = rtt;

			printf("[RECV] mode=%d sn=%d rtt=%d\n", mode, (int)sn, (int)rtt);
		}
		if (next > 1000) break;
	}

	ts1 = iclock() - ts1;

	ikcp_release(kcp1);
	ikcp_release(kcp2);

	const char *names[3] = { "default", "normal", "fast" };
	printf("%s mode result (%dms):\n", names[mode], (int)ts1);
	printf("avgrtt=%d maxrtt=%d tx=%d\n", (int)(sumrtt / count), (int)maxrtt, (int)vnet->tx1);
	printf("press enter to next ...\n");
	char ch; scanf("%c", &ch);
}

int main(int argc, char **argv)
{
	bool passed = true;
	passed &= test_ack_timestamps(false, 1000);
	passed &= test_ack_timestamps(true, 1000);
	passed &= test_ack_timestamps(false, 0xfffffff0u);
	passed &= test_ack_timestamps(true, 0xfffffff0u);
	if (!passed) return 1;
	if (argc == 2 && strcmp(argv[1], "--ack-timestamp-test") == 0) return 0;
	test(0);	// 默认模式，类似 TCP：正常模式，无快速重传，常规流控
	test(1);	// 普通模式，关闭流控等
	test(2);	// 快速模式，所有开关都打开，且关闭流控
	return 0;
}

/*
default mode result (20917ms):
avgrtt=740 maxrtt=1507

normal mode result (20131ms):
avgrtt=156 maxrtt=571

fast mode result (20207ms):
avgrtt=138 maxrtt=392
*/


