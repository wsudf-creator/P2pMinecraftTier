#include <iostream>
#include <memory>
#include <vector>
#include <boost/asio.hpp>
#include "P2pChannel.h"

using boost::asio::ip::udp;

class UdpListener
{
public:
    UdpListener(boost::asio::io_context& io_context, unsigned short port, std::atomic<std::shared_ptr<P2pChannel>>* channel);

    // 启动异步接收循环

    void setChannel(std::shared_ptr<P2pChannel>& channel)
    {
        channel_->store(channel);
    }

    void start()
    {
        do_receive();
    }

    udp::endpoint& getGostEndpoint_()
    {
        return remote_endpoint_;
    }
    

private:
    void do_receive(); 
    void on_receive(const char* data, std::size_t length, const udp::endpoint& from_ep);

    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    std::vector<char> recv_buffer_;
    std::atomic<std::shared_ptr<P2pChannel>>* channel_;
};
