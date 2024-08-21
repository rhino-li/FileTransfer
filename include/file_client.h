#ifndef __FILE_CLIENT_H__
#define __FILE_CLIENT_H__


#include "file_operator.h"

#define CLIENT_FILE_PATH "./client_file/"


namespace filetrans
{
    class FileClient
    {
    private:
        Socket::socket_ptr m_client_sock; 
        time_t m_create_time; // 连接创建时间
        FileOperator::fileop_ptr m_file_op;
    public:
        FileClient()
        {
            m_file_op.reset(new FileOperator());
        }
        ~FileClient()
        {
            close_client();
        }
        static void showmenu();
        bool quit_proactively();
        void start_client();
        void client_connect(std::string& host_ip,uint16_t port=0);

        bool authentication();
        bool client_login();
        bool client_register();

        bool show_optional_files();
        bool upload_file(); // 上传文件
        bool download_file(); // 下载文件
        void close_client();
    };
    
}

#endif