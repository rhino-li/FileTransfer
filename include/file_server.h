/**
 * 文件传输的相关功能API
 */

#ifndef __FILE_SERVER_H__
#define __FILE_SERVER_H__

#include <string>
#include <vector>
#include <sys/stat.h>
#include "scheduler.h"
#include "file_operator.h"
#include "../src/db/user.h"


// #define OPTIONAL_PATH "./server_file/"

namespace filetrans
{

    class FileServer : public std::enable_shared_from_this<FileServer>, public UserOP
    {
    private:
        std::vector<Socket::socket_ptr> m_socks; // 监听的socket数组
        ThreadPool::thread_pool_ptr m_pool;
        FileOperator m_fileop;
        Mysql m_db;
        bool is_stop;
    public:
        typedef std::shared_ptr<FileServer> file_server_ptr;
        FileServer():is_stop(true)
        {
            m_pool.reset(new ThreadPool(4));
            handle_for_sigpipe();
        };
        ~FileServer()
        {
            server_stop();
        }
        bool server_listen(IPAddress::ipaddr_ptr addr); // 新建socket，绑定地址，开始监听
        void start_server(IPAddress::ipaddr_ptr addr); // 启动服务器，需要在server_bind完成之后
        void server_stop(); // 关闭服务器
        
        bool user_authentication(std::string& optional_path,Socket::socket_ptr connected_sock);
        bool user_login(fileprotocol::AuthRequest* auth_request,fileprotocol::AuthResponseState& auth_response_state);
        bool user_register(fileprotocol::AuthRequest* auth_request,fileprotocol::AuthResponseState& auth_response_state);
        bool user_logout(fileprotocol::AuthRequest* auth_request,fileprotocol::AuthResponseState& auth_response_state);

        bool send_optional_files(std::string optional_path,Socket::socket_ptr connected_sock);
        bool send_file_context(std::string optional_path,Socket::socket_ptr connected_sock); // download
        bool recv_file_context(std::string optional_path,fileprotocol::MsgHeader recv_header,Socket::socket_ptr connected_sock); // upload
        void handle_client(Socket::socket_ptr client);
        void handle_for_sigpipe()
        {
            struct sigaction sa; //信号处理结构体
            memset(&sa, '\0', sizeof(sa));
            sa.sa_handler = SIG_IGN;//设置信号的处理回调函数 这个SIG_IGN宏代表的操作就是忽略该信号 
            sa.sa_flags = 0;
            if(sigaction(SIGPIPE, &sa, NULL))//将信号和信号的处理结构体绑定
            return;
        }
    };

}

#endif
