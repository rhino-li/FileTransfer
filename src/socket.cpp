#include <iostream>
#include "socket.h"

namespace filetrans
{
    /**
     * IPAddress类相关API定义
     *
     * */ 
    IPAddress::IPAddress(int flag)
    {
        m_flag=flag;
        if(m_flag == 4)
        {
            memset(&m_addr,0,sizeof(sockaddr_in));
            m_addr.sin_family = AF_INET;
        }
        else if(m_flag == 6)
        {
            memset(&m_addr6,0,sizeof(sockaddr_in6));
            m_addr6.sin6_family = AF_INET6;
        }
    }
    IPAddress::IPAddress(int flag,uint8_t address,uint16_t port)
    {
        m_flag = flag;
        if(flag == 4)
        {
            memset(&m_addr,0,sizeof(sockaddr_in));
            m_addr.sin_family = AF_INET;
            m_addr.sin_addr.s_addr = htonl(address);
            m_addr.sin_port = htons(port);
        }
        else if(flag == 6)
        {
            memset(&m_addr6,0,sizeof(sockaddr_in6));
            m_addr6.sin6_family = AF_INET6;
            memcpy(&m_addr6.sin6_addr.s6_addr,(uint8_t*)address,16);
            m_addr6.sin6_port = htons(port);
        }
    }
    IPAddress::ipaddr_ptr IPAddress::Create(const char* address, uint16_t port)
    {
        addrinfo hints,*results;
        memset(&hints,0,sizeof(addrinfo));
        hints.ai_flags = AI_NUMERICHOST;
        hints.ai_family = AF_UNSPEC;
        int rt = getaddrinfo(address,NULL,&hints,&results);
        if(rt!=0)
        {
            perror("ip address create error");
            return nullptr;
        }
        try
        {
            sockaddr *addr = results->ai_addr;
            if(addr==nullptr) return nullptr;
            IPAddress::ipaddr_ptr result(new IPAddress());
            
            if(addr->sa_family==AF_INET)
            {
                result->m_flag = 4;
                result->m_addr=*(sockaddr_in*)addr; 
                result->m_addr.sin_port = htons(port);
            }
            else if(addr->sa_family==AF_INET6)
            {
                result->m_flag = 6;
                result->m_addr6=*(sockaddr_in6*)addr; 
                result->m_addr6.sin6_port = htons(port);
            }
            freeaddrinfo(results);
            return result;
        }
        catch(...)
        {
            freeaddrinfo(results);
            return nullptr;
        }
    }
    int IPAddress::get_family()
    {
        if(m_flag==4)
        {
            return m_addr.sin_family;
        }
        else
        {
            return m_addr6.sin6_family;
        }
    }
    sockaddr* IPAddress::get_addr()
    {
        if(m_flag==4)
        {
            return (sockaddr*)&m_addr;
        }
        else
        {
            return (sockaddr*)&m_addr6;
        }
    }
    socklen_t IPAddress::get_addr_len()
    {
        if(m_flag==4)
        {
            return sizeof(m_addr);
        }
        else
        {
            return sizeof(m_addr6);
        }
    }

    /**
     * Socket类相关API定义
     *
     * */ 
    Socket::Socket(int family,int type,int protocol)
    {
        m_socket = -1;
        m_family = family;
        m_type = type;
        m_protocol = protocol;
        is_connected = false;
    }
    Socket::~Socket()
    {
        close();
    }
    void Socket::init_socket()
    {
        int val=1;
        setOption(SOL_SOCKET,SO_REUSEADDR,&val,(socklen_t)sizeof(val));
        setOption(SOL_SOCKET,SO_KEEPALIVE,&val,(socklen_t)sizeof(val));
        if(m_type == SOCK_STREAM)
        {
            setOption(IPPROTO_TCP,TCP_NODELAY,&val,(socklen_t)sizeof(val));
        }
    }
    Socket::socket_ptr Socket::create_tcp_socket()
    {
        socket_ptr sock(new Socket(Ipv4,TCP,0));
        return sock;
    }
    bool Socket::setOption(int level,int option,const void *result,socklen_t len)
    {
        int rt = setsockopt(m_socket,level,option,result,(socklen_t)len);
        if(rt==-1)
        {
            perror("setsockopt error");
            return false;
        }
        return true;
    }
    IPAddress::ipaddr_ptr Socket::getLocalAddress()
    {
        IPAddress::ipaddr_ptr local_address;
        if(m_family==AF_INET) local_address.reset(new IPAddress(4));
        else if(m_family==AF_INET6) local_address.reset(new IPAddress(6));
        auto addr = local_address->get_addr();
        socklen_t addrsize = sizeof(addr);
        int rt = ::getsockname(m_socket,(sockaddr *)&addr,&addrsize);
        if(rt==-1)
        {
            perror("getsockname error");
            printf("sock=%d\n",m_socket);
        }
        return local_address;
    }
    IPAddress::ipaddr_ptr Socket::getRemoteAddress()
    {
        
        IPAddress::ipaddr_ptr remote_address;

        if(m_family==AF_INET) remote_address.reset(new IPAddress(4));
        else if(m_family==AF_INET6) remote_address.reset(new IPAddress(6));
        auto addr = remote_address->get_addr();
        socklen_t addrsize = sizeof(addr);
        int rt = ::getsockname(m_socket,(sockaddr *)&addr,&addrsize);
        if(rt==-1)
        {
            perror("getpeername error");
            printf("sock=%d\n",m_socket);
        }
        return remote_address;
        
    }

    bool Socket::bind(const IPAddress::ipaddr_ptr address)
    {
        if(m_socket == -1)
        {
            m_socket = ::socket(m_family,m_type,m_protocol);
            if(m_socket!=-1)
            {
                init_socket();
            }
            else
            {
                perror("socket error");
                return false;
            }
        }

        if(address->get_family()!=m_family)
        {
            printf("address family(%d) and socket family(%d) is not equal.",address->get_family(),m_family);
            return false;
        }

        int rt = ::bind(m_socket,address->get_addr(),address->get_addr_len());
        if(rt==-1)
        {
            perror("bind error");
            return false;
        }
        m_localAddress = address;
        return true;
    }

    bool Socket::listen(int backlog)
    {
        if(m_socket==-1)
        {
            printf("listen error:socket fd = -1.");
            return false;
        }
        int rt = ::listen(m_socket,backlog);
        if(rt==-1)
        {
            perror("listen error");
            return false;
        }
        return true;
    }
    Socket::socket_ptr Socket::accept()
    {
        socket_ptr new_socket(new Socket(m_family,m_type,m_protocol));
        sockaddr_in client_addr;
        uint32_t size = sizeof(client_addr);
        
        int connected_fd = ::accept(m_socket,(sockaddr*)&client_addr,&size);
        
        if(connected_fd==-1)
        {
            perror("accept error\n");
            return nullptr;
        }
        printf("accepted client ip:%s:%d\n",inet_ntoa(client_addr.sin_addr),client_addr.sin_port);

        new_socket->m_socket = connected_fd;
        new_socket->is_connected = true;
        new_socket->init_socket();
        new_socket->m_localAddress = new_socket->getLocalAddress();
        new_socket->m_remoteAddress = new_socket->getRemoteAddress();

        return new_socket;
    }
    bool Socket::connect(const IPAddress::ipaddr_ptr address)
    {
        m_remoteAddress = address;
        if(m_socket == -1)
        {
            m_socket = ::socket(m_family,m_type,m_protocol);
            if(m_socket!=-1)
            {
                init_socket();
            }
            else
            {
                perror("socket error");
                return false;
            }
        }
        if(address->get_family()!=m_family)
        {
            printf("address family(%d) and socket family(%d) is not equal.",address->get_family(),m_family);
            return false;
        }

        int rt=::connect(m_socket,address->get_addr(),address->get_addr_len());
        if(rt==-1)
        {
            perror("connect error");
            close();
            return false;
        }

        is_connected = true;
        // m_remoteAddress = getRemoteAddress();
        return true;
    }
    int Socket::recv(void* buffer,size_t length,int flags)
    {
        if(is_connected)
        {
            return ::recv(m_socket,buffer,length,flags);
        }
        printf("socket is not connected");
        return -1;
    }
    int Socket::send(const void* buffer,size_t length,int flags)
    {
        try{
            if(is_connected)
            {
                return ::send(m_socket,buffer,length,flags);
            }
        }
        catch(std::exception& ex)
        {
            std::cerr<<ex.what()<<std::endl;
        }
        printf("socket is not connected");
        return -1;
    }
    bool Socket::close()
    {
        is_connected = false;
        if(m_socket!=-1)
        {
            int rt = ::close(m_socket);
            if(rt==-1)
            {
                printf("close fd(%d)error.",m_socket);
                return false;
            }
            m_socket=-1;
        }
        return true;
    }
    void Socket::clear_recv_buffer()
    {
        struct timeval tmOut;
        tmOut.tv_sec = 0;
        tmOut.tv_usec = 0;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(m_socket, &fds);
        int nRet;
        char tmp[2];
        memset(tmp, 0, sizeof( tmp ) );
        while(1)
        {           
            nRet = select(FD_SETSIZE, &fds, NULL, NULL, &tmOut);
            if(nRet== 0)  break;
            ::recv(m_socket, tmp, 1, 0);
        }
    }
    
}
