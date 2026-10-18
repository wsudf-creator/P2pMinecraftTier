#pragma once

#include <iostream>
#include <memory>
#include <array>
#include <boost/asio.hpp>
#include <functional>

using boost::asio::ip::tcp;

class LocalMinecraftServer : public std::enable_shared_from_this<LocalMinecraftServer> 
{
public:
    LocalMinecraftServer(boost::asio::io_context& io_context, short port, std::function<void(const char*, std::size_t)> fn);
    // 当从 P2P/KCP 收到房主端发来的数据时，调用此方法写入本地游戏客户端
    void send_to_game(const char* data, std::size_t size);

    void setKcpCallback(std::function<void(const char*, std::size_t)> callback_)
    {
        kcpSendCallback_ = callback_;
    }

private:
    void start_accept();
   
    void start_read();
    std::function<void(const char*, std::size_t)> kcpSendCallback_;

    tcp::acceptor acceptor_;
    tcp::socket socket_;
    std::array<char, 4096> read_buffer_;
    bool is_connected_ = false;
};