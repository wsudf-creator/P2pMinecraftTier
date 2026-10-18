#include "LocalMinecraftClient.h"

LocalMinecraftClient::LocalMinecraftClient(boost::asio::io_context& io_context, const std::string& host, short port)
    : socket_(io_context), endpoint_(boost::asio::ip::make_address(host), port) 
{

}

void LocalMinecraftClient::send_to_minecraft(const char* data, std::size_t size)
{
    auto self(shared_from_this());
    // 投递到 socket 绑定的 io_context 线程，保证线程安全
    boost::asio::post(socket_.get_executor(), [this, self, payload = std::vector<char>(data, data + size)]() mutable {
        write_queue_.push(std::move(payload));
        if (!is_writing_) 
        {
            start_write();
        }
    });
} 

void LocalMinecraftClient::connect_to_minecraft()
{
    auto self(shared_from_this());
    socket_.async_connect(endpoint_, [this, self](boost::system::error_code ec) {
        if (!ec) 
        {
            std::cout << "[LocalMC] Connected to local Minecraft server (127.0.0.1:25565) successfully!" << std::endl;
            start_read();
        } 
        else 
        {
            std::cerr << "[LocalMC] Connect failed: " << ec.message() << std::endl;
        }
    });
}

void LocalMinecraftClient::start_read()
{
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(read_buffer_),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (!ec) 
            {
                // 这里收到了来自本地 Minecraft 服务端吐出的 TCP 数据
                // 可以直接将其压入 KCP 发送给远端玩家：
                kcpSendCallback_(read_buffer_.data(), bytes_transferred);

                std::cout << "[LocalMC] Read " << bytes_transferred 
                            << " bytes from Minecraft, forwarding to P2P..." << std::endl;

                start_read();
            } 
            else 
            {
                std::cerr << "[LocalMC] Connection closed or error: " << ec.message() << std::endl;
            }
        });
}

void LocalMinecraftClient::start_write()
{
    is_writing_ = true;
    auto self(shared_from_this());

    boost::asio::async_write(
        socket_,
        boost::asio::buffer(write_queue_.front()),
        [this, self](boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
            if (!ec) 
            {
                write_queue_.pop();
                if (!write_queue_.empty()) 
                {
                    start_write(); // 递归发送下一包
                } 
                else 
                {
                    is_writing_ = false;
                }
            } 
            else 
            {
                std::cerr << "[LocalMC] Write error: " << ec.message() << std::endl;
                std::queue<std::vector<char>> empty;
                std::swap(write_queue_, empty); // 清空队列
                is_writing_ = false;
            }
        });
}