#include <iostream>
#include <unistd.h>
#include <sys/time.h>
#include "../include/file_client.h"

namespace filetrans
{
    void FileClient::start_client()
    {
        std::string host_ip;
        uint16_t port;
        std::cout<<"please input the hostname or ip address and port of the server: ";
        std::cin>>host_ip>>port;
        client_connect(host_ip,port);
        bool flag = authentication(); // 身份验证
        m_file_op->get_all_files_md5(CLIENT_FILE_PATH);
        
        while(flag)
        {
            int choice=0;
            showmenu();
            std::cin.clear();
            std::cin.sync();
            std::cin>>choice;
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
                quit_proactively();
                break;
            }
            else std::cout<<"please input valid choice!"<<std::endl;

            m_client_sock->clear_recv_buffer();
            
        }
        close_client();
    }
    bool FileClient::authentication()
    {
        bool over = true; // 标识身份验证的成功与否
        std::string ss;
        std::cout<<"login(i) or register(r) or logout(o) account? ";std::cin>>ss;
        int times = 3;
        while (times>0)
        {
            // 发送身份验证请求
            std::string username,passwd;
            std::cout<<"please input username: "; std::cin>>username;
            std::cout<<"please input passwd: "; std::cin>>passwd;
            fileprotocol::MsgHeader auth_request_header;
            fileprotocol::MsgBody auth_request_body;
            auth_request_header.set_magic(MY_PROTOCOL_MAGIC);
            auth_request_header.set_version(MY_PROTOCOL_VERSION);
            auth_request_header.set_type(fileprotocol::MsgType::AUTH_REQUEST);
            fileprotocol::AuthRequest* auth_request = auth_request_body.mutable_auth_request();
            if(ss == "i")
            {
                auth_request->set_auth_request_state(fileprotocol::AuthRequestState::LOGIN);
                times--;
            }
            else if(ss == "r")
            {
                auth_request->set_auth_request_state(fileprotocol::AuthRequestState::REGISTER);
            }
            else if(ss == "o")
            {
                auth_request->set_auth_request_state(fileprotocol::AuthRequestState::LOGINOUT);
            }
            else break;
            auth_request->set_username(username);
            auth_request->set_passwd(passwd);
            uint32_t auth_request_body_size = auth_request_body.ByteSize();
            auth_request_header.set_length(auth_request_body_size);
            char auth_request_header_bytes[HEADER_LEN];
            char auth_request_body_bytes[auth_request_body_size];
            auth_request_header.SerializeToArray(auth_request_header_bytes,sizeof(auth_request_header_bytes));
            auth_request_body.SerializeToArray(auth_request_body_bytes,auth_request_body_size);
            if(m_client_sock->send(auth_request_header_bytes,sizeof(auth_request_header_bytes),0)==-1) return false;
            if(m_client_sock->send(auth_request_body_bytes,auth_request_body_size,0)==-1) return false;
            // 接收响应
            fileprotocol::MsgHeader auth_response_header;
            fileprotocol::MsgBody auth_response_body;
            char auth_response_header_bytes[HEADER_LEN];
            m_client_sock->recv(auth_response_header_bytes,sizeof(auth_response_header_bytes),0);
            auth_response_header.ParseFromArray(auth_response_header_bytes,sizeof(auth_response_header_bytes));
            if(auth_response_header.type() != fileprotocol::MsgType::AUTH_RESPONSE)
            {
                break;
            }
            uint32_t auth_response_body_size = auth_response_header.length();
            char auth_response_body_bytes[auth_response_body_size];
            m_client_sock->recv(auth_response_body_bytes,auth_response_body_size,0);
            auth_response_body.ParseFromArray(auth_response_body_bytes,auth_response_body_size);
            fileprotocol::AuthResponse* auth_response = auth_response_body.mutable_auth_response();
            auto state = auth_response->auth_response_state();
            if(state == fileprotocol::AuthResponseState::LOGIN_SUCCESS)
            {
                over = true;
                std::cout<<"login success."<<std::endl;
                break;
            }
            else if(state == fileprotocol::AuthResponseState::REGISTER_SUCCESS)
            {
                over = true;
                std::cout<<"register success. "<<std::endl;
                std::cout<<"automatic login success. "<<std::endl;
                break;
            }
            else if(state == fileprotocol::AuthResponseState::LOGOUT_SUCCESS)
            {
                over = false;
                std::cout<<"logout success."<<std::endl;
                break;
            }
            else if(state == fileprotocol::AuthResponseState::PASSWD_ERROR || state == fileprotocol::AuthResponseState::USER_NOT_EXIT)
            {
                ss.clear();
                std::cout<<"passwd error or user not exist, login again(i) or register(r)? ";
                std::cin>>ss;
            }
            else if(state == fileprotocol::AuthResponseState::USER_ALEADY_EXIT)
            {
                ss.clear();
                std::cout<<"register error,user aleady exist, login(i) or register(r) or cancel(c)?  ";
                std::cin>>ss;
                if(ss == "c")
                {
                    over = false;
                    break;
                }
            }
            else if(state == fileprotocol::AuthResponseState::UNKNOW_ERROR)
            {
                over = false;
                std::cout<<"something unkown error."<<std::endl;
                break;
            }
        }
        return over;
    }
    bool FileClient::quit_proactively()
    {
        // 发送CLOSE请求
        fileprotocol::MsgHeader send_header;
        char send_header_bytes[HEADER_LEN];
        send_header.set_magic(MY_PROTOCOL_MAGIC);
	    send_header.set_version(MY_PROTOCOL_VERSION);
        send_header.set_type(fileprotocol::MsgType::CLOSE);
        send_header.set_length(0);
        send_header.SerializeToArray(send_header_bytes,sizeof(send_header_bytes));
		if(m_client_sock->send(send_header_bytes,sizeof(send_header_bytes),0)==-1) return false;
        return true;
    }
    void FileClient::showmenu()
    {
        std::cout << "please enter 1, 2, 3,or 4:\n"
            "1) browse                          2) upload\n"
            "3) download(Not supported)         4) quit\n";
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
        fileprotocol::MsgHeader send_header;
        char send_header_bytes[HEADER_LEN];
        send_header.set_magic(MY_PROTOCOL_MAGIC);
	    send_header.set_version(MY_PROTOCOL_VERSION);
        // 发送浏览请求
        send_header.set_type(fileprotocol::MsgType::BROWSE_REQUEST);
        send_header.set_length(0);
        send_header.SerializeToArray(send_header_bytes,sizeof(send_header_bytes));
		if(m_client_sock->send(send_header_bytes,sizeof(send_header_bytes),0)==-1) return -1;
        // 接收浏览响应
        fileprotocol::MsgHeader recv_header;
        fileprotocol::MsgBody recv_body;
		char recv_header_bytes[HEADER_LEN];
		m_client_sock->recv(recv_header_bytes,sizeof(recv_header_bytes),0);
		recv_header.ParseFromArray(recv_header_bytes,sizeof(recv_header_bytes));
		if(recv_header.magic()!=MY_PROTOCOL_MAGIC || recv_header.version()!=MY_PROTOCOL_VERSION)
		{
			std::cout<<"not my package."<<std::endl;
			return false;
		}
		uint32_t recv_body_size = recv_header.length();
		char recv_body_bytes[recv_body_size];
		m_client_sock->recv(recv_body_bytes,sizeof(recv_body_bytes),0);
		recv_body.ParseFromArray(recv_body_bytes,sizeof(recv_body_bytes));
        if(recv_body.browse_response().filenames().size()!=0)
        {
            std::cout<<"Files on the server:"<<std::endl;
		    for(auto filename:recv_body.browse_response().filenames())
		    {
			    std::cout<<filename<<std::endl;
		    }
        }
        else{
            std::cout<<"No files on the server."<<std::endl;
        }
		
        // 发送ack包告诉服务器已收到
		send_header.set_type(fileprotocol::MsgType::ACK);
		memset(send_header_bytes,0,sizeof(send_header_bytes));
		send_header.SerializeToArray(send_header_bytes,sizeof(send_header_bytes));
		if(m_client_sock->send(send_header_bytes,sizeof(send_header_bytes),0)==-1) return false;
        return true;
    }

    bool FileClient::upload_file()
    {
        char filepath[FILE_NAME_LEN] = CLIENT_FILE_PATH;
        std::string filename;
        while (true)
        {
            std::cout<<"please input upload file's name: ";
            std::cin>>filename;
            strcat(filepath,filename.c_str());
            if(access(filepath,F_OK)==-1)
            {
                std::cout<<"file is not exist."<<std::endl;
                continue;
            }
            else
            {
                break;
            }
            strcpy(filepath,CLIENT_FILE_PATH);
        }
        // std::cout<<"upload file:"<<filepath<<std::endl;
        return m_file_op->send_file_client(filepath,m_client_sock);
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