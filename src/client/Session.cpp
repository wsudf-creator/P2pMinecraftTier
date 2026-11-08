#include "Session.h"

PlayerSession::PlayerSession(boost::asio::io_context& io_context, udp::endpoint client_ep, const std::string& mc_host, short mc_port, SendCallback cb)
        : io_context_(io_context), client_endpoint_(client_ep), 
          tcp_socket_(io_context), mc_endpoint_(boost::asio::ip::make_address(mc_host), mc_port), send_callback_(std::move(cb)), kcp_timer_(io_context)
{

    // 初始化该玩家专属的 KCP (假设 user_id 为 1)
    kcp_ = ikcp_create(1, this);
    kcp_->output = [](const char* buf, int len, ikcpcb* kcp, void* user) -> int {
        auto* session = static_cast<PlayerSession*>(user);

        std::vector<char> sendBuf(len + 1);
        sendBuf[0] = static_cast<char>(0xdf); //Message head
        std::memcpy(sendBuf.data() + 1, buf, len);

        session->send_p2p_packet(sendBuf.data(), sendBuf.size());
        return 0;
    };
    ikcp_setmtu(kcp_, 1400);
    ikcp_wndsize(kcp_, 2048, 2048);
    ikcp_nodelay(kcp_, 1, 10, 2, 1);

    startKcpTimer();
}

PlayerSession::~PlayerSession()
{
    if (kcp_) ikcp_release(kcp_);
}

// 启动：为该玩家连接本地 Minecraft 服务端
void PlayerSession::start()
{
    auto self(shared_from_this());
    tcp_socket_.async_connect(mc_endpoint_, [this, self](boost::system::error_code ec) {
        if (!ec) 
        {
            std::cout << "[Session] Connected to local MC for client: " << client_endpoint_.address().to_string() << std::endl;
            is_tcp_connected_ = true;
                
            //TCP 连上后，把期间积压的包一次性处理掉
            flush_pending_packets();
            
            start_read_tcp();
        } 
        else 
        {
            std::cerr << "[Session] MC connect failed: " << ec.message() << std::endl;
        }
    });
}

void PlayerSession::handle_incoming_p2p(const char* data, std::size_t size)
{
    if (size < 24)
    {
        return ;
    }
    ikcp_input(kcp_, data, size);

    std::cout << "[Host]: KCP decrypted " << size << " bytes, writing to local MC..." << std::endl;
    
    // 循环读干 KCP 缓冲区，写入专属的本地 TCP
    char kcp_buf[65535];
    while (true) 
    {
        int hr = ikcp_recv(kcp_, kcp_buf, sizeof(kcp_buf));
        if (hr <= 0) break;

        if (!is_tcp_connected_) 
        {
            // 如果 TCP 还没握手成功，先缓存起来
            pending_packets_.emplace_back(kcp_buf, kcp_buf + hr);
        }
        else
        {
            send_to_tcp(kcp_buf, hr);
        }
    }
}

