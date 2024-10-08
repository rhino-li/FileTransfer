/**
 * 封装通用文件操作api
 */
#ifndef __FILE_OPERATOR_H__
#define __FILE_OPERATOR_H__

#include <string>
#include <vector>
#include <dirent.h>
#include <unordered_set>
#include <unordered_map>
#include <signal.h>
#include <sys/stat.h>
#include "mutex.h"
#include "socket.h"
#include "../src/protocol/protocol.pb.h"

#define FILE_NAME_LEN 50
#define HASH_LEN 100
#define CHUNK_SIZE (300*1024)  // 分片大小

#define HEADER_LEN 20
#define MY_PROTOCOL_MAGIC 0xABCD
#define MY_PROTOCOL_VERSION 240810

namespace filetrans
{

    struct FileHeader
    {
	    char filename[FILE_NAME_LEN];
	    size_t filesize;
        char filehash[HASH_LEN] = {0};
    };

    enum Status
    {
        TRANSFERRING=1, // 传输
        PAUSE=2, // 暂停
        END=3, // 取消
        DONE=4 // 完成
    };

    struct SendInfo
    {
        Status status;
        char filehash[HASH_LEN]={0};
        long long total_chunk;
        long long chunk_index; // 当前chunk的id
        size_t data_len; // 当前块的长度
    };
    
    struct ResponseInfo
    {
        Status status;
        char filehash[HASH_LEN]={0}; 
        long long need_chunk_index; // 期待的chunk
    };
    struct FileCTX
    {
        Status status; 
        std::string filehash;
        long long total_chunk;
        long long need_chunk_index; // 期待的chunk
        std::unordered_set<int> has_been_uploaded; // 曾经上传过的分片id
        std::vector<std::pair<size_t,const char*>> chunks; // 第[i]个chunk的内容
    };
    

    class FileOperator :public std::enable_shared_from_this<FileOperator>
    {
    public:
        RWMutex m_rw_mutex;
        std::unordered_set<std::string> m_complete_files_hash; // 本地所有完整文件的hash（用于秒传）(status==DONE)
        std::unordered_map<std::string,FileCTX> m_incomplete_files; // 本地所有不完整文件已收到的chunks内容
       
        FileOperator();
        typedef std::shared_ptr<FileOperator> fileop_ptr;
        void get_all_files_md5(const std::string dir);
        std::string get_file_md5(std::string filepath);
        static bool delete_folder(const char* path);

        bool send_update_status_request(fileprotocol::Status latest,Socket::socket_ptr sock);
        bool send_ack(Socket::socket_ptr sock);
        bool recv_ack(Socket::socket_ptr sock);

        int splice_send_client(FILE *fp,int start_index,uint32_t total_chunk,fileprotocol::MsgBody& send_body,Socket::socket_ptr sock);
        int splice_recv_client(FileCTX& filecontext,ResponseInfo& res_info,Socket::socket_ptr sock);
        bool send_file_client(const char* filepath,Socket::socket_ptr sock);
        bool recv_file_client(char path[FILE_NAME_LEN],FileHeader filectx,Socket::socket_ptr sock);
        
        int splice_send_server(FILE *fp,int start_index,SendInfo& send_info,Socket::socket_ptr sock);
        int splice_recv_server(FileCTX& filecontext, Socket::socket_ptr sock);
        bool send_file_server(const char path[FILE_NAME_LEN],FileHeader filectx,Socket::socket_ptr sock);
        bool recv_file_server(fileprotocol::MsgHeader recv_header,std::string filepath,Socket::socket_ptr sock);
    };
    

}

#endif
