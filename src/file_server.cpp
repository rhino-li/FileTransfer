#include <iostream>
#include "../include/file_server.h"

namespace filetrans
{
    bool FileServer::server_listen(IPAddress::ipaddr_ptr addr)
    {
        Socket::socket_ptr sock = Socket::create_tcp_socket();
        bool rt;
        rt = sock->bind(addr);
        if(!rt)
        {
            return false;
        }
        rt = sock->listen();
        if(!rt)
        {
            return false;
        }
        m_socks.push_back(sock);
        return true;
    }
    void FileServer::start_server(IPAddress::ipaddr_ptr addr)
    {
        if(!is_stop)
        {
            printf("Warnning: the server is running.\n");
            return;
        }
        is_stop = false; 
        
        int rt = server_listen(addr);
        if(!rt)
        {
            printf("server start error.\n");
            exit(2);
        }

        std::cout<<"server starting..."<<std::endl;

        while (true)
        {
            for(auto sock:m_socks)
            {
                Socket::socket_ptr client = sock->accept();
                if(client)
                {
                    Task* task = new Task(std::bind(&FileServer::handle_client,this,client));
                    m_pool->add_task(task);
                    // handle_client(client);
                }
            }
        }
        server_stop();
    }

    void FileServer::server_stop()
    {
        if(is_stop) return;
        for(auto sock:m_socks)
        {
            sock->close();
        }
        is_stop = true;
    }
    void getFileNames(std::string path, std::vector<std::string>& files)
    {
	    DIR* pdir;
	    dirent* ptr;
	    if(!(pdir = opendir(path.c_str()))) return;
	    while((ptr = readdir(pdir))!=0)
	    {
		    if(strcmp(ptr->d_name,".")!=0 && strcmp(ptr->d_name,"..")!=0)
		    {
			    files.push_back(path+"/"+ptr->d_name);
		    }
	    }
	    closedir(pdir);
    }
    bool FileServer::send_optional_files(std::string optional_path,Socket::socket_ptr connected_sock)
    {
        std::vector<std::string> fileNames;
        getFileNames(optional_path,fileNames);
        fileprotocol::MsgHeader send_header;
        char send_header_bytes[HEADER_LEN];
        send_header.set_magic(MY_PROTOCOL_MAGIC);
	    send_header.set_version(MY_PROTOCOL_VERSION);
        // 发送浏览响应
        fileprotocol::MsgBody send_body;
        send_header.set_type(fileprotocol::MsgType::BROWSE_RESPONSE);
		fileprotocol::BrowseResponse* browse_response = send_body.mutable_browse_response();
      	for (const auto &ph : fileNames) 
		{
		    browse_response->add_filenames(ph);
	    }
		uint32_t send_body_size = send_body.ByteSize();
		char send_body_bytes[send_body_size];
		send_body.SerializeToArray(send_body_bytes,send_body_size);
		send_header.set_length(send_body_size);
		send_header.SerializeToArray(send_header_bytes,sizeof(send_header_bytes));
		connected_sock->send(send_header_bytes,sizeof(send_header_bytes),0);
      	connected_sock->send(send_body_bytes,send_body_size,0);
		// 接收ack确认
        fileprotocol::MsgHeader recv_header;
        char recv_header_bytes[HEADER_LEN];
		connected_sock->recv(recv_header_bytes,sizeof(recv_header_bytes),0);
		recv_header.ParseFromArray(recv_header_bytes,sizeof(recv_header_bytes));
		if(recv_header.type()==fileprotocol::MsgType::ACK) return true;
		else return false;
    }

    bool FileServer::send_file_context(std::string optional_path,Socket::socket_ptr connected_sock)
    {
        char flag[4]={0};
        FileHeader re_filectx;   
        char re_file[FILE_NAME_LEN] = {0};
        strcpy(re_file,optional_path.c_str());
        int rt = 1;
    
        while(true) // 服务器上不存在此文件
        {
            char tmp[FILE_NAME_LEN] = {0};
            connected_sock->recv(tmp,sizeof(tmp),0);
            strcat(re_file,tmp);
            if(access(re_file,F_OK)==-1)
            {
                rt = connected_sock->send("noo",4,0);
                if(rt==-1) return false;
            }
            else{
                strcpy(re_filectx.filename,tmp);
                rt = connected_sock->send("yes",4,0);
                if(rt==-1) return false;
                sleep(1);
                break;
            }
            strcpy(re_file,optional_path.c_str());
            memset(tmp,0,sizeof(tmp));
        }

        std::cout<<"client requests to download file:"<<re_file<<std::endl;
        bool re = m_fileop.send_file_server(optional_path.c_str(),re_filectx,connected_sock);
        return re;
    }
    bool FileServer::recv_file_context(std::string optional_path,fileprotocol::MsgHeader recv_header,Socket::socket_ptr connected_sock)
    {
        std::string filepath = optional_path;
        return m_fileop.recv_file_server(recv_header,filepath,connected_sock);
    }
    
    bool is_folder_exist(const char* path)
    {
        DIR *dp;
        if ((dp = opendir(path)) == NULL)
        {
            return false;
        }
        closedir(dp);
        return true;
    }
    /**
     * 本函数的返回值，标识的是身份验证的通过与否
     */
    bool FileServer::user_authentication(std::string& optional_path,Socket::socket_ptr connected_sock)
    {
        bool over = true; 
        int times = 3; // 一共三次登录机会
        fileprotocol::MsgHeader recv_login_header;
        fileprotocol::MsgBody recv_login_body;
        fileprotocol::AuthRequest* auth_request = nullptr;
        while (times > 0)
        {
            /**
             * 此flag标识的是身份验证业务的处理完成与否，如果未完成，一直进行身份验证业务的处理
             * 身份验证业务包含登录/注册/注销用户
             * 也就是说，只有在登录成功/注册成功/注销用户成功时，flag为true，结束业务处理，退出循环
             * 而只有在登录成功/注册成功时，此函数才返回true，注销用户成功时函数返回false
             */
            bool flag = false;
            // 接收用户身份验证信息
            recv_login_header.Clear();
            recv_login_body.Clear();
            char recv_login_header_bytes[HEADER_LEN];
            connected_sock->recv(recv_login_header_bytes,sizeof(recv_login_header_bytes),0);
            recv_login_header.ParseFromArray(recv_login_header_bytes,sizeof(recv_login_header_bytes));
            if(recv_login_header.type() != fileprotocol::MsgType::AUTH_REQUEST 
                || recv_login_header.magic() != MY_PROTOCOL_MAGIC || recv_login_header.version() != MY_PROTOCOL_VERSION)
                {
                    std::cout<<"something error."<<std::endl;
                    return false;
                }
            uint32_t recv_login_body_size = recv_login_header.length();
            char recv_login_body_bytes[recv_login_body_size];
            connected_sock->recv(recv_login_body_bytes,recv_login_body_size,0);
            recv_login_body.ParseFromArray(recv_login_body_bytes,recv_login_body_size);
            auth_request = recv_login_body.mutable_auth_request();
            fileprotocol::AuthResponseState auth_response_state;
            if(auth_request->auth_request_state() == fileprotocol::AuthRequestState::LOGIN)
            {
                flag = user_login(auth_request,auth_response_state);
                times--;
                if(times == 0 && flag==false) over = false; // 第三次也登录失败
            }
            else if(auth_request->auth_request_state() == fileprotocol::AuthRequestState::REGISTER)
            {
                flag = user_register(auth_request,auth_response_state);
            }
            else if(auth_request->auth_request_state() == fileprotocol::AuthRequestState::LOGINOUT)
            {
                flag = user_logout(auth_request,auth_response_state);
                over = false;
            }
            // 发送响应信息
            fileprotocol::MsgHeader auth_response_header;
            fileprotocol::MsgBody auth_response_body;
            char auth_response_header_bytes[HEADER_LEN];
            auth_response_header.set_type(fileprotocol::MsgType::AUTH_RESPONSE);
            fileprotocol::AuthResponse* auth_response = auth_response_body.mutable_auth_response();
            auth_response->set_auth_response_state(auth_response_state);
            uint32_t auth_response_body_size = auth_response_body.ByteSize();
            auth_response_header.set_length(auth_response_body_size);
            auth_response_header.SerializeToArray(auth_response_header_bytes,sizeof(auth_response_header_bytes));
            char auth_response_body_bytes[auth_response_body_size];
            auth_response_body.SerializeToArray(auth_response_body_bytes,auth_response_body_size);
            if(connected_sock->send(auth_response_header_bytes,sizeof(auth_response_header_bytes),0)==-1) return false;
            if(connected_sock->send(auth_response_body_bytes,auth_response_body_size,0)==-1) return false;
            if(flag == true) break;
        }
        if(over)
        {
            std::string username = auth_request->username(); 
            char dirpath[50] = "./server_file/";
            strcat(dirpath,username.c_str());
            strcat(dirpath,"/");
            if(!is_folder_exist(dirpath)) // 目录不存在-->新注册的账号-->创建目录
            {
                if(mkdir(dirpath, 0755) == -1) 
                {
                    std::cerr << "user dir create error." << std::endl;
                    exit(1);
                }
            }
            optional_path = dirpath;
        }
        return over;
    }
    bool FileServer::user_login(fileprotocol::AuthRequest* auth_request,fileprotocol::AuthResponseState& auth_response_state)
    {
        std::string username = auth_request->username(); 
        std::string passwd = auth_request->passwd();
        User usr = query_user_info(username);
        if(usr.get_name() == "")
        {
            auth_response_state = fileprotocol::AuthResponseState::USER_NOT_EXIT;
            return false;
        }
        if(usr.get_pwd() == passwd)
        {
            auth_response_state = fileprotocol::AuthResponseState::LOGIN_SUCCESS;
            return true;
        }
        else
        {
            auth_response_state = fileprotocol::AuthResponseState::PASSWD_ERROR;
            return false;
        }
    }
    bool FileServer::user_register(fileprotocol::AuthRequest* auth_request,fileprotocol::AuthResponseState& auth_response_state)
    {
        std::string username = auth_request->username(); 
        std::string passwd = auth_request->passwd();
        User usr = query_user_info(username);
        if(usr.get_name() != "") 
        {
            auth_response_state = fileprotocol::AuthResponseState::USER_ALEADY_EXIT;
            return false;
        }
        if(insert_user(usr))
        {
            auth_response_state = fileprotocol::AuthResponseState::REGISTER_SUCCESS;
            return true;
        }
        else
        {
            auth_response_state = fileprotocol::AuthResponseState::UNKNOW_ERROR;
            return false;
        }
    }
    bool FileServer::user_logout(fileprotocol::AuthRequest* auth_request,fileprotocol::AuthResponseState& auth_response_state)
    {
        std::string username = auth_request->username(); 
        std::string passwd = auth_request->passwd();
        User usr = query_user_info(username);
        if(usr.get_name() == "") 
        {
            auth_response_state = fileprotocol::AuthResponseState::USER_NOT_EXIT;
            return true;
        }
        if(delete_user(usr))
        {
            auth_response_state = fileprotocol::AuthResponseState::LOGOUT_SUCCESS;
            char dirpath[50] = "./server_file/";
            strcat(dirpath,username.c_str());
            strcat(dirpath,"/");
            filetrans::FileOperator::delete_folder(dirpath);
            return true;
        }
        else
        {
            auth_response_state = fileprotocol::AuthResponseState::UNKNOW_ERROR;
            return false;
        }
    }

    void FileServer::handle_client(Socket::socket_ptr client)
    {
        std::string optional_path;
        bool flag = user_authentication(optional_path,client);
        if(flag)
        {
            printf("handling client...\n");
            m_fileop.get_all_files_md5(optional_path);
        }

        while(flag)
        {
            fileprotocol::MsgHeader recv_header;
			char recv_header_bytes[HEADER_LEN];
            while(true)
            {
                client->recv(recv_header_bytes,sizeof(recv_header_bytes),0);
			    recv_header.ParseFromArray(recv_header_bytes,sizeof(recv_header_bytes));
			    if(recv_header.magic()==MY_PROTOCOL_MAGIC || recv_header.version()==MY_PROTOCOL_VERSION) break;
            }
			
            fileprotocol::MsgType type = recv_header.type();
            std::cout<<"recv status from client:"<<type<<std::endl;
            if(type==fileprotocol::MsgType::BROWSE_REQUEST)
            {
                bool rt = send_optional_files(optional_path,client);
                if(!rt) break;
            }
            else if(type==fileprotocol::MsgType::FILE_UPLOAD_REQUEST)
            {
                bool rt = recv_file_context(optional_path,recv_header,client);
                if(!rt) break;
            }
            else if(type==fileprotocol::MsgType::FILE_DOWNLOAD_REQUEST)
            {
                bool rt = send_file_context(optional_path,client);
                if(!rt) break;
            }
            else if(type==fileprotocol::MsgType::CLOSE) break;
            else continue;
            client->clear_recv_buffer();
        }
        printf("client stop.\n");
        client->close();
    }
}