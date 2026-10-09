#include "UdpListener.h"


UdpListener::UdpListener(boost::asio::io_context& io_context, uint16_t port, std::atomic<std::shared_ptr<P2pChannel>>* channel)
        : socket_(io_context),
          recv_buffer_(2048),
          channel_(channel)
{ 
    udp::endpoint listen_endpoint(boost::asio::ip::address_v4::loopback(), port); // 仅监听 127.0.0.1
    // 如果需要监听所有网卡，改用：udp::endpoint(udp::v4(), port);

    socket_.open(listen_endpoint.protocol());

    // 2. 配置 Socket 属性 (推荐设置端口复用)
    socket_.set_option(udp::socket::reuse_address(true));

    // 3. 绑定端口
    socket_.bind(listen_endpoint);

    std::cout << "[UDP Listener] Successfully listening on " 
                << listen_endpoint.address().to_string() << ":" << std::dec << listen_endpoint.port() << std::endl;
}

 void UdpListener::do_receive() 
 {  
    // async_receive_from 每次接收数据时都会填充 remote_endpoint_
    socket_.async_receive_from(
        boost::asio::buffer(recv_buffer_), remote_endpoint_,
        [this](boost::system::error_code ec, std::size_t bytes_recvd) {
            if (!ec) 
            {
                // --- 成功收到本地程序 (如 GOST) 发来的数据包 ---
                on_receive(recv_buffer_.data(), bytes_recvd, remote_endpoint_);
            } else 
            {
                // 处理非主动取消的错误
                if (ec != boost::asio::error::operation_aborted) 
                {
                    std::cerr << "[UDP Listener] Receive error: " << ec.message() << std::endl;
                } else 
                {
                    return; // Socket 关闭，终止递归调用
                }
            }

            // 核心：保持异步接收循环，防止 Socket 停止监听
            do_receive();
    });
}


void UdpListener::on_receive(const char* data, std::size_t length, const udp::endpoint& from_ep) 
{
        // 在这里将本地收到包 (例如 GOST 投递来的 TCP-over-UDP 流量) 压入你的 KCP 发送队列
        std::cout << "[UDP Listener] Recv " << length << " bytes from " << from_ep << std::endl;
        if (channel_->load())
        {
            channel_->load()->send(data, length);
        }
        else
        {
            std::cerr << "[UdpListener::on_receive]:P2P通道不存在" << std::endl;
        }
}