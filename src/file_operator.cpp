#include <atomic>
#include <stdio.h>
#include <cmath>
#include <istream>
#include <sstream>
#include <iostream>


#include "file_operator.h"

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

    std::string FileOperator::get_file_md5(char filepath[FILE_NAME_LEN])
    {
        char cwd[100];
        getcwd(cwd,sizeof(cwd));
        chdir(cwd);
        char cmd[100] = "md5sum ";
        strcat(cmd,filepath);
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

    int FileOperator::splice_send_client(FILE *fp,int start_index, SendInfo& send_info,Socket::socket_ptr sock)
    {
        sock->clear_recv_buffer();
        char flag[4];
        int send_size = 0;
        printf("(you can enter 'p' to pause sending)\n");
        pthread_t send_pause;
        pthread_create(&send_pause,NULL,monitor_send_signal,NULL);
        pthread_detach(send_pause);

        fseek(fp,start_index*CHUNK_SIZE,SEEK_SET);
        send_info.chunk_index = start_index;
        char buffer[CHUNK_SIZE]={0};
        pause_send_signal = false;
        int rt = 0;
        int size =0;
        long long i=0;
        ResponseInfo res_info;

        while(send_info.chunk_index < send_info.total_chunk && !pause_send_signal) // 分片发送
        {
            size = fread(buffer,1,sizeof(buffer),fp);
            send_info.data_len = size;
            if(send_info.chunk_index == send_info.total_chunk-1)  // 最后一片
            {
                send_info.status = DONE;
            }
            rt = sock->send((char*)&send_info,sizeof(send_info),0);
            if(rt==-1) return -1;
            sock->recv(&flag,4,0);
            rt = sock->send(buffer,sizeof(buffer),0);
            if(rt==-1) return -1;
            i=send_info.chunk_index;
            
            send_size += size;
            sock->recv(&res_info,sizeof(res_info),0); // 暂停后这里的res_info.status没有改
            if(res_info.status!=TRANSFERRING)
            {
                break;
            }
            send_info.chunk_index++;
            memset(buffer,0,sizeof(buffer));
            if(i<=send_info.total_chunk-1)
            {
                printf("\rsending[%.0lf%%]",i*100.0/(send_info.total_chunk-1));
            }
        }
        
        pthread_cancel(send_pause);
        
        if(pause_send_signal) // 暂停发送 
        {
            send_info.status = PAUSE;
            rt = sock->send((char*)&send_info,sizeof(send_info),0);
            if(rt==-1) return -1;
            sock->recv(&flag,4,0);
            send_size -=size;
        }
        std::cout<<std::endl;
        return send_size;
    }

    bool FileOperator::send_file_client(const char path[FILE_NAME_LEN], FileHeader filectx, Socket::socket_ptr sock)
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
            size_t tmp = splice_send_client(fp,start_chunk,send_info,sock);
            if(tmp==-1) return false;
            send_total_size += tmp;
            if(send_info.status == DONE) // 发完了
            {
                break;
            }

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
                send_info.status = TRANSFERRING;
                start_chunk = send_info.chunk_index;
                int rt = sock->send((char*)&send_info,sizeof(send_info),0);
                if(rt==-1) return false;
                sock->recv(&flag,4,0);
            }
            else if(signal == "e") // 终止发送
            {
                send_info.status = END;
                int rt = sock->send((char*)&send_info,sizeof(send_info),0);
                if(rt==-1) return false;
                sock->recv(&flag,4,0);
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
            strcpy(filecontext.filehash,filectx.filehash);
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
        
        // if(send_total_size == filectx.filesize)
        // {
        //     printf("Done.\n");
        //     return true;
        // }
        // else
        // {
        //     printf("Error:send size and file size is not equal.");
        //     return false;
        // }
    }
    int FileOperator::splice_recv_server(FileCTX& filecontext,ResponseInfo& res_info,Socket::socket_ptr sock)
    {
        sock->clear_recv_buffer();
        SendInfo send_info;
        int recv_total_size = 0;
        char flag[4];
            
        while(true)
        {
            char buffer[CHUNK_SIZE]={0};
            int header_len = sock->recv((char*)&send_info,sizeof(send_info),0);
            int rt = sock->send("yes",4,0);
            if(rt==-1) return false;
            res_info.status = send_info.status;
            if(send_info.status == PAUSE || send_info.status == END) break; // 不携带数据了
            
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
            filecontext.status = send_info.status;
            filecontext.total_chunk = send_info.total_chunk;
            filecontext.chunks.resize(filecontext.total_chunk);
            size_t cur = send_info.chunk_index;
            filecontext.has_been_uploaded.insert(cur);
            char *tmp = new char[CHUNK_SIZE];
            memcpy(tmp,buffer,sizeof(buffer));
            filecontext.chunks[cur] = std::make_pair(data_len,tmp);
                
            if(send_info.status == DONE)
            {
                break;
            }
        }

        if(res_info.status == PAUSE)
        {
            filecontext.need_chunk_index = send_info.chunk_index;
            res_info.need_chunk_index = send_info.chunk_index;
            m_rw_mutex.wrlock();
            m_incomplete_files[filecontext.filehash] = filecontext;
            m_rw_mutex.unlock();
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
    bool FileOperator::recv_file_server(char path[FILE_NAME_LEN], FileHeader filectx, Socket::socket_ptr sock)
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
            printf("success! received %f KB.\n",filectx.filesize/(1024.0));
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
            strcpy(filecontext.filehash,filectx.filehash);
            filecontext.has_been_uploaded.clear();
            filecontext.chunks.clear();
            res_info.status = filecontext.status =TRANSFERRING;
            res_info.need_chunk_index = filecontext.need_chunk_index = 0;
            // m_incomplete_files[res_info.filehash] = filecontext;
        }
        int rt = sock->send((char*)&res_info,sizeof(res_info),0);
        if(rt==-1) return false;

        char flag[4];
        while(true)
        {
            int recv_size = splice_recv_server(filecontext,res_info,sock);
            if(recv_size == -1) return false;
            recv_total_size += recv_size;

            if(res_info.status==DONE) break;
            if(res_info.status == PAUSE)
            {
                SendInfo send_info;
                sock->recv(&send_info,sizeof(send_info),0);
                res_info.status = send_info.status;
                sock->send("yes",4,0);
                if(send_info.status==TRANSFERRING)
                {
                    continue;
                } 
                else if(send_info.status==END)
                {
                    std::cout<<"client cancel send."<<std::endl;
                    return true;
                }
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