#include <iostream>
#include "file_server.h"

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
    bool FileServer::send_optional_files(Socket::socket_ptr connected_sock)
    {
        std::vector<std::string> files;
        std::string path = "server_file";
        
        DIR* pdir;
	    dirent* ptr;
	    if(!(pdir = opendir(path.c_str()))) return false;
	    while((ptr = readdir(pdir))!=0)
	    {
		    if(strcmp(ptr->d_name,".")!=0 && strcmp(ptr->d_name,"..")!=0)
		    {
			    files.push_back(path+"/"+ptr->d_name);
		    }
	    }
	    closedir(pdir);

        char filenames[500] = {0};
        for(const auto &i:files)
        {
            strcat(filenames,i.c_str());
            strcat(filenames,";");
        }
        char flag[4];
        int rt = connected_sock->send(filenames,sizeof(filenames),0);
        if(rt==-1) return false;
        connected_sock->recv(flag,4,0);
        return true;
    }

    bool FileServer::send_file_context(Socket::socket_ptr connected_sock)
    {
        char flag[4]={0};
        FileHeader re_filectx;   
        char re_file[FILE_NAME_LEN] = OPTIONAL_PATH;
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
            strcpy(re_file,OPTIONAL_PATH);
            memset(tmp,0,sizeof(tmp));
        }
        

        std::cout<<"client requests to download file:"<<re_file<<std::endl;
        std::string path="./server_file/";
        bool re = m_fileop.send_file_server(path.c_str(),re_filectx,connected_sock);

        return re;
    }
    bool FileServer::recv_file_context(Socket::socket_ptr connected_sock)
    {
        FileHeader filectx;

        return m_fileop.recv_file_server(OPTIONAL_PATH,filectx,connected_sock);
    }

    void FileServer::handle_client(Socket::socket_ptr client)
    {
        // time_t curtime;
        // time(&curtime);
        // std::cout<<"task start in "<<ctime(&curtime)<<std::endl;
        printf("handling client...\n");
        char status[4] = {0};
        int choice = 0;
        m_fileop.get_all_files_md5(OPTIONAL_PATH);

        while(true)
        {
            
            while(true)
            {
                client->recv(status,sizeof(status),0);
                int rt = client->send("yes",4,0);
                if(rt==-1) break;
                choice = atoi(status);
                if(choice!=0) break;
            }
            
            std::cout<<"recv status from client:"<<choice<<std::endl;
            if(choice==1)
            {
                bool rt = send_optional_files(client);
                if(!rt) break;
            }
            else if(choice==2)
            {
                bool rt = recv_file_context(client);
                if(!rt) break;
            }
            else if(choice==3)
            {
                bool rt = send_file_context(client);
                if(!rt) break;
            }
            else if(choice==4) break;
            else continue;
            client->clear_recv_buffer();
        }
        printf("client stop.\n");
        client->close();
    }
}