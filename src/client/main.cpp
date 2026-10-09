#include "StunClient.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[])
{
    #ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    #endif

    boost::asio::io_context io_context_p2p;
    boost::asio::io_context io_context_udplistner;
    auto work_guard_p2p = boost::asio::make_work_guard(io_context_p2p);
    auto work_guard_udplistener = boost::asio::make_work_guard(io_context_udplistner);

    udp::resolver resolver(io_context_p2p);

    std::string ip;
    std::string port("3478");

    std::cout << "[Server Ip]:" << std::flush;
    std::getline(std::cin, ip);

    if (argc == 2) //stunclient mode:0.mc server 1.mc client
    {
        char* mode = argv[1];
        auto endpoints = resolver.resolve(udp::v4(), ip, port);
        udp::endpoint targetEndpoint = *endpoints.begin();

        StunClient client(io_context_p2p, 0, targetEndpoint);
        

        std::thread net_thread([&io_context_p2p](){
            io_context_p2p.run();
        });
        if (mode[0] == '1')
        {
            UdpListener udpListener(io_context_udplistner, 9000, &client.p2pChannel());
            udpListener.start();
            io_context_udplistner.run();
        }
        
        client.waiting();
        work_guard_p2p.reset();
        io_context_p2p.stop();

        if (mode[0] == '1')
        {
            work_guard_udplistener.reset();
            io_context_udplistner.stop();
        }

        
        if (net_thread.joinable()) 
        {
            net_thread.join();
        }
    }
    else if (argc == 3) // port mode
    {
        char* mode = argv[2];
        auto endpoints = resolver.resolve(udp::v4(), ip, port);
        udp::endpoint targetEndpoint = *endpoints.begin();


        StunClient client(io_context_p2p, atoi(argv[1]), targetEndpoint);

        std::thread net_thread([&io_context_p2p](){
            io_context_p2p.run();
        });

        if (mode[0] == '1')
        {
            UdpListener udpListener(io_context_udplistner, 9000, &client.p2pChannel());
            udpListener.start();
            io_context_udplistner.run();
        }
        
        client.waiting();
        work_guard_p2p.reset();
        io_context_p2p.stop();

        if (mode[0] == '1')
        {
            work_guard_udplistener.reset();
            io_context_udplistner.stop();
        }

        if (net_thread.joinable()) 
        {
            net_thread.join();
        }

    }
    else
    {

        printf("usage: %s local_port or %s", argv[0]);
    }
    
}