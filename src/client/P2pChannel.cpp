#include "P2pChannel.h"


int udpOutputCallback(const char* buf, int len, ikcpcb* kcp, void* user)
{
    auto* client = static_cast<P2pChannel*>(user);

    std::vector<char> sendBuf(len + 1);
    sendBuf[0] = static_cast<char>(0xdf);
    std::memcpy(sendBuf.data() + 1, buf, len);

    client->socket()->async_send_to(
        boost::asio::buffer(sendBuf), client->peerEndpoint(),
        [](const boost::system::error_code& ec, std::size_t){
            if (ec) 
            {
                std::cerr << "\n[udpOutputCallback]发送失败: " << ec.message() << "\n> " << std::flush;
            }
        }
    );

    return 0;
}

P2pChannel::P2pChannel(boost::asio::io_context* ioctxPtr, udp::socket* socket)
    : ioctx_(ioctxPtr),
      heartbeat_timer_(*ioctxPtr),
      kcp_timer_(*ioctxPtr),
      last_active_time_(std::chrono::steady_clock::now())
{
    socket_ = socket;
}

void P2pChannel::createKcpConversation()
{
    if (!conv_)
    {
        std::cerr << "[UdpChat::createKcpConversation]:Invalid conv" << std::endl;
    }

    kcp_ = ikcp_create(conv_, static_cast<void*>(this));
    kcp_->output = udpOutputCallback;
    std::cout << "kcp session established" << std::endl;

    ikcp_nodelay(kcp_, 1, 10, 2, 1); 
    ikcp_wndsize(kcp_, 256, 256);
    ikcp_setmtu(kcp_, 1350); // 防 IP 分片
}

    // 发送消息到目标节点
void P2pChannel::send(const std::string& message, MessageType type) 
{
    boost::asio::post(socket_->get_executor(), [this, message, type](){
        // if (peer_endpoint_.size() == 0)
        // {
        //     std::cout << "[Invalid peer address]" << std::endl;
        //     return ;
        // }

        auto send_data = std::make_shared<std::string>();
        send_data->append(message);                          // 拼接文本

        ikcp_send(kcp_, send_data->c_str(), send_data->size());

    });
}

void P2pChannel::send(const char* data, int size)
{
    boost::asio::post(socket_->get_executor(), [this, data, size](){
        if (!kcp_)
        {
            std::cerr << "[P2pChannel]: kcp session hasn't been established" << std::endl;
        }
        ikcp_send(kcp_, data, size);
    });
}


void P2pChannel::sendHeartbeat() 
{
    uint8_t cmd = MessageType::Heartbeating; // 只有一个字节的命令字 0x01
    
    // 走 ikcp_send 发送，KCP 会自动为其加上 24 字节的 KCP 头部
    int ret = ikcp_send(kcp_, reinterpret_cast<const char*>(&cmd), sizeof(cmd));
    if (ret < 0) {
        std::cerr << "[KCP] 心跳包入队失败, code: " << ret << std::endl;
    }
}

void P2pChannel::startKcpTimer()
{
    kcp_timer_.expires_after(std::chrono::milliseconds(10));
    kcp_timer_.async_wait([this](boost::system::error_code ec) {
        if (!ec && kcp_)
        {
            // 获取当前毫秒级时间戳驱动 KCP 状态机
            IUINT32 current_ms = getCurrentTimestampMs();

            ikcp_update(kcp_, current_ms);

            // 递归循环定时器
            startKcpTimer();
        }       
    });
}

void P2pChannel::startHeartbeatLoop()
{
    heartbeat_timer_.expires_after(std::chrono::seconds(1));
    heartbeat_timer_.async_wait([this](boost::system::error_code ec){
        if (!ec)
        {
            sendHeartbeat();
            startHeartbeatLoop();
        }
    });
}