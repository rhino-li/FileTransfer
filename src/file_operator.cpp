#include <atomic>
#include <stdio.h>
#include <cmath>
#include <istream>
#include <sstream>
#include <iostream>

#include "../include/file_operator.h"

namespace filetrans
{
    std::atomic<bool> pause_send_signal(false);
    std::atomic<bool> pause_recv_signal(false);

    std::vector<std::string> split(std::string str, char del) 
    {
	    std::stringstream ss(str);
	    std::string temp;
	    std::vector<std::string> ret;
	    while (getline(ss, temp, del)) {
		    ret.push_back(temp);
	    }
	    return ret;
    }

    FileOperator::FileOperator()
    {
        m_complete_files_hash.clear();
        m_incomplete_files.clear();
    }

    void FileOperator::get_all_files_md5(const std::string dir)
    {
        std::vector<std::string> files;
        DIR* pdir;
	    dirent* ptr;
	    if(!(pdir = opendir(dir.c_str()))) return;
	    while((ptr = readdir(pdir))!=0)
	    {
		    if(strcmp(ptr->d_name,".")!=0 && strcmp(ptr->d_name,"..")!=0)
		    {
			    files.push_back(dir+ptr->d_name);
		    }
	    }
	    closedir(pdir);

        for(auto &i:files)
        {
            char filepath[FILE_NAME_LEN] = {0};
            strcat(filepath,i.c_str());
            std::string imd5 = get_file_md5(filepath);
            m_rw_mutex.wrlock();
            m_complete_files_hash.insert(imd5);
            m_rw_mutex.unlock();
        }
    }

    std::string FileOperator::get_file_md5(std::string filepath)
    {
        char cwd[100];
        getcwd(cwd,sizeof(cwd));
        chdir(cwd);
        char cmd[100] = "md5sum ";
        strcat(cmd,filepath.c_str());
        FILE* pipe = popen(cmd,"r");
        if(!pipe)
        {
            printf("popen error.\n");
            exit(2);
        }
        std::string result;
        char buffer[200];
        fgets(buffer,sizeof(buffer),pipe);
        result = split(buffer,' ')[0];
        // std::cout<<result<<std::endl;
        return result;
    }
    bool FileOperator::delete_folder(const char* path)
    {
        DIR *dir = opendir(path);
        if(dir == nullptr) return false;
        dirent *entry;
        while ((entry = readdir(dir)) != nullptr) 
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            std::string fullPath = std::string(path) + "/" + entry->d_name;
            if (entry->d_type == DT_DIR) 
            {
                if (!delete_folder(fullPath.c_str())) 
                {
                    closedir(dir);
                    return false;
                }
            } 
            else
            {
                if (remove(fullPath.c_str()) != 0) 
                {
                    closedir(dir);
                    return false;
                }
            }
        }
        closedir(dir);
        if (rmdir(path) != 0) return false;
        return true;
    }

    void* monitor_send_signal(void* arg)
    {
        std::string input;
        while (true)
        {
            getline(std::cin,input);
            if(input == "p")
            {
                pause_send_signal = true;
                break;
            }
            sleep(1);
        }
        return nullptr;
    }

    void* monitor_recv_signal(void* arg)
    {
        std::string input;
        while (true)
        {
            getline(std::cin,input);
            if(input == "p")
            {
                pause_recv_signal = true;
                break;
            }
            sleep(1);
        }
        return nullptr;
    }

    int FileOperator::splice_send_client(FILE *fp,int start_index,uint32_t total_chunk, fileprotocol::MsgBody& send_body,Socket::socket_ptr sock)
    {
        sock->clear_recv_buffer();
        int send_size = 0;
        printf("(you can enter 'p' to pause sending)\n");
        pthread_t send_pause;
        pthread_create(&send_pause,NULL,monitor_send_signal,NULL);
        pthread_detach(send_pause);

        send_body.clear_body();
        fileprotocol::FileTransfer* file_transfer = send_body.mutable_file_transfer();
        file_transfer->set_status(fileprotocol::Status::TRANSFERRING);
        fseek(fp,start_index*CHUNK_SIZE,SEEK_SET);
        file_transfer->set_chunk_index(start_index);
        pause_send_signal = false;     

        fileprotocol::MsgHeader send_header;
        send_header.set_magic(MY_PROTOCOL_MAGIC);
        send_header.set_version(MY_PROTOCOL_VERSION);
        send_header.set_type(fileprotocol::MsgType::FILE_TRANSFER);
        char send_header_bytes[HEADER_LEN];

        char buffer[CHUNK_SIZE] = {0};
        uint64_t size = 0;
        uint32_t i = 0;
        while(file_transfer->chunk_index() < total_chunk && !pause_send_signal) // 分片发送
        {
            size = fread(buffer,1,sizeof(buffer),fp);
            file_transfer->set_data_len(size);
            file_transfer->set_data(buffer,size);
            if(file_transfer->chunk_index() == total_chunk-1)  // 最后一片
            {
                file_transfer->set_status(fileprotocol::Status::DONE);
            }
            uint32_t send_body_size = send_body.ByteSize();
            char* send_body_bytes = new char[send_body_size];
            send_header.set_length(send_body_size);
            send_header.SerializeToArray(send_header_bytes,sizeof(send_header_bytes));
            send_body.SerializeToArray(send_body_bytes,send_body_size);
            if(sock->send(send_header_bytes,sizeof(send_header_bytes),0)==-1) return -1;
            if(sock->send(send_body_bytes,send_body_size,0)==-1) return -1;
            
            send_size += size;
            // 接收响应
            fileprotocol::MsgHeader recv_header;
            char recv_header_bytes[HEADER_LEN];
            sock->recv(recv_header_bytes,sizeof(recv_header_bytes),0); // 暂停后这里的res_info.status没有改
            recv_header.ParseFromArray(recv_header_bytes,sizeof(recv_header_bytes));
            if(recv_header.type() != fileprotocol::MsgType::STATUS_UPDATE)
            {
                std::cout<<"something error."<<std::endl;
                break;
            }
            fileprotocol::MsgBody recv_body;
            uint32_t recv_body_size = recv_header.length();
            char recv_body_bytes[recv_body_size];
            sock->recv(recv_body_bytes,sizeof(recv_body_bytes),0);
            recv_body.ParseFromArray(recv_body_bytes,sizeof(recv_body_bytes));
            fileprotocol::StatusUpdate file_status = recv_body.status_update();
            if(file_status.status() != fileprotocol::Status::TRANSFERRING)
            {
                std::cout<<"something error."<<std::endl;
                break;
            }

            file_transfer->set_chunk_index(file_status.need_chunk_index());
            i=file_transfer->chunk_index();
            memset(buffer,0,sizeof(buffer));
            memset(send_header_bytes,0,sizeof(send_header_bytes));
            if(i <= total_chunk)
            {
                printf("\rsending[%.0lf%%]",i*100.0/(total_chunk));
            }
        }
        
        pthread_cancel(send_pause);
        
        if(pause_send_signal) // 暂停发送 
        {
            send_header.clear_type();
            send_header.set_type(fileprotocol::MsgType::STATUS_UPDATE);
            send_header.clear_length();
            send_body.clear_body();
            fileprotocol::StatusUpdate* status_update = send_body.mutable_status_update();
            status_update->set_status(fileprotocol::Status::PAUSE);
            status_update->set_need_chunk_index(i);
            uint32_t send_status_size = send_body.ByteSize();
            char send_status_bytes[send_status_size];
            send_header.set_length(send_status_size);
            memset(send_header_bytes,0,sizeof(send_header_bytes));
            send_header.SerializeToArray(send_header_bytes,sizeof(send_header_bytes)); 
            send_body.SerializeToArray(send_status_bytes,sizeof(send_status_bytes));
            if(sock->send(send_header_bytes,sizeof(send_header_bytes),0)==-1) return -1;
            if(sock->send(send_status_bytes,sizeof(send_status_bytes),0)==-1) return -1;
            // 接收ACK
            fileprotocol::MsgHeader recv_header;
            char recv_header_bytes[HEADER_LEN];
			sock->recv(recv_header_bytes,sizeof(recv_header_bytes),0);
			recv_header.ParseFromArray(recv_header_bytes,sizeof(recv_header_bytes));
			if(recv_header.type() != fileprotocol::MsgType::ACK)
            {
                std::cout<<"something error."<<std::endl;
                return -1;
            }
            // std::cout<<"needchunkindex:"<<status_update->need_chunk_index()<<std::endl;
        }
        std::cout<<std::endl;
        return send_size;
    }
    bool FileOperator::recv_ack(Socket::socket_ptr sock)
    {
        fileprotocol::MsgHeader recv_ack_header;
        char recv_ack_header_bytes[HEADER_LEN];
		sock->recv(recv_ack_header_bytes,sizeof(recv_ack_header_bytes),0);
		recv_ack_header.ParseFromArray(recv_ack_header_bytes,sizeof(recv_ack_header_bytes));
		if(recv_ack_header.type() != fileprotocol::MsgType::ACK)
        {
            std::cout<<"something error."<<std::endl;
            return false;
        }
        return true;
    }
    bool FileOperator::send_update_status_request(fileprotocol::Status latest,Socket::socket_ptr sock)
    {
        fileprotocol::MsgHeader send_update_header;
        send_update_header.set_magic(MY_PROTOCOL_MAGIC);
        send_update_header.set_version(MY_PROTOCOL_VERSION);
        send_update_header.set_type(fileprotocol::MsgType::STATUS_UPDATE);
        char send_update_header_bytes[HEADER_LEN];

        fileprotocol::MsgBody send_update_body;
        fileprotocol::StatusUpdate* status_update = send_update_body.mutable_status_update();
        status_update->set_status(latest);
        uint32_t send_update_body_size = send_update_body.ByteSize();
        send_update_header.set_length(send_update_body_size);
        send_update_header.SerializeToArray(send_update_header_bytes,sizeof(send_update_header_bytes));
        char send_update_body_bytes[send_update_body_size];
        send_update_body.SerializeToArray(send_update_body_bytes,sizeof(send_update_body_bytes));
        if(sock->send(send_update_header_bytes,sizeof(send_update_header_bytes),0)==-1) return -1;
        if(sock->send(send_update_body_bytes,sizeof(send_update_body_bytes),0)==-1) return -1;
        // 接收ack
        if(!recv_ack(sock)) return false;
        return true;
    }
    bool FileOperator::send_file_client(const char* filepath, Socket::socket_ptr sock)
    {
        fileprotocol::MsgHeader send_header;
        send_header.set_magic(MY_PROTOCOL_MAGIC);
        send_header.set_version(MY_PROTOCOL_VERSION);
        fileprotocol::MsgBody send_body;
        // 发送上传请求+文件摘要
		fileprotocol::FileSummary* file_summary = send_body.mutable_file_summary();
        FILE *fp = fopen(filepath,"rb");
        fseek(fp,0,SEEK_END);
        int totalSize = ftell(fp);
        fseek(fp,0,SEEK_SET);
        std::string filename = split(filepath,'/').back();
        send_header.set_type(fileprotocol::MsgType::FILE_UPLOAD_REQUEST);
    	file_summary->set_filename(filename);
		file_summary->set_format(filename.substr(filename.find_last_of('.') + 1));
		file_summary->set_filehash(get_file_md5(filepath));
		file_summary->set_filesize(totalSize);
        file_summary->set_total_chunk(ceil((double)totalSize/CHUNK_SIZE));
        uint32_t send_body_size = send_body.ByteSize();
		send_header.set_length(send_body_size);
		char* send_body_bytes = new char[send_body_size];
		send_body.SerializeToArray(send_body_bytes,send_body_size);
		char send_header_bytes[HEADER_LEN];
		send_header.SerializeToArray(send_header_bytes,sizeof(send_header_bytes));
		if(sock->send(send_header_bytes,sizeof(send_header_bytes),0)==-1) return false;
      	if(sock->send(send_body_bytes,send_body_size,0)==-1) return false;
		delete send_body_bytes;
        // 接收服务器上目标文件的最新状态
		fileprotocol::MsgHeader recv_header;
		char recv_header_bytes[HEADER_LEN];
		sock->recv(recv_header_bytes,sizeof(recv_header_bytes),0);
		recv_header.ParseFromArray(recv_header_bytes,sizeof(recv_header_bytes));
		if(recv_header.type() != fileprotocol::MsgType::STATUS_UPDATE)
		{
			std::cout<<"server maybe refused."<<std::endl;
			return false;
		}
        fileprotocol::MsgBody recv_body;
        uint32_t recv_body_size = recv_header.length();
        char recv_body_bytes[recv_body_size];
		sock->recv(recv_body_bytes,sizeof(recv_body_bytes),0);
        recv_body.ParseFromArray(recv_body_bytes,sizeof(recv_body_bytes));
        fileprotocol::StatusUpdate file_status = recv_body.status_update();
        if(file_status.status() == fileprotocol::Status::DONE)
        {
            printf("file sent success.\n");
            return true;
        }
        int send_total_size = 0;
        int start_chunk = 0;
        if(file_status.status() == fileprotocol::Status::PAUSE) // 断点续传
        {
            start_chunk = file_status.need_chunk_index();
            send_total_size += start_chunk * CHUNK_SIZE;
        }
        else // 新文件
        {
            start_chunk = 0;
        }
        // 发送文件内容
		uint32_t total_chunk = file_summary->total_chunk();
		std::string filehash = file_summary->filehash();
		send_header.clear_type();
		send_header.set_type(fileprotocol::MsgType::FILE_TRANSFER);
		send_body.clear_body();
		fileprotocol::FileTransfer* file_transfer = send_body.mutable_file_transfer();
		file_transfer->set_status(fileprotocol::Status::TRANSFERRING);
		file_transfer->set_filehash(filehash);
        while (true)
        {
            size_t tmp = splice_send_client(fp,start_chunk,total_chunk,send_body,sock);
            if(tmp==-1) return false;
            send_total_size += tmp;
            if(send_body.has_file_transfer()) // 发完了
            {
                break;
            }
            // send_body.has_status_update() == true
            fileprotocol::StatusUpdate* status_update = send_body.mutable_status_update();
            std::string signal;
            printf("you want to end(e) or continue(c)? ");
            while(1)
            {
                std::cin>>signal;
                if(signal!="c"&&signal!="e")
                {
                    std::cout<<"c or e please. "<<std::endl;
                }
                else{
                    break;
                }
            }
            if(signal == "c") // 继续
            {
                start_chunk = status_update->need_chunk_index();
                send_update_status_request(fileprotocol::Status::TRANSFERRING,sock);
            }
            else if(signal == "e") // 终止发送
            {
                send_update_status_request(fileprotocol::Status::END,sock);
                break;
            }
        }

        printf("sent %f KB.\n",send_total_size/(1024.0));
        fclose(fp);
        printf("Done.\n");
        return true;
    }

    int FileOperator::splice_recv_client(FileCTX& filecontext,ResponseInfo& res_info,Socket::socket_ptr sock)
    {
        SendInfo send_info;
        int recv_total_size = 0;
        char flag[4];
            
        printf("(you can enter 'p' to pause recving)\n");
        pthread_t recv_pause;
        pthread_create(&recv_pause,NULL,monitor_recv_signal,NULL);
        pthread_detach(recv_pause);
        pause_recv_signal = false;
        
        long long i=0;
            
        while(!pause_recv_signal)
        {
            char buffer[CHUNK_SIZE]={0};
            int header_len = sock->recv((char*)&send_info,sizeof(send_info),0);
            // int rt = sock->send("yes",4,0);
            // if(rt==-1) return false;
            // res_info.status = send_info.status;
            res_info.status = TRANSFERRING;
            if(send_info.status == END)  // 不携带数据了
            {
                res_info.status = END;
                break;
            }
            
            size_t data_len = send_info.data_len;
            int size=0;
            while(size < data_len)
            {
                int remain_len = data_len-size;
                size += sock->recv(buffer+size,remain_len,0);
            }
            recv_total_size += size;
            
            res_info.need_chunk_index = send_info.chunk_index+1;
            int r = sock->send(&res_info,sizeof(res_info),0);
            if(r==-1) return -1;
            sock->recv(&flag,4,0);
            filecontext.status = res_info.status;
            filecontext.total_chunk = send_info.total_chunk;
            if(filecontext.chunks.size()==0)
            {
                filecontext.chunks.resize(filecontext.total_chunk);
            }
            size_t cur = send_info.chunk_index;
            filecontext.has_been_uploaded.insert(cur);
            char *tmp = new char[CHUNK_SIZE];
            memcpy(tmp,buffer,sizeof(buffer));
            filecontext.chunks[cur] = std::make_pair(data_len,tmp);
            i=send_info.chunk_index;
            if(i<=filecontext.total_chunk-1)
            {
                printf("\rrecving[%.0lf%%]",i*100.0/(filecontext.total_chunk-1));
            }
                
            if(send_info.status == DONE)
            {
                res_info.status = DONE;
                filecontext.status = res_info.status;
                break;
            }
            
        }
        pthread_cancel(recv_pause);

        if(pause_recv_signal)
        {
            res_info.status = PAUSE;
            int r = sock->send(&res_info,sizeof(res_info),0);
            if(r==-1) return -1;
            sock->recv(&flag,4,0);
        }
        sock->clear_recv_buffer();

        if(res_info.status == PAUSE)
        {
            filecontext.need_chunk_index = send_info.chunk_index+1;
            res_info.need_chunk_index = send_info.chunk_index+1;
            m_rw_mutex.wrlock();
            m_incomplete_files[filecontext.filehash] = filecontext;
            m_rw_mutex.unlock();
            std::cout<<std::endl;
            return recv_total_size-send_info.data_len;
        }

        if(send_info.status == END) // 取消发送
        {
            m_rw_mutex.wrlock();
            m_incomplete_files.erase(res_info.filehash); 
            m_rw_mutex.unlock();
            std::cout<<"cancel the recv."<<std::endl;
            return -1;
        }
        return recv_total_size;
    }

    bool FileOperator::recv_file_client(char path[FILE_NAME_LEN], FileHeader filectx, Socket::socket_ptr sock)
    {
        sock->recv((char*)&filectx,sizeof(filectx),0);
        ResponseInfo res_info;
        strcpy(res_info.filehash,filectx.filehash);
        m_rw_mutex.rdlock();
        int cnt = m_complete_files_hash.count(filectx.filehash);
        m_rw_mutex.unlock();
        if(cnt!=0) // 本机有完整版（妙传）
        {
            res_info.status = DONE;
            int rt = sock->send((char*)&res_info,sizeof(res_info),0);
            if(rt==-1) return false;
            printf("\rrecving[100%].");
            printf("\nsuccess! received %f KB.\n",filectx.filesize/(1024.0));
            return true;
        }
        
        // 发送响应信息
        size_t recv_total_size = 0;
        FileCTX filecontext = {};
        if(cnt!=0) // 本机有分片（续传）
        {
            filecontext = m_incomplete_files[filectx.filehash];
            res_info.status = filecontext.status;
            res_info.need_chunk_index = filecontext.need_chunk_index;
            recv_total_size += res_info.need_chunk_index * CHUNK_SIZE;
        }
        else // 新文件
        {
            filecontext.filehash = filectx.filehash;
            filecontext.has_been_uploaded.clear();
            filecontext.chunks.clear();
            res_info.status = filecontext.status =TRANSFERRING;
            res_info.need_chunk_index = filecontext.need_chunk_index = 0;
        }
        int rt = sock->send((char*)&res_info,sizeof(res_info),0);
        if(rt==-1) return false;

        char flag[4]={0};
        while(true)
        {
            int recv_size = splice_recv_client(filecontext,res_info,sock);
            if(recv_size == -1) return false;
            recv_total_size += recv_size;

            if(res_info.status==DONE) break;
            std::string signal;
            printf("you want to end(e) or continue(c)? ");
            while(1)
            {
                std::cin>>signal;
                if(signal!="c"&&signal!="e")
                {
                    std::cout<<"c or e please. "<<std::endl;
                }
                else{
                    break;
                }
            }
            if(signal == "c") // 继续
            {
                res_info.status = TRANSFERRING;
                int rt = sock->send((char*)&res_info,sizeof(res_info),0);
                if(rt==-1) return false;
                sock->recv(&flag,4,0);
            }
            else if(signal == "e") // 终止发送
            {
                res_info.status = END;
                int rt = sock->send((char*)&res_info,sizeof(res_info),0);
                if(rt==-1) return false;
                sock->recv(&flag,4,0);
                break;
            }
        }
        // 分片收齐
        if(filecontext.has_been_uploaded.size() == filecontext.total_chunk) 
        {
            char filepath[FILE_NAME_LEN];
            strcpy(filepath,path);
            strcat(filepath,filectx.filename);
            FILE *fp = fopen(filepath,"wb");
            if(fp==NULL)
            {
                printf("open file error.\n");
                return false;
            }
            auto chunks = filecontext.chunks;
            for(int i=0;i<chunks.size();i++)
            {
                fwrite(chunks[i].second,1,chunks[i].first,fp);
            }
            fclose(fp);
            if(get_file_md5(filepath)==res_info.filehash)
            {
                m_rw_mutex.wrlock();
                m_complete_files_hash.insert(filecontext.filehash);
                m_rw_mutex.unlock();
                printf("\nsuccess! received %f KB.\n",recv_total_size/(1024.0));
                return true;
            }
            else
            {
                printf("\nmd5 not equal.\n");
                return false;
            }
        }
    }
    
    int FileOperator::splice_send_server(FILE *fp,int start_index, SendInfo& send_info,Socket::socket_ptr sock)
    {
        sock->clear_recv_buffer();
        char flag[4];
        int send_size = 0;

        fseek(fp,start_index*CHUNK_SIZE,SEEK_SET);
        send_info.chunk_index = start_index;
        char buffer[CHUNK_SIZE];
        pause_send_signal = false;
        int rt = 0;
        int size = 0;
        ResponseInfo res_info;

        while(send_info.chunk_index < send_info.total_chunk) // 分片发送
        {
            size = fread(buffer,1,sizeof(buffer),fp);
            send_info.data_len = size;
            send_info.status = TRANSFERRING;
            if(send_info.chunk_index == send_info.total_chunk-1)  // 最后一片
            {
                send_info.status = DONE;
            }
            rt = sock->send((char*)&send_info,sizeof(send_info),0);
            if(rt==-1) return -1;
            rt = sock->send(buffer,sizeof(buffer),0);
            if(rt==-1) return -1;
            sock->recv(&res_info,sizeof(res_info),0);
            sock->send("yes",4,0);
            if(res_info.status==PAUSE || res_info.status==END)
            {
                send_info.status = res_info.status;
                break;
            }
            send_size += size;
            memset(buffer,0,sizeof(buffer));
            send_info.chunk_index++;
        }
        if(res_info.status==PAUSE || res_info.status==END) // 接收方没收到当前包
        {
            return send_size-size;
        }
        else // DONE
        {
            return send_size;
        }
        
    }
    
    bool FileOperator::send_file_server(const char path[FILE_NAME_LEN], FileHeader filectx, Socket::socket_ptr sock)
    {
        char flag[4];
        char filepath[FILE_NAME_LEN]={0};
        strcpy(filepath,path);
        strcat(filepath,filectx.filename);
        printf("sending %s \n",filepath);

        FILE *fp = fopen(filepath,"rb");
        fseek(fp,0,SEEK_END);
        filectx.filesize = ftell(fp);
        fseek(fp,0,SEEK_SET);

        SendInfo send_info;
        send_info.status = TRANSFERRING;
        strcpy(send_info.filehash,get_file_md5(filepath).c_str());
        send_info.total_chunk = ceil((double)filectx.filesize/CHUNK_SIZE);
        send_info.chunk_index = 0;

        strcpy(filectx.filehash, send_info.filehash);
        int rt = sock->send((char*)&filectx,sizeof(filectx),0);
        if(rt==-1) return false;
        
        ResponseInfo res_info;
        sock->recv(&res_info,sizeof(res_info),0);
        if(res_info.status == DONE)
        {
            printf("file sent success.\n");
            return true;
        }
        int send_total_size = 0;
        int start_chunk = 0;
        if(res_info.status == PAUSE) // 断点续传
        {
            start_chunk = res_info.need_chunk_index;
            send_total_size += start_chunk * CHUNK_SIZE;
        }
        else // 新文件
        {
            start_chunk = 0;
        }

        while (true)
        {
            size_t tmp = splice_send_server(fp,start_chunk,send_info,sock);
            if(tmp==-1) return false;
            send_total_size += tmp;
            if(send_info.status == DONE) // 发完了
            {
                break;
            }
            // 客户端暂停（必走的分支）
            if(send_info.status == PAUSE)
            {
                sock->recv(&res_info,sizeof(res_info),0);
                rt = sock->send("yes",4,0);
                if(rt==-1) return false;
                if(res_info.status==TRANSFERRING)
                {
                    std::cout<<"client continue recv."<<std::endl;
                    start_chunk = res_info.need_chunk_index;
                    continue;
                } 
                else if(res_info.status==END)
                {
                    std::cout<<"client cancel recv."<<std::endl;
                    return true;
                }
            }
        }

        printf("sent %f KB.\n",send_total_size/(1024.0));
        fclose(fp);
        printf("Done.\n");
        return true;
    }
    int FileOperator::splice_recv_server(FileCTX& filecontext, Socket::socket_ptr sock)
    {
        // sock->clear_recv_buffer();
        int recv_total_size = 0;
        
        fileprotocol::MsgBody send_body;
        fileprotocol::StatusUpdate* status_update = send_body.mutable_status_update();
        fileprotocol::MsgHeader send_header;
        send_header.set_magic(MY_PROTOCOL_MAGIC);
        send_header.set_version(MY_PROTOCOL_VERSION);
        send_header.set_type(fileprotocol::MsgType::STATUS_UPDATE);
        char send_header_bytes[HEADER_LEN];

        fileprotocol::MsgHeader recv_header;
        char recv_header_bytes[HEADER_LEN];

        uint32_t data_len = 0;
            
        while(true)
        {
            // 接收消息头
            sock->recv(recv_header_bytes,sizeof(recv_header_bytes),0);
            recv_header.ParseFromArray(recv_header_bytes,sizeof(recv_header_bytes));
            if(recv_header.type() != fileprotocol::MsgType::FILE_TRANSFER)
            {
                break;
            }
            // 接收消息体（文件数据）
            uint32_t length = recv_header.length();
            fileprotocol::MsgBody recv_body;
            char* file_transfer_body_bytes = new char[length];
            int size=0;
            while(size < length)
            {
                int remain_len = length-size;
                size += sock->recv(file_transfer_body_bytes+size,remain_len,0);;
            }
            recv_body.ParseFromArray(file_transfer_body_bytes,length);
            fileprotocol::FileTransfer* file_transfer = recv_body.mutable_file_transfer();
            // 保存文件数据
            data_len = file_transfer->data_len();
            recv_total_size += data_len;
            size_t cur = file_transfer->chunk_index();
            filecontext.status = TRANSFERRING;
            filecontext.need_chunk_index = cur+1;
            filecontext.has_been_uploaded.insert(cur);
            char *tmp = new char[data_len];
            memcpy(tmp,file_transfer->data().c_str(),data_len);
            filecontext.chunks[cur] = std::make_pair(data_len,tmp);
            // 发送响应
            status_update->set_status(fileprotocol::Status::TRANSFERRING);
            status_update->set_need_chunk_index(cur+1);
            uint32_t send_body_size = send_body.ByteSize();
            send_header.set_length(send_body_size);
            char send_body_bytes[send_body_size];
            send_body.SerializeToArray(send_body_bytes,sizeof(send_body_bytes));
            send_header.SerializeToArray(send_header_bytes,sizeof(send_header_bytes));
            if(sock->send(send_header_bytes,sizeof(send_header_bytes),0)==-1) return -1;
            if(sock->send(send_body_bytes,sizeof(send_body_bytes),0)==-1) return -1;
                
            if(file_transfer->status() == fileprotocol::Status::DONE)
            {
                filecontext.status = DONE;
                break;
            }
        }

        if(recv_header.type() == fileprotocol::MsgType::STATUS_UPDATE)
        {
            // 接收消息体（更新的状态信息）
            uint32_t recv_status_body_size = recv_header.length();
            char recv_status_body_bytes[recv_status_body_size];
            fileprotocol::MsgBody recv_status_body;
            sock->recv(recv_status_body_bytes,sizeof(recv_status_body_bytes),0);
            recv_status_body.ParseFromArray(recv_status_body_bytes,sizeof(recv_status_body_bytes));
            fileprotocol::StatusUpdate* status_update = recv_status_body.mutable_status_update();
            // 发送ack 
            recv_header.clear_type();
            recv_header.set_type(fileprotocol::MsgType::ACK);
            recv_header.set_length(0);
            memset(recv_header_bytes,0,sizeof(recv_header_bytes));
            recv_header.SerializeToArray(recv_header_bytes,sizeof(recv_header_bytes));
            if(sock->send(recv_header_bytes,sizeof(recv_header_bytes),0)==-1) return -1;
            // 维护全局文件信息
            if(status_update->status() == fileprotocol::Status::PAUSE)
            {
                filecontext.status = PAUSE;
                filecontext.need_chunk_index = status_update->need_chunk_index();
                // std::cout<<"pause at index:"<<filecontext.need_chunk_index<<std::endl;
                m_rw_mutex.wrlock();
                m_incomplete_files[filecontext.filehash] = filecontext;
                m_rw_mutex.unlock();
                return recv_total_size;
            }

            if(status_update->status() == fileprotocol::Status::END) // 取消发送
            {
                m_rw_mutex.wrlock();
                m_incomplete_files.erase(filecontext.filehash); 
                m_rw_mutex.unlock();
                std::cout<<"cancel the recv."<<std::endl;
                return -1;
            }
        }
        return recv_total_size;
    }
    bool FileOperator::send_ack(Socket::socket_ptr sock)
    {
        fileprotocol::MsgHeader ack_header;
        ack_header.set_magic(MY_PROTOCOL_MAGIC);
        ack_header.set_version(MY_PROTOCOL_VERSION);
        ack_header.set_length(0);
        ack_header.set_type(fileprotocol::MsgType::ACK);
        char ack_header_bytes[HEADER_LEN];
        ack_header.SerializeToArray(ack_header_bytes,sizeof(ack_header_bytes));
        if(sock->send(ack_header_bytes,sizeof(ack_header_bytes),0)==-1) return false;
        return true;
    }
    bool FileOperator::recv_file_server(fileprotocol::MsgHeader recv_header,std::string filepath, Socket::socket_ptr sock)
    {
        // 接收文件摘要
        fileprotocol::MsgBody file_summary_body;
        uint32_t recv_body_size = recv_header.length();
        char file_summary_bytes[recv_body_size];
        sock->recv(file_summary_bytes,sizeof(file_summary_bytes),0);
        file_summary_body.ParseFromArray(file_summary_bytes,sizeof(file_summary_bytes));
        fileprotocol::FileSummary* file_summary = file_summary_body.mutable_file_summary();

        std::string filename = file_summary->filename();
        std::string filehash = file_summary->filehash();
        uint32_t filesize = file_summary->filesize();
        uint32_t total_chunk = file_summary->total_chunk();
        m_rw_mutex.rdlock();
        int cnt = m_complete_files_hash.count(filehash);
        m_rw_mutex.unlock();
        
        fileprotocol::MsgHeader send_header;
        send_header.set_magic(MY_PROTOCOL_MAGIC);
        send_header.set_version(MY_PROTOCOL_VERSION);
        send_header.set_type(fileprotocol::MsgType::STATUS_UPDATE);
        char send_header_bytes[HEADER_LEN];
        fileprotocol::MsgBody send_body;
        fileprotocol::StatusUpdate* status_update = send_body.mutable_status_update();
        uint32_t send_body_size = 0 ;

        if(cnt!=0) // 本机有完整版（妙传）
        {
            status_update->set_status(fileprotocol::Status::DONE);
            send_body_size= send_body.ByteSize();
            char send_body_bytes[send_body_size];
            send_header.set_length(send_body_size);
            send_header.SerializeToArray(send_header_bytes,sizeof(send_header_bytes));
            send_body.SerializeToArray(send_body_bytes,sizeof(send_body_bytes));
            if(sock->send(send_header_bytes,sizeof(send_header_bytes),0)==-1) return false;
            if(sock->send(send_body_bytes,sizeof(send_body_bytes),0)==-1) return false;
            printf("success! received %f KB.\n",filesize/(1024.0));
            return true;
        }
        
        // 发送响应信息
        size_t recv_total_size = 0;
        FileCTX filecontext = {};
        if(cnt!=0) // 本机有分片（续传）
        {
            filecontext = m_incomplete_files[filehash];
            status_update->set_status(fileprotocol::Status::PAUSE);
            status_update->set_need_chunk_index(filecontext.need_chunk_index);
            recv_total_size += filecontext.need_chunk_index * CHUNK_SIZE;
        }
        else // 新文件
        {
            filecontext.filehash = filehash;
            filecontext.total_chunk = total_chunk;
            filecontext.need_chunk_index = 0;
            filecontext.has_been_uploaded.clear();
            filecontext.chunks.clear();
            filecontext.chunks.resize(total_chunk);
            status_update->set_status(fileprotocol::Status::TRANSFERRING);
            status_update->set_need_chunk_index(0);
        }
        send_body_size= send_body.ByteSize();
        char send_body_bytes[send_body_size];
        send_header.set_length(send_body_size);
        send_header.SerializeToArray(send_header_bytes,sizeof(send_header_bytes));
        send_body.SerializeToArray(send_body_bytes,sizeof(send_body_bytes));
        if(sock->send(send_header_bytes,sizeof(send_header_bytes),0)==-1) return false;
        if(sock->send(send_body_bytes,sizeof(send_body_bytes),0)==-1) return false;

        // 接收文件内容
        while(true)
        {
            int recv_size = splice_recv_server(filecontext,sock);
            if(recv_size == -1) return false;
            recv_total_size += recv_size;

            if(filecontext.status==DONE) break;
            if(filecontext.status == PAUSE)
            {
                fileprotocol::MsgHeader recv_update_header;
                char recv_update_header_bytes[HEADER_LEN];
                sock->recv(recv_update_header_bytes,sizeof(recv_update_header_bytes),0);
                recv_update_header.ParseFromArray(recv_update_header_bytes,sizeof(recv_update_header_bytes));
                if(recv_update_header.type() != fileprotocol::MsgType::STATUS_UPDATE)
                {
                    std::cout<<"something error."<<std::endl;
                    return false;
                }
                uint32_t recv_update_body_size = recv_update_header.length();
                char recv_update_body_bytes[recv_update_body_size];
                fileprotocol::MsgBody recv_update_body;
                sock->recv(recv_update_body_bytes,sizeof(recv_update_body_bytes),0);
                recv_update_body.ParseFromArray(recv_update_body_bytes,sizeof(recv_update_body_bytes));
                fileprotocol::StatusUpdate* status_update = recv_update_body.mutable_status_update();
                // 发送ack
                if(!send_ack(sock)) return false;

                if(status_update->status() == fileprotocol::Status::TRANSFERRING)
                {
                    continue;
                } 
                else if(status_update->status() == fileprotocol::Status::END)
                {
                    std::cout<<"client cancel send."<<std::endl;
                    return true;
                }
            }
        }
        // 分片收齐
        if(filecontext.has_been_uploaded.size() == filecontext.total_chunk) 
        {
            filepath += filename;
            std::cout<<"writing file:"<<filepath<<std::endl;
            FILE *fp = fopen(filepath.c_str(),"wb");
            if(fp==NULL)
            {
                printf("open file error.\n");
                return false;
            }
            auto chunks = filecontext.chunks;
            for(int i=0;i<chunks.size();i++)
            {
                fwrite(chunks[i].second,1,chunks[i].first,fp);
            }
            fclose(fp);
            if(get_file_md5(filepath)==filehash)
            {
                m_rw_mutex.wrlock();
                m_complete_files_hash.insert(filehash);
                m_rw_mutex.unlock();
                printf("success! received %f KB.\n",recv_total_size/(1024.0));
                return true;
            }
            else
            {
                printf("md5 not equal.\n");
                return false;
            }
        }
    }
}