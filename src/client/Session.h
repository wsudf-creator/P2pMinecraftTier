#pragma once

#include <iostream>
#include <memory>
#include <unordered_map>
#include <queue>
#include <vector>
#include <functional>
#include <boost/asio.hpp>
#include "ikcp.h"

using boost::asio::ip::udp;
using boost::asio::ip::tcp;

// 定义玩家的唯一标识（可以用 UDP endpoint 作为 key）
struct EndpointHash 
{
    std::size_t operator()(const udp::endpoint& ep) const 
    {
        return std::hash<std::string>()(ep.address().to_string() + ":" + std::to_string(ep.port()));
    }
};

class PlayerSession : public std::enable_shared_from_this<PlayerSession> {
public:
    using SendCallback = std::function<void(const udp::endpoint&, const char*, std::size_t)>;
    PlayerSession(boost::asio::io_context& io_context, udp::endpoint client_ep, const std::string& mc_host, short mc_port, SendCallback send_cb);

    ~PlayerSession();
    

    // 启动：为该玩家连接本地 Minecraft 服务端
    void start();

    // 处理来自该玩家的 P2P/UDP 原始数据
    void handle_incoming_p2p(const char* data, std::size_t size);

    // 定时器驱动：定期调用 ikcp_update
    void update(IUINT32 current_ms) 
    {
        ikcp_update(kcp_, current_ms);
    }

    void startKcpTimer()
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

    static uint32_t getCurrentTimestampMs() 
    {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        // 转换为毫秒并强转为 32 位无符号整数
        return static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
        );
    }

    udp::endpoint get_client_endpoint() const { return client_endpoint_; }

private:
    void send_p2p_packet(const char* data, std::size_t len) 
    {
        // 这里回调给外层的统一 UDP socket 发送给对应的 client_endpoint_
        // 需通过回调或引用交由全局管理器发送
        if (send_callback_)
        {
            send_callback_(client_endpoint_, data, len);
        }
    }

    void start_read_tcp() 
    {
        auto self(shared_from_this());
        tcp_socket_.async_read_some(boost::asio::buffer(tcp_read_buf_), 
            [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
                if (!ec) 
                {
                    // 从本地 MC 读到数据，压入该玩家的 KCP 发给远端玩家
                    std::cout << "[Session] 从本地 MC 读到响应数据，大小: " << bytes_transferred << " 字节，正在回传..." << std::endl;
                    ikcp_send(kcp_, tcp_read_buf_.data(), bytes_transferred);
                    ikcp_flush(kcp_);
                    start_read_tcp();
                } 
                else
                {
                    std::cerr << "[Session] MC TCP disconnected for client." << std::endl;
                }
            });
    }

    void send_to_tcp(const char* data, std::size_t size) 
    {
        auto self(shared_from_this());
        boost::asio::post(io_context_, [this, self, payload = std::vector<char>(data, data + size)]() mutable {
            write_queue_.push(std::move(payload));
            if (!is_writing_) 
            {
                start_write_tcp();
            }
        });
    }

    void start_write_tcp() 
    {
        is_writing_ = true;
        auto self(shared_from_this());
        boost::asio::async_write(tcp_socket_, boost::asio::buffer(write_queue_.front()),
            [this, self](boost::system::error_code ec, std::size_t) {
                if (!ec) 
                {
                    write_queue_.pop();
                    if (!write_queue_.empty()) 
                    {
                        start_write_tcp();
                    } 
                    else 
                    {
                        is_writing_ = false;
                    }
                } 
                else 
                {
                    is_writing_ = false;
                }
            });
    }

    void flush_pending_packets() 
    {
        for (const auto& packet : pending_packets_) 
        {
            send_to_tcp(packet.data(), packet.size());
        }
        pending_packets_.clear();
    }

    bool is_tcp_connected_ = false;
    std::vector<std::vector<char>> pending_packets_; // 缓存连接建立前

    boost::asio::io_context& io_context_;
    udp::endpoint client_endpoint_;
    tcp::socket tcp_socket_;
    tcp::endpoint mc_endpoint_;
    ikcpcb* kcp_ = nullptr; 
    boost::asio::steady_timer kcp_timer_;
    
    std::array<char, 8192> tcp_read_buf_;
    std::queue<std::vector<char>> write_queue_;
    bool is_writing_ = false;
    SendCallback send_callback_;
};