#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <iostream>
#include <vector>
#include <dirent.h>
#include <cstring>
#include <errno.h>
#include <cmath>
#include <istream>
#include <sstream>


#include "../src/protocol/protocol.pb.h"

using namespace std;
 
#define portnum 12345
#define FILE_SIZE 500 
#define BUFFER_SIZE (10*1024)
#define HEADER_LEN 20

struct fileContext
{
	char filename[50];
	size_t filesize;
};

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

std::string get_file_md5(string filepath)
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
    return result;
}

void getFileNames(std::string path, std::vector<string>& files)
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
 
int main()
{
	//初始化套接字
	int server_fd=socket(AF_INET,SOCK_STREAM,0);
	//绑定端口和ip;
	sockaddr_in server_addr;   //struct sockaddr_in为结构体类型 ，server_addr为定义的结构体   
	server_addr.sin_family=AF_INET;   //Internet地址族=AF_INET(IPv4协议) 
	server_addr.sin_port=htons(portnum);  //将主机字节序转化为网络字节序 ,portnum是端口号
	(server_addr.sin_addr).s_addr=htonl(INADDR_ANY);//IP地址

	bind(server_fd,(struct sockaddr *)&server_addr,sizeof(server_addr));

	while(1)
	{
		//开启监听
		listen(server_fd,5); //5是最大连接数，指服务器最多连接5个用户
		std::cout<<"listening"<<std::endl;

		sockaddr_in client_addr;
		uint32_t size=sizeof(client_addr);
		int connect_fd=accept(server_fd,(struct sockaddr *)&client_addr,&size);  //server_fd服务器的socket描述字,&client_addr指向struct sockaddr *的指针,&size指向协议地址长度指针
		printf("accepted client ip:%s:%d\n",inet_ntoa(client_addr.sin_addr),client_addr.sin_port);

		fileprotocol::MsgHeader send_header;
		fileprotocol::MsgBody send_body;
		send_header.set_magic(0xABCD);
		send_header.set_version(240810);

		while(1)
		{
			fileprotocol::MsgHeader recv_header;
			char recv_header_bytes[HEADER_LEN];
			recv(connect_fd,recv_header_bytes,sizeof(recv_header_bytes),0);
			recv_header.ParseFromArray(recv_header_bytes,sizeof(recv_header_bytes));

			if(recv_header.magic()!=0xABCD || recv_header.version()!=240810)
			{
				std::cout<<"not my package."<<std::endl;
				continue;
			}

			if(recv_header.type()==fileprotocol::MsgType::BROWSE_REQUEST)
			{
				std::cout<<"client request optional files list."<<std::endl;
				vector<string> fileNames;
      			string path("file");
      			getFileNames(path, fileNames);
				send_header.set_type(fileprotocol::MsgType::BROWSE_RESPONSE);
				fileprotocol::BrowseResponse browse_response;
      			for (const auto &ph : fileNames) 
				{
					browse_response.add_filenames(ph);
	    		}
				send_body.set_allocated_browse_response(&browse_response);
				uint32_t send_body_size = send_body.ByteSize();
				char send_body_bytes[send_body_size];
				send_body.SerializeToArray(send_body_bytes,send_body_size);
				char send_header_bytes[HEADER_LEN];
				send_header.set_length(send_body_size);
				send_header.SerializeToArray(send_header_bytes,sizeof(send_header_bytes));
				send(connect_fd,send_header_bytes,sizeof(send_header_bytes),0);
      			send(connect_fd, send_body_bytes,send_body_size,0);
				recv_header.clear_type();
				memset(recv_header_bytes,0,sizeof(recv_header_bytes));
				recv(connect_fd,recv_header_bytes,sizeof(recv_header_bytes),0);
				recv_header.ParseFromArray(recv_header_bytes,sizeof(recv_header_bytes));
				if(recv_header.type()==fileprotocol::MsgType::ACK) continue;
				else break;
			}
			else if (recv_header.type()==fileprotocol::MsgType::FILE_UPLOAD_REQUEST)
			{
				// 接收文件摘要
				fileprotocol::MsgBody recv_body;
				fileprotocol::FileSummary file_summary;
				uint32_t recv_body_size = recv_header.length();
				char recv_body_bytes[recv_body_size];
				int size = 0;
				while(size<recv_body_size)
				{
					int remain_len = recv_body_size-size;
					size += recv(connect_fd,recv_body_bytes+size,remain_len,0);
				}
				recv_body.ParseFromArray(recv_body_bytes,recv_body_size);
				file_summary = recv_body.file_summary();
				std::cout<<"client wanna upload file:"<<file_summary.filename()<<std::endl;
				
				// 发送ack
				char send_header_bytes[HEADER_LEN];
				send_header.set_type(fileprotocol::MsgType::ACK);
				send_header.set_length(0);
				send_header.SerializeToArray(send_header_bytes,sizeof(send_header_bytes));
				send(connect_fd,send_header_bytes,sizeof(send_header_bytes),0);

				// 接收文件内容
				fileprotocol::FileTransfer file_transfer;
				recv_body.Clear();
				memset(recv_header_bytes,0,sizeof(recv_header_bytes));
				recv(connect_fd,recv_header_bytes,sizeof(recv_header_bytes),0);
				recv_header.ParseFromArray(recv_header_bytes,sizeof(recv_header_bytes));
				recv_body_size = recv_header.length();
				char *recv_body_bytes_ctx = new char[recv_body_size];
				size = 0;
				while(size<recv_body_size)
				{
					int remain_len = recv_body_size-size;
					size += recv(connect_fd,recv_body_bytes_ctx+size,remain_len,0);
				}
				recv_body.ParseFromArray(recv_body_bytes_ctx,recv_body_size);
				file_transfer = recv_body.file_transfer();

				char file[50] = "./file/";
      			strcat(file,file_summary.filename().c_str());
      			FILE *fp = fopen(file, "wb");
				fwrite(file_transfer.data().c_str(), 1, file_transfer.data_len(), fp);
				fclose(fp);
				string md5 = get_file_md5(file);
				if(md5 == file_summary.filehash())
				{
					printf("received %f KB\n", file_transfer.data_len()/(1024.0));
				}
				else
				{
					std::cout<<"Error:md5 not equal."<<std::endl;
					break;
				}

				// 发送ack
				memset(send_header_bytes,0,sizeof(send_header_bytes));
				send_header.set_type(fileprotocol::MsgType::ACK);
				send_header.set_length(0);
				send_header.SerializeToArray(send_header_bytes,sizeof(send_header_bytes));
				send(connect_fd,send_header_bytes,sizeof(send_header_bytes),0);
				delete recv_body_bytes_ctx;

			}
			else if(recv_header.type()==fileprotocol::MsgType::FILE_DOWNLOAD_REQUEST)
			{
				fileContext re_fileMsg;
      			char re_filename[50] = {0};
      			if(recv(connect_fd,re_filename,sizeof(re_filename), 0)==-1)
				{
					perror("recv error.");
				}
      			std::cout<<re_filename<<std::endl;

      			char re_file[50] = "./file/";
      			strcat(re_file,re_filename);
      			std::cout<<"traget file:"<<re_file<<std::endl;

      			FILE *fp = fopen(re_file, "rb");
      			fseek( fp, 0, SEEK_END );
      			int totalSize =  ftell(fp);
      			std::cout<<totalSize<<std::endl;
      			fclose(fp);
      			fp = fopen(re_file, "rb");
      			int  readSize = 0;
      			int  sendTotalSize = 0;
      			char sendBuf[1024*1024] = {0};
      			while((readSize = fread(sendBuf, 1, sizeof(sendBuf), fp)) > 0)
      			{
        			send(connect_fd, sendBuf, readSize, 0);
        			sendTotalSize += readSize;

        			printf("send %f KB\n", sendTotalSize/(1024.0));
      			}
      			fclose(fp);
      			if(sendTotalSize == totalSize) printf("Done!\n");
      			else printf("Error!");
			}
			else if(recv_header.type()==fileprotocol::MsgType::CLOSE)
			{
				break;
			}
			else{
				std::cout<<"please input 1~4"<<std::endl;
			}
			
		}
		
	}
		
	close(server_fd);
	return 0;
 
}
 
 
 
// void *net_thread(void * fd)
// {
// 	pthread_detach(pthread_self()); //线程分离
// 	int new_fd=*((int *)fd);
// 	int file2_fp;
	
// 	while(1)
// 	{
// 		// recv函数接收数据到缓冲区buffer中 
//         char buffer[BUFFER_SIZE];
//         memset( buffer,0, sizeof(buffer) );		
//         if(read(new_fd, buffer, sizeof(buffer)) < 0) 
//         { 
//            perror("Server Recieve Data Failed:"); 
//            break; 
//         } 
		
//         // 然后从buffer(缓冲区)拷贝到file_name中 
//         char file_name[FILE_SIZE]; 
// 		memset( file_name,0, sizeof(file_name) );	
//         strncpy(file_name, buffer, strlen(buffer)>FILE_SIZE?FILE_SIZE:strlen(buffer)); 
// 		memset( buffer,0, sizeof(buffer) );
//         printf("%s\n", file_name); 
		
// 		if( strcmp(file_name,"null")==0 )
// 	    {
// 		   break;
// 		   close(new_fd);
// 	    }
		
// 		  // 打开文件并读取文件数据 
//          file2_fp = open(file_name,O_RDONLY,0777); 
//          if(file2_fp<0) 
//          { 
//             printf("File:%s Not Found\n", file_name); 
//          } 
//          else 
//          { 
//             int length = 0; 
// 			memset( buffer,0, sizeof(buffer) );
//             // 每读取一段数据，便将其发送给客户端，循环直到文件读完为止 
//             while( (length = read(file2_fp, buffer, sizeof(buffer))) > 0  )    
//             {   
//                 if( write(new_fd, buffer, length) < 0) 
//                 { 
//                     printf("Send File:%s Failed.\n", file_name); 
//                     break; 
//                 } 
//                 memset( buffer,0, sizeof(buffer) );
//             } 
//               // 关闭文件 
//               close(file2_fp); 
//               printf("File:%s Transfer Successful!\n", file_name); 
//          }   
// 	}
// 	close(new_fd);
// }