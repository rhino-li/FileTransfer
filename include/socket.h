/**
* 封装底层socket接口
*
**/

#ifndef __FILE_SOCKET_H__
#define __FILE_SOCKET_H__

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <memory>
#include <cstring>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <unistd.h>

namespace filetrans 
{
    class IPAddress : public std::enable_shared_from_this<IPAddress> 
    {
        private:
            sockaddr_in m_addr;
            sockaddr_in6 m_addr6;
            int m_flag; // ==4:ipv4,==6:ipv6 标识是什么类型的IP地址
        public:
            typedef std::shared_ptr<IPAddress> ipaddr_ptr;
            IPAddress(int flag=4);
            IPAddress(int flag, uint8_t address,uint16_t port=0);
            static ipaddr_ptr Create(const char* address, uint16_t port=0);
            int get_family();
            sockaddr* get_addr();
            socklen_t get_addr_len();
    };

    class Socket : public std::enable_shared_from_this<Socket> 
    {
        protected:
            int m_socket; // 句柄
            int m_family;
            int m_type;
            int m_protocol;
            bool is_connected;
            

        public:
            IPAddress::ipaddr_ptr m_localAddress;
            IPAddress::ipaddr_ptr m_remoteAddress;
            typedef std::shared_ptr<Socket> socket_ptr;
            
            enum Family 
            {
                Ipv4 = AF_INET,
                Ipv6 = AF_INET6,
                Unix = AF_UNIX
            };
            enum Type 
            {
                TCP = SOCK_STREAM,
                UDP = SOCK_DGRAM
            };
            
            Socket(int family,int type,int protocol=0);
            ~Socket();
            void init_socket();

            static Socket::socket_ptr create_tcp_socket();
            bool setOption(int level,int option,const void *result,socklen_t len);
            bool getOption(int level,int option,const void *result,socklen_t len);
            IPAddress::ipaddr_ptr getLocalAddress(); // getsockname
            IPAddress::ipaddr_ptr getRemoteAddress(); // getpeername
            bool bind(const IPAddress::ipaddr_ptr address);
            bool listen(int backlog = SOMAXCONN);
            Socket::socket_ptr accept();
            bool connect(const IPAddress::ipaddr_ptr address);
            int send(const void* buffer,size_t length,int flags); 
            int recv(void* buffer,size_t length,int flags);
            // TODO: sendto() recvfrom()
            bool close(); // 关闭socket
            void clear_recv_buffer();
            
    };
}



#endif