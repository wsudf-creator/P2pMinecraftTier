#include "LocalMinecraftServer.h"

LocalMinecraftServer::LocalMinecraftServer(boost::asio::io_context& io_context, short port, std::function<void(const char*, std::size_t)> fn)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
        socket_(io_context) 
{
    setKcpCallback(fn);
    start_accept();
}

    // 当从 P2P/KCP 收到房主端发来的数据时，调用此方法写入本地游戏客户端
void LocalMinecraftServer::send_to_game(const char* data, std::size_t size) 
{
    if (!is_connected_)
    { 
        std::cout << "[LocalMCServer] isn't connected" << std::endl;
        return;
    }
    
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(data, size),
        [this, self](boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
            if (ec) 
            {
                std::cerr << "[LocalMCServer] Write to game failed: " << ec.message() << std::endl;
            }
        });
}

void LocalMinecraftServer::start_accept()
{
    acceptor_.async_accept(socket_, [this](boost::system::error_code ec) {
        if (!ec) 
        {
            std::cout << "[LocalMCServer] Player's Minecraft client connected locally!" << std::endl;
            is_connected_ = true;
            start_read();
        } 
        else 
        {
            std::cerr << "[LocalMCServer] Accept failed: " << ec.message() << std::endl;
        }
    });
}

void LocalMinecraftServer::start_read()
{
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(read_buffer_),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (!ec) 
            {
                // 这里收到了来自本地游戏客户端的 TCP 数据
                // 将其压入 KCP 发送给远端房主：
                kcpSendCallback_(read_buffer_.data(), bytes_transferred);

                std::cout << "[LocalMCServer] Read " << bytes_transferred 
                            << " bytes from local game, forwarding to P2P..." << std::endl;

                start_read();
            } 
            else 
            {
                is_connected_ = false;
                std::cerr << "[LocalMCServer] Game client disconnected: " << ec.message() << std::endl;
                // 可以选择重新开始等待下一个连接
                start_accept();
            }
        });
}