
/*建立p2p通道后，模拟客户端与Minecraft房主进行tcp连接 */
#pragma once

#include <iostream>
#include <memory>
#include <array>
#include <queue>
#include <boost/asio.hpp>
#include <functional>


using boost::asio::ip::tcp;

class LocalMinecraftClient : public std::enable_shared_from_this<LocalMinecraftClient>
{
public:
    LocalMinecraftClient(boost::asio::io_context& io_context, const std::string& host, short port);

    void start() 
    {
        connect_to_minecraft();
    
    }

    void setKcpCallback(std::function<void(const char*, std::size_t)> callback_)
    {
        kcpSendCallback_ = callback_;
    }


    // 当从 P2P 隧道收到远端玩家的数据时，调用此方法写入本地 Minecraft
    void send_to_minecraft(const char* data, std::size_t size);

private:
    void connect_to_minecraft();

    void start_read();
    void start_write();
   
    std::function<void(const char*, std::size_t)> kcpSendCallback_;

    tcp::socket socket_;
    tcp::endpoint endpoint_;
    std::array<char, 8192> read_buffer_;

    // 发送队列与状态标志
    std::queue<std::vector<char>> write_queue_;
    bool is_writing_ = false;
};