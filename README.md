# xibai-vpn

家人们谁懂啊，朋友舍不得花钱打游戏，作为唯一一个妓院出身的怨种不得不贡献出自己的服务器手搓一个简易自用免费vpn帮他们在线局域网快乐玩耍（天朝几家运营商推出了自家的openvpn套餐所以封禁了用户用openvpn这种方案组网的情况）

## 构思

>这里因为是单纯想着和几个怨种朋友玩，所以一切从简，花里胡哨的东西一概不予考虑。

本地的win客户端就采用通用方式：

+ tap虚拟网卡接收以及监听对应的局域网流量
+ 虚拟网卡与公网服务端的流量收发
+ 普通的AES/DES对称加密解密
+ 心跳机制，断路重连

主要是服务器方面，为了躲避服务器运营商的查封，所以不能留下太明显的vpn技术特征，所以服务器这边涉及虚拟网卡通信的部分咱们是统统滴不要，自己搓一个简易的虚拟网关出来。

+ 被动式连接，仅作分发（流量转发处理）
+ 心跳机制，断路重连
+ ~~对应的AES/DES对称加解密~~(考虑性能加解密一并交给客户端)

## 报文结构

src_ip + src_port + dst_ip + dst_port + flag + num + len + data

ip  ：源ip与目标ip 皆为本地虚拟局域网ip
port：双方的端口
flag：包功能标记符
num ：包计数，udp无状态不可信，标记序号方便分组发送时重组数据包
len ：截取到的虚拟网卡数据包的长度
data：客户端获取到的本地网卡经过的完整 ip层数据包（暂时只考虑udp通信的游戏组网）

## 目前实现

~~使用服务端抓取流经网卡目标端口的udp流量包直接解析其中的ip与端口然后转发给客户端~~

在实际实现过程中害怕服务器性能瓶颈导致游戏体验不佳，所以在编写之初尽可能削减服务端的存在感，因此服务端最好就像个代理服务器一样，仅作流量转发，加解密的事情咱们一律相信怨种好友们的cpu了2333

server：

1. 接收数据（同时标记实际ip并持续发送心跳包）
2. 解析数据（仅获取虚拟局域网ip及端口）
3. 转发数据（将获取到的udp加密数据直接转发）

client：

1. 客户端截取tap网卡的udp流量包
2. 提取目标ip及端口，并将数据包加密
3. 将ip、端口、加密数据转发至服务端
4. 从服务端接收数据，解析数据
5. 将解密udp数据包发送至本地tap网卡

## Todo

~~1. 客户端通信功能~~
~~2. 服务端通信功能~~
	~~+ 通信功能需要考虑到局域网ip与实际链路出口ip的重绑定（出口ip可能会因为实际情况发生变化maybe）~~
	~~+ 基本通信功能已完成，但是实际转发数据包与预想的完整数据包转发不同，wintun只能截获到二层数据包也即是说其数据为 ip帧及其以上 ，没有包含 14 字节的 以太网帧，需要修改一下数据包长度处理部分~~
~~3. tap网卡安装~~
4. 流量加解密
5. 心跳重连机制
6. 程序退出时的资源释放