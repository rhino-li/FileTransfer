#include <iostream>
#include <unistd.h>
#include <sys/time.h>
#include "file_client.h"

namespace filetrans
{
    void FileClient::start_client()
    {
        std::string host_ip;
        uint16_t port;
        std::cout<<"please input the hostname or ip address and port of the server: ";
        std::cin>>host_ip>>port;
        client_connect(host_ip,port);
        m_file_op->get_all_files_md5(CLIENT_FILE_PATH);

        
        while(true)
        {
            int choice=0;
            showmenu();
            std::cin.clear();
            std::cin.sync();
            std::cin>>choice;
            char choice_send[4] = {0};
            char flag[4];
            sprintf(choice_send,"%d",choice);
            int rt = m_client_sock->send(choice_send,sizeof(choice_send),0);
            if(rt==-1) break;
            m_client_sock->recv(&flag,4,0);
            if(choice==1)
            {
                bool rt = show_optional_files();
                if(!rt) break;
            }
            else if(choice==2)
            {
                bool rt = upload_file();
                if(!rt) break;
            }
            else if(choice==3)
            {
                bool rt = download_file();
                if(!rt) break;
            }
            else if(choice==4)
            {
                break;
            }
            else std::cout<<"please input valid choice!"<<std::endl;

            m_client_sock->clear_recv_buffer();
            
        }
        close_client();
    }
    void FileClient::showmenu()
    {
        std::cout << "please enter 1, 2, 3,or 4:\n"
            "1) browse           2) upload\n"
            "3) download         4) quit\n";
    }

    void FileClient::client_connect(std::string& host_ip,uint16_t port)
    {
        m_client_sock = Socket::create_tcp_socket();
        hostent* h;
        h = gethostbyname(host_ip.c_str());
        IPAddress::ipaddr_ptr server_addr = IPAddress::Create(host_ip.c_str(),port);

        bool rt = m_client_sock->connect(server_addr);
        if(!rt)
        {
            exit(2);
        }
        m_client_sock->init_socket();
        std::cout<<"connected!"<<std::endl;
        time(&m_create_time);
    }

    bool FileClient::show_optional_files()
    {
        char optional_files[500];
        m_client_sock->recv(optional_files,sizeof(optional_files),0);
        int rt = m_client_sock->send("yes",4,0);
        if(rt==-1) return false;
        std::cout<<"files on server:"<<std::endl;
        char* token;
        token = strtok(optional_files,";");
        while(token)
        {
            std::cout<<token<<std::endl;
            token = strtok(NULL,";");
        }
        return true;
    }

    bool FileClient::upload_file()
    {
        filetrans::FileHeader filectx;
        char filepath[FILE_NAME_LEN] = CLIENT_FILE_PATH;

        while (true)
        {
            char tmp[FILE_NAME_LEN] = {0};
            std::cout<<"please input upload file's name: ";
            std::cin>>tmp;
            strcat(filepath,tmp);

            if(access(filepath,F_OK)==-1)
            {
                std::cout<<"file is not exist."<<std::endl;
                continue;
            }
            else
            {
                strcpy(filectx.filename,tmp);
                break;
            }
            strcpy(filepath,CLIENT_FILE_PATH);
            memset(tmp,0,sizeof(tmp));
        }

        return m_file_op->send_file_client(CLIENT_FILE_PATH,filectx,m_client_sock);
    }

    bool FileClient::download_file()
    {
        char flag[4] = {0};
        char send_file_name[50]={0};
        while(true)
        {
            std::cout<<"please input download file's name: ";
            std::cin>>send_file_name;
            int rt = m_client_sock->send(send_file_name,sizeof(send_file_name),0);
            if(rt==-1) return false;
            memset(flag,0,sizeof(flag));
            sleep(1);
            int ret = m_client_sock->recv(flag,4,0);
            if(strcmp(flag,"yes") == 0)
            {
                break;
            }
        }
        FileHeader recv_filectx;
        return m_file_op->recv_file_client(CLIENT_FILE_PATH,recv_filectx,m_client_sock);
    }

    void FileClient::close_client()
    {
        m_client_sock->close();
    }

}