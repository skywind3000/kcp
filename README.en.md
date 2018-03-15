KCP - A Fast and Reliable ARQ Protocol
======================================

[![Powered][2]][1] [![Build Status][4]][5] 

[1]: https://github.com/skywind3000/kcp
[2]: http://skywind3000.github.io/word/images/kcp.svg
[3]: https://raw.githubusercontent.com/skywind3000/kcp/master/kcp.svg
[4]: https://api.travis-ci.org/skywind3000/kcp.svg?branch=master
[5]: https://travis-ci.org/skywind3000/kcp


# Introduction

KCP is a fast and reliable protocol that can achieve the transmission effect of a reduction of the average latency by 30% to 40% and reduction of the maximum delay by a factor of three, at the cost of 10% to 20% more bandwidth wasted than TCP. It is implemented by using the pure algorithm, and is not responsible for the sending and receiving of the underlying protocol (such as UDP), requiring the users to define their own transmission mode for the underlying data packet, and provide it to KCP in the way of callback. Even the clock needs to be passed in from the outside, without any internal system calls.

The entire protocol has only two source files of ikcp.h, ikcp.c, which can be easily integrated into the user's own protocol stack. You may have implement a P2P, or a UDP-based protocol, but are lack of a set of perfect ARQ reliable protocol implementation, then by simply copying the two files to the existing project, and writing a couple of lines of code, you can use it.



# Technical Specifications

TCP is designed for traffic (the amount of kilobits per second of data that can be transmitted), which focuses on the full use of bandwidth. While KCP is designed for the flow rate (the amount of time it takes to send a single packet from one end to the other), with 10% -20% bandwidth waste in exchange for transmission speed 30%-40% faster than TCP. TCP channel is a grand canal with very slow flow rate, but very large flow per second, while KCP is a small torrent with the rapid flow. KCP has both normal and fast modes, achieving the result of flow rate increase by the following strategies:

#### RTO Doubled vs Not Doubled:

TCP timeout calculation is RTOx2, so three consecutive packet losses will make it RTOx8, which is very terrible, while after KCP fast mode is enabled, it is not x2, but x1.5 (Experimental results show that the value of 1.5 is relatively good), which has improved the transmission speed.

#### Selective Retransmission vs Full Retransmission:

When packet loss occurs in TCP, all the data after the lost packet will be retransmitted, while KCP is selective retransmission, and only re-transmits the data packets that are really lost.

#### Fast Retransmission:

The transmitting terminal sends 1, 2, 3, 4 and 5 packets, and then receives the remote ACK: 1, 3, 4 and 5, when receiving ACK3, KCP knows that 2 is skipped 1 time, and when receiving ACK4, it knows that 2 is skipped 2 times, at this point, it can consider that 2 is lost, without waiting until timeout, it will directly retransmit packet 2, which can greatly improve the transmission speed when packet loss occurs.

#### Delayed ACK vs Non-delayed ACK:

In order to make full use of the bandwidth, TCP delays sending an ACK (Even NODELAY does not work), so that the timeout calculation will come out with a relatively high RTT, which has extended the judgment process when packet loss occurs. While for KCP, it is adjustable whether to delay sending an ACK.

#### UNA vs ACK+UNA：

There are two kinds of ARQ model responses: UNA (All packets before this number received, such as TCP) and ACK (The packet with this number received). Using UNA alone will result in full retransmissions, and using ACK alone has too much cost for packet loss, hence in the previous protocols, one of the two has been selected; while in KCP protocol, all packets have UNA information except for a single ACK packet.

#### Non-concessional Flow Control:

KCP normal mode uses the same fair concession rules as TCP, i.e., the send window size is determined by: four factors including the size of the send cache, the size of the receive buffer at the receiving end, packet loss concession and slow start. However, when sending small data with high timeliness requirement, it is allowed to select skipping the latter two steps through configuration, and use only the first two items to control the transmission frequency, sacrificing some of the fairness and bandwidth utilization, in exchange for the effect of smooth transmission even when BT is opened.


# Basic Usage

1. Create KCP object:

   ```cpp
   // Initialize the kcp object, conv is an integer that represents the session number, 
   // same as the conv of tcp, both communication sides shall ensure the same conv, 
   // so that mutual data packets can be recognized, user is a pointer which will be 
   // passed to the callback function.
   ikcpcb *kcp = ikcp_create(conv, user);
   ```

2. Set the callback function:

   ```cpp
   // KCP lower layer protocol output function, which will be called by KCP when it 
   // needs to send data, buf/len represents the buffer and data length. 
   // user refers to the incoming value at the time the kcp object is created to 
   // distinguish between multiple KCP objects
   int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
   {
     ....
   }
   // Set the callback function
   kcp->output = udp_output;
   ```

3. Call update in an interval:

   ```cpp
   // Call ikcp_update at a certain frequency to update the kcp state, and pass in 
   // the current clock (in milliseconds). If the call is executed every 10ms, or 
   // ikcp_check is used to determine time of the next call for update, no need to 
   // call every time;
   ikcp_update(kcp, millisec);
   ```

4. Input a lower layer data packet:

   ```cpp
   // Need to call when a lower layer data packet (such as UDP packet)is received:
   ikcp_input(kcp, received_udp_packet, received_udp_size);
   ```

   After processing the output / input of the lower layer protocols, KCP protocol can work normally, and ikcp_send is used to send data to the remote end. While the other end uses ikcp_recv (kcp, ptr, size) to receive the data.


# Protocol Configuration

The protocol default mode is a standard ARQ, and various acceleration switches can be enabled by configuration:

1. Working Mode:
   ```cpp
   int ikcp_nodelay(ikcpcb *kcp, int nodelay, int interval, int resend, int nc)
   ```

   - `nodelay` : Whether nodelay mode is enabled, 0 is not enabled; 1 enabled.
   - `interval` ：Protocol internal work interval, in milliseconds, such as 10 ms or 20 ms.
   - `resend` ：Fast retransmission mode, 0 represents off by default, 2 can be set (2 ACK spans will result in direct retransmission)
   - `nc` ：Whether to turn off flow control, 0 represents “Do not turn off” by default, 1 represents “Turn off”.
   - Normal Mode: ikcp_nodelay(kcp, 0, 40, 0, 0);
   - Turbo Mode： ikcp_nodelay(kcp, 1, 10, 2, 1);

2. Window Size:
   ```cpp
   int ikcp_wndsize(ikcpcb *kcp, int sndwnd, int rcvwnd);
   ```
   The call will set the maximum send window and maximum receive window size of the procotol, which is 32 by default. This can be understood as SND_BUF and RCV_BUF of TCP, but the unit is not the same, SND / RCV_BUF unit is byte, while this unit is the packet.

3. Maximum Transmission Unit:

   Pure algorithm protocol is not responsible for MTU detection, the default mtu is 1400 bytes, which can be set using ikcp_setmtu. The value will affect the maximum transmission unit upon data packet merging and fragmentation.

4. Minimum RTO:

   No matter TCP or KCP, they have the limitation for the minimum RTO when calculating the RTO, even if the calculated RTO is 40ms, as the default RTO is 100ms, the protocol can only detect packet loss after 100ms, which is 30ms in the fast mode, and the value can be manually changed: 
   ```cpp
   kcp->rx_minrto = 10;
   ```



# Document Indexing

Both the use and configuration of the protocol is very simple, in most cases, after you read the above contents, basically you will be able to use it. If you need further fine control, such as changing the KCP memory allocator, or if you need more efficient large-scale scheduling of KCP links (such as more than 3,500 links), or to better combine with TCP, you can continue the extensive reading:

- [KCP Best Practice](https://github.com/skywind3000/kcp/wiki/KCP-Best-Practice-EN)
- [Integration with the Existing TCP Server](https://github.com/skywind3000/kcp/wiki/KCP-Best-Practice-EN)
- [Benchmarks](https://github.com/skywind3000/kcp/wiki/KCP-Benchmark)


# Related Applications

- [kcptun](https://github.com/xtaci/kcptun): High-speed remote port forwarding based (tunnel) on kcp-go, with ssh-D, it allows smoother online video viewing than finalspeed.
- [dog-tunnel](https://github.com/vzex/dog-tunnel): Network tunnel developed by GO, using KCP to greatly improve the transmission speed, and migrated a GO version of the KCP.
- [v2ray](https://www.v2ray.com)：Well-known proxy software, Shadowsocks replacement, integrated with kcp protocol after 1.17, using UDP transmission, no data packet features.
- [asio-kcp](https://github.com/libinzhangyuan/asio_kcp): Use the complete UDP network library of KCP, complete implementation of UDP-based link state management, session control and KCP protocol scheduling, etc.
- [kcp-java](https://github.com/hkspirt/kcp-java)：Implementation of Java version of KCP protocol.
- [kcp-netty](https://github.com/szhnet/kcp-netty)：Java implementation of KCP based on Netty.
- [kcp-go](https://github.com/xtaci/kcp-go): High-security GO language implementation of kcp, including simple implementation of UDP session management, as a base library for subsequent development.
- [kcp-csharp](https://github.com/limpo1989/kcp-csharp): The csharp migration of kcp, containing the session management, which can access the above kcp-go server.
- [kcp-rs](https://github.com/en/kcp-rs): The rust migration of KCP
- [lua-kcp](https://github.com/linxiaolong/lua-kcp): Lua extension of KCP, applicable for Lua server
- [node-kcp](https://github.com/leenjewel/node-kcp): KCP interface for node-js 
- [nysocks](https://github.com/oyyd/nysocks): Nysocks provides proxy services base on libuv and kcp for nodejs users. Both SOCKS5 and ss protocols are supported in the client.
- [shadowsocks-android](https://github.com/shadowsocks/shadowsocks-android): Shadowsocks for android has integrated kcptun using kcp protocol to accelerate shadowsocks, with good results
- [kcpuv](https://github.com/elisaday/kcpuv): The kcpuv library developed with libuv, currently still in the early alpha phase.
- [xkcptun](https://github.com/liudf0716/xkcptun): C language implementation of kcptun, embedded-friendly for [LEDE](https://github.com/lede-project/source) and [OpenWrt](https://github.com/openwrt/openwrt) projects.


# Protocol Comparison

If the network is never congested, KCP/TCP performance is similar; but the network itself is not reliable, and packet loss and jitter may be inevitable (otherwise why there are various reliable protocols). Compared in the intranet environment which is almost ideal, they have similar performance, but on the public Internet, under 3G / 4G network situation, or using the intranet packet loss simulation, the gap is obvious. The public network has an average of nearly 10% packet loss during peak times, which is even worse in wifi / 3g / 4g network, all of which will cause transmission congestion.

Thanks to [zhangyuan](https://github.com/libinzhangyuan) the author of [asio-kcp](https://github.com/libinzhangyuan/asio_kcp) for the horizontal evaluation on KCP, enet and udt, and the conclusions are as follows:

- ASIO-KCP **has good performace in wifi and phone network(3G, 4G)**.
- The kcp is the **first choice for realtime pvp game**.
- The lag is less than 1 second when network lag happen. **3 times better than enet** when lag happen.
- The enet is a good choice if your game allow 2 second lag.
- **UDT is a bad idea**. It always sink into badly situation of more than serval seconds lag. And the recovery is not expected.
- enet has the problem of lack of doc. And it has lots of functions that you may intrest.
- kcp's doc is in both chinese and english. Good thing is the function detail which is writen in code is english. And you can use asio_kcp which is a good wrap.
- The kcp is a simple thing. You will write more code if you want more feature.
- UDT has a perfect doc. UDT may has more bug than others as I feeling.

For specifics please refer to: [Reliable Udp Benchmark](https://github.com/libinzhangyuan/reliable_udp_bench_mark) and [KCP-Benchmark](https://github.com/skywind3000/kcp/wiki/KCP-Benchmark), for more guidance to the hesitant users.

# KCP is used by

See [Success Stories](https://github.com/skywind3000/kcp/wiki/Success-Stories).

# Donation

![欢迎使用支付宝对该项目进行捐赠](https://raw.githubusercontent.com/skywind3000/kcp/master/donation.png)

Donation is welcome by using alipay, the money will be used to improve the protocol and documentation.


twitter: https://twitter.com/skywind3000
blog: http://www.skywind.me

zhihu: https://www.zhihu.com/people/skywind3000
