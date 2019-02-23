KCP - A Fast and Reliable ARQ Protocol
======================================

[![Powered][2]][1] [![Build Status][4]][5]
[![Backers on Open Collective](https://opencollective.com/kcp/backers/badge.svg)](#backers)
 [![Sponsors on Open Collective](https://opencollective.com/kcp/sponsors/badge.svg)](#sponsors) 

[1]: https://github.com/skywind3000/kcp
[2]: http://skywind3000.github.io/word/images/kcp.svg
[3]: https://raw.githubusercontent.com/skywind3000/kcp/master/kcp.svg
[4]: https://api.travis-ci.org/skywind3000/kcp.svg?branch=master
[5]: https://travis-ci.org/skywind3000/kcp

[README in English](https://github.com/skywind3000/kcp/blob/master/README.en.md) 

# 简介

KCP是一个快速可靠协议，能以比 TCP浪费10%-20%的带宽的代价，换取平均延迟降低 30%-40%，且最大延迟降低三倍的传输效果。纯算法实现，并不负责底层协议（如UDP）的收发，需要使用者自己定义下层数据包的发送方式，以 callback的方式提供给 KCP。 连时钟都需要外部传递进来，内部不会有任何一次系统调用。

整个协议只有 ikcp.h, ikcp.c两个源文件，可以方便的集成到用户自己的协议栈中。也许你实现了一个P2P，或者某个基于 UDP的协议，而缺乏一套完善的ARQ可靠协议实现，那么简单的拷贝这两个文件到现有项目中，稍微编写两行代码，即可使用。


# 技术特性

TCP是为流量设计的（每秒内可以传输多少KB的数据），讲究的是充分利用带宽。而 KCP是为流速设计的（单个数据包从一端发送到一端需要多少时间），以10%-20%带宽浪费的代价换取了比 TCP快30%-40%的传输速度。TCP信道是一条流速很慢，但每秒流量很大的大运河，而KCP是水流湍急的小激流。KCP有正常模式和快速模式两种，通过以下策略达到提高流速的结果：

#### RTO翻倍vs不翻倍：

   TCP超时计算是RTOx2，这样连续丢三次包就变成RTOx8了，十分恐怖，而KCP启动快速模式后不x2，只是x1.5（实验证明1.5这个值相对比较好），提高了传输速度。

#### 选择性重传 vs 全部重传：

   TCP丢包时会全部重传从丢的那个包开始以后的数据，KCP是选择性重传，只重传真正丢失的数据包。

#### 快速重传：

   发送端发送了1,2,3,4,5几个包，然后收到远端的ACK: 1, 3, 4, 5，当收到ACK3时，KCP知道2被跳过1次，收到ACK4时，知道2被跳过了2次，此时可以认为2号丢失，不用等超时，直接重传2号包，大大改善了丢包时的传输速度。

#### 延迟ACK vs 非延迟ACK：

   TCP为了充分利用带宽，延迟发送ACK（NODELAY都没用），这样超时计算会算出较大 RTT时间，延长了丢包时的判断过程。KCP的ACK是否延迟发送可以调节。

#### UNA vs ACK+UNA：

   ARQ模型响应有两种，UNA（此编号前所有包已收到，如TCP）和ACK（该编号包已收到），光用UNA将导致全部重传，光用ACK则丢失成本太高，以往协议都是二选其一，而 KCP协议中，除去单独的 ACK包外，所有包都有UNA信息。

#### 非退让流控：

   KCP正常模式同TCP一样使用公平退让法则，即发送窗口大小由：发送缓存大小、接收端剩余接收缓存大小、丢包退让及慢启动这四要素决定。但传送及时性要求很高的小数据时，可选择通过配置跳过后两步，仅用前两项来控制发送频率。以牺牲部分公平性及带宽利用率之代价，换取了开着BT都能流畅传输的效果。


# 基本使用

1. 创建 KCP对象：

   ```cpp
   // 初始化 kcp对象，conv为一个表示会话编号的整数，和tcp的 conv一样，通信双
   // 方需保证 conv相同，相互的数据包才能够被认可，user是一个给回调函数的指针
   ikcpcb *kcp = ikcp_create(conv, user);
   ```

2. 设置回调函数：

   ```cpp
   // KCP的下层协议输出函数，KCP需要发送数据时会调用它
   // buf/len 表示缓存和长度
   // user指针为 kcp对象创建时传入的值，用于区别多个 KCP对象
   int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
   {
     ....
   }
   // 设置回调函数
   kcp->output = udp_output;
   ```

3. 循环调用 update：

   ```cpp
   // 以一定频率调用 ikcp_update来更新 kcp状态，并且传入当前时钟（毫秒单位）
   // 如 10ms调用一次，或用 ikcp_check确定下次调用 update的时间不必每次调用
   ikcp_update(kcp, millisec);
   ```

4. 输入一个下层数据包：

   ```cpp
   // 收到一个下层数据包（比如UDP包）时需要调用：
   ikcp_input(kcp, received_udp_packet, received_udp_size);
   ```
   处理了下层协议的输出/输入后 KCP协议就可以正常工作了，使用 ikcp_send 来向
   远端发送数据。而另一端使用 ikcp_recv(kcp, ptr, size)来接收数据。


# 协议配置

协议默认模式是一个标准的 ARQ，需要通过配置打开各项加速开关：

1. 工作模式：
   ```cpp
   int ikcp_nodelay(ikcpcb *kcp, int nodelay, int interval, int resend, int nc)
   ```

   - nodelay ：是否启用 nodelay模式，0不启用；1启用。
   - interval ：协议内部工作的 interval，单位毫秒，比如 10ms或者 20ms
   - resend ：快速重传模式，默认0关闭，可以设置2（2次ACK跨越将会直接重传）
   - nc ：是否关闭流控，默认是0代表不关闭，1代表关闭。
   - 普通模式： ikcp_nodelay(kcp, 0, 40, 0, 0);
   - 极速模式： ikcp_nodelay(kcp, 1, 10, 2, 1);

2. 最大窗口：
   ```cpp
   int ikcp_wndsize(ikcpcb *kcp, int sndwnd, int rcvwnd);
   ```
   该调用将会设置协议的最大发送窗口和最大接收窗口大小，默认为32. 这个可以理解为 TCP的 SND_BUF 和 RCV_BUF，只不过单位不一样 SND/RCV_BUF 单位是字节，这个单位是包。

3. 最大传输单元：

   纯算法协议并不负责探测 MTU，默认 mtu是1400字节，可以使用ikcp_setmtu来设置该值。该值将会影响数据包归并及分片时候的最大传输单元。

4. 最小RTO：

   不管是 TCP还是 KCP计算 RTO时都有最小 RTO的限制，即便计算出来RTO为40ms，由于默认的 RTO是100ms，协议只有在100ms后才能检测到丢包，快速模式下为30ms，可以手动更改该值：
   ```cpp
   kcp->rx_minrto = 10;
   ```


# 文档索引

协议的使用和配置都是很简单的，大部分情况看完上面的内容基本可以使用了。如果你需要进一步进行精细的控制，比如改变 KCP的内存分配器，或者你需要更有效的大规模调度 KCP链接（比如 3500个以上），或者如何更好的同 TCP结合，那么可以继续延伸阅读：

- [Wiki Home](https://github.com/skywind3000/kcp/wiki)
- [KCP 最佳实践](https://github.com/skywind3000/kcp/wiki/KCP-Best-Practice)
- [同现有TCP服务器集成](https://github.com/skywind3000/kcp/wiki/Cooperate-With-Tcp-Server)
- [传输数据加密](https://github.com/skywind3000/kcp/wiki/Network-Encryption)
- [应用层流量控制](https://github.com/skywind3000/kcp/wiki/Flow-Control-for-Users)
- [性能评测](https://github.com/skywind3000/kcp/wiki/KCP-Benchmark)


# 开源案例

- [kcptun](https://github.com/xtaci/kcptun): 基于 kcp-go做的高速远程端口转发(隧道) ，配合ssh -D，可以比 shadowsocks 更流畅的看在线视频。
- [dog-tunnel](https://github.com/vzex/dog-tunnel): GO开发的网络隧道，使用 KCP极大的改进了传输速度，并移植了一份 GO版本 KCP
- [v2ray](https://www.v2ray.com)：著名代理软件，Shadowsocks 代替者，1.17后集成了 kcp协议，使用UDP传输，无数据包特征。
- [asio-kcp](https://github.com/libinzhangyuan/asio_kcp): 使用 KCP的完整 UDP网络库，完整实现了基于 UDP的链接状态管理，会话控制，KCP协议调度等
- [kcp-java](https://github.com/hkspirt/kcp-java)：Java版本 KCP协议实现。
- [kcp-netty](https://github.com/szhnet/kcp-netty)：kcp的Java语言实现，基于netty。
- [kcp-go](https://github.com/xtaci/kcp-go): 高安全性的kcp的 GO语言实现，包含 UDP会话管理的简单实现，可以作为后续开发的基础库。 
- [kcp-csharp](https://github.com/limpo1989/kcp-csharp): kcp的 csharp移植，同时包含一份回话管理，可以连接上面kcp-go的服务端。
- [kcp-csharp](https://github.com/KumoKyaku/KCP): 新版本 Kcp的 csharp移植。线程安全，运行时无alloc，对gc无压力。
- [kcp-rs](https://github.com/en/kcp-rs): KCP的 rust移植
- [kcp-rust](https://github.com/Matrix-Zhang/kcp)：新版本 KCP的 rust 移植
- [tokio-kcp](https://github.com/Matrix-Zhang/tokio_kcp)：rust tokio 的 kcp 集成
- [lua-kcp](https://github.com/linxiaolong/lua-kcp): KCP的 Lua扩展，用于 Lua服务器
- [node-kcp](https://github.com/leenjewel/node-kcp): node-js 的 KCP 接口  
- [nysocks](https://github.com/oyyd/nysocks): 基于libuv实现的[node-addon](https://nodejs.org/api/addons.html)，提供nodejs版本的代理服务，客户端接入支持SOCKS5和ss两种协议
- [shadowsocks-android](https://github.com/shadowsocks/shadowsocks-android): Shadowsocks for android 集成了 kcptun 使用 kcp协议加速 shadowsocks，效果不错
- [kcpuv](https://github.com/elisaday/kcpuv): 使用 libuv开发的kcpuv库，目前还在 Demo阶段
- [Lantern](https://getlantern.org/)：更好的 VPN，Github 50000 星，使用 kcpgo 加速
- [rpcx](https://github.com/smallnest/rpcx) ：RPC 框架，1000+ 星，使用 kcpgo 加速 RPC
- [xkcptun](https://github.com/liudf0716/xkcptun): c语言实现的kcptun，主要用于[OpenWrt](https://github.com/openwrt/openwrt), [LEDE](https://github.com/lede-project/source)开发的路由器项目上
- [et-frame](https://github.com/egametang/ET): C#前后端框架(前端unity3d)，统一用C#开发游戏，实现了前后端kcp协议

# 商业案例

- [明日帝国](https://www.taptap.com/app/50664)：Game K17 的 《明日帝国》 （Google Play），使用 KCP 加速游戏消息，让全球玩家流畅联网
- [仙灵大作战](https://www.taptap.com/app/27242)：4399 的 MOBA游戏，使用 KCP 优化游戏同步
- [CC](http://cc.163.com/)：网易 CC 使用 kcp 加速视频推流，有效提高流畅性
- [BOBO](http://bobo.163.com/)：网易 BOBO 使用 kcp 加速主播推流
- [云帆加速](http://www.yfcloud.com/)：使用 KCP 加速文件传输和视频推流，优化了台湾主播推流的流畅度

欢迎告知更多案例

# 协议比较

如果网络永远不卡，那 KCP/TCP 表现类似，但是网络本身就是不可靠的，丢包和抖动无法避免（否则还要各种可靠协议干嘛）。在内网这种几乎理想的环境里直接比较，大家都差不多，但是放到公网上，放到3G/4G网络情况下，或者使用内网丢包模拟，差距就很明显了。公网在高峰期有平均接近10%的丢包，wifi/3g/4g下更糟糕，这些都会让传输变卡。

感谢 [asio-kcp](https://github.com/libinzhangyuan/asio_kcp) 的作者 [zhangyuan](https://github.com/libinzhangyuan) 对 KCP 与 enet, udt做过的一次横向评测，结论如下：

- ASIO-KCP **has good performace in wifi and phone network(3G, 4G)**.
- The kcp is the **first choice for realtime pvp game**.
- The lag is less than 1 second when network lag happen. **3 times better than enet** when lag happen.
- The enet is a good choice if your game allow 2 second lag.
- **UDT is a bad idea**. It always sink into badly situation of more than serval seconds lag. And the recovery is not expected.
- enet has the problem of lack of doc. And it has lots of functions that you may intrest.
- kcp's doc is chinese. Good thing is the function detail which is writen in code is english. And you can use asio_kcp which is a good wrap.
- The kcp is a simple thing. You will write more code if you want more feature.
- UDT has a perfect doc. UDT may has more bug than others as I feeling.

具体见：[横向比较](https://github.com/libinzhangyuan/reliable_udp_bench_mark) 和 [评测数据](https://github.com/skywind3000/kcp/wiki/KCP-Benchmark)，为犹豫选择的人提供了更多指引。



# 欢迎捐赠

![欢迎使用支付宝对该项目进行捐赠](https://raw.githubusercontent.com/skywind3000/kcp/master/donation.png)

欢迎使用支付宝手扫描上面的二维码，对该项目进行捐赠。捐赠款项将用于持续优化 KCP协议以及完善文档。

感谢：明明、星仔、进、帆、颁钊、斌铨、晓丹、余争、虎、晟敢、徐玮、王川、赵刚强、胡知锋、万新朝、何新超、刘旸、侯宪辉、吴佩仪、华斌、如涛、胡坚。。。（早先的名单实在不好意思没记录下来）等同学的捐助与支持。


欢迎关注

KCP交流群：364933586（QQ群号），KCP集成，调优，网络传输以及相关技术讨论
Gitter 群：https://gitter.im/skywind3000/KCP

blog: http://www.skywind.me

zhihu: https://www.zhihu.com/people/skywind3000

## Contributors

This project exists thanks to all the people who contribute. 
<a href="https://github.com/skywind3000/kcp/graphs/contributors"><img src="https://opencollective.com/kcp/contributors.svg?width=890&button=false" /></a>


## Backers

Thank you to all our backers! 🙏 [[Become a backer](https://opencollective.com/kcp#backer)]

<a href="https://opencollective.com/kcp#backers" target="_blank"><img src="https://opencollective.com/kcp/backers.svg?width=890"></a>


## Sponsors

Support this project by becoming a sponsor. Your logo will show up here with a link to your website. [[Become a sponsor](https://opencollective.com/kcp#sponsor)]

<a href="https://opencollective.com/kcp/sponsor/0/website" target="_blank"><img src="https://opencollective.com/kcp/sponsor/0/avatar.svg"></a>
<a href="https://opencollective.com/kcp/sponsor/1/website" target="_blank"><img src="https://opencollective.com/kcp/sponsor/1/avatar.svg"></a>
<a href="https://opencollective.com/kcp/sponsor/2/website" target="_blank"><img src="https://opencollective.com/kcp/sponsor/2/avatar.svg"></a>
<a href="https://opencollective.com/kcp/sponsor/3/website" target="_blank"><img src="https://opencollective.com/kcp/sponsor/3/avatar.svg"></a>
<a href="https://opencollective.com/kcp/sponsor/4/website" target="_blank"><img src="https://opencollective.com/kcp/sponsor/4/avatar.svg"></a>
<a href="https://opencollective.com/kcp/sponsor/5/website" target="_blank"><img src="https://opencollective.com/kcp/sponsor/5/avatar.svg"></a>
<a href="https://opencollective.com/kcp/sponsor/6/website" target="_blank"><img src="https://opencollective.com/kcp/sponsor/6/avatar.svg"></a>
<a href="https://opencollective.com/kcp/sponsor/7/website" target="_blank"><img src="https://opencollective.com/kcp/sponsor/7/avatar.svg"></a>
<a href="https://opencollective.com/kcp/sponsor/8/website" target="_blank"><img src="https://opencollective.com/kcp/sponsor/8/avatar.svg"></a>
<a href="https://opencollective.com/kcp/sponsor/9/website" target="_blank"><img src="https://opencollective.com/kcp/sponsor/9/avatar.svg"></a>


