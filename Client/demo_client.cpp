#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <cstring>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <iostream>
#include <errno.h>
#include <cmath>
#include <istream>
#include <sstream>

#include "../src/protocol/protocol.pb.h"
#include "../src/db/user.h"

using namespace std;
 
#define portnum 12345
#define FILE_SIZE 500 
#define BUFFER_SIZE (10*1024)
#define HEADER_LEN 20
#define FILE_NAME_LEN 50

struct fileContext
{
	char filename[50];
	size_t filesize;
};

void showmenu()
{
    std::cout << "please enter 1, 2, 3,or 4:\n"
            "1) browse           2) upload\n"
            "3) download         4) quit\n";
}
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

 
int main()
{
	char name[30]={0};
	printf("请输入服务器的主机名或者ip\n");
	scanf("%s",name);
	struct hostent *h;
	//获取服务器信息
	h=gethostbyname(name);
	//初始化套接字
	int client_fd=socket(AF_INET,SOCK_STREAM,0);
	if(-1==client_fd)
	{
		perror("socket");
		exit(2);
	}
	struct sockaddr_in server_addr;
	server_addr.sin_family=AF_INET;
	server_addr.sin_port=htons(portnum);
	server_addr.sin_addr=*((struct in_addr *)h->h_addr_list[0]);

	connect(client_fd,(struct sockaddr *)&server_addr,sizeof(server_addr));
	std::cout<<"connected!"<<std::endl;

	showmenu();
	int choice;
	std::cin>>choice;
	std::cin.sync();

	fileprotocol::MsgHeader send_header;
	fileprotocol::MsgBody send_body;
	send_header.set_magic(0xABCD);
	send_header.set_version(240810);
	char send_header_bytes[HEADER_LEN];
	// 登录
	string username;
	string passwd;
	std::cout<<"username:";
	std::cin>>username;
	std::cout<<"password:";
	std::cin>>passwd;
	send_header.set_type(fileprotocol::MsgType::LOGIN_REQUEST);
	fileprotocol::LoginRequest* login_request = send_body.mutable_login_request();
	login_request->set_username(username);
	login_request->set_passwd(passwd);
	uint32_t send_body_size = send_body.ByteSize();
	send_header.set_length(send_body_size);
	send_header.SerializeToArray(send_header_bytes,sizeof(send_header_bytes));
	char send_body_bytes[send_body_size];
	send_body.SerializeToArray(send_body_bytes,sizeof(send_body_bytes));
	send(client_fd,send_header_bytes,sizeof(send_header_bytes),0);
	send(client_fd,send_body_bytes,send_body_size,0);

	fileprotocol::MsgHeader recv_ack_header;
    char recv_ack_header_bytes[HEADER_LEN];
	recv(client_fd,recv_ack_header_bytes,sizeof(recv_ack_header_bytes),0);
	recv_ack_header.ParseFromArray(recv_ack_header_bytes,sizeof(recv_ack_header_bytes));
	if(recv_ack_header.type() != fileprotocol::MsgType::ACK)
    {
        std::cout<<"login error."<<std::endl;
    }
        return true;


	
	while(1)
	{
		char choice_send[4] = {0};
		sprintf(choice_send,"%d",choice);
		send_header.Clear();
		send_body.clear_body();
		
  		if(choice == 1)
  		{
			send_header.set_type(fileprotocol::MsgType::BROWSE_REQUEST);
			send_header.set_length(0);
    		send_header.SerializeToArray(send_header_bytes,sizeof(send_header_bytes));
			send(client_fd,send_header_bytes,sizeof(send_header_bytes),0);
			
			fileprotocol::MsgHeader recv_header;
			char recv_header_bytes[HEADER_LEN];
			recv(client_fd,recv_header_bytes,sizeof(recv_header_bytes),0);
			recv_header.ParseFromArray(recv_header_bytes,sizeof(recv_header_bytes));

			if(recv_header.magic()!=0xABCD || recv_header.version()!=240810)
			{
				std::cout<<"not my package."<<std::endl;
				continue;
			}

			fileprotocol::MsgBody recv_body;
			uint32_t recv_body_size = recv_header.length();
			char recv_body_bytes[recv_body_size];
			recv(client_fd,recv_body_bytes,sizeof(recv_body_bytes),0);
			recv_body.ParseFromArray(recv_body_bytes,sizeof(recv_body_bytes));
			std::cout<<"Files on the server:"<<std::endl;
			for(auto filename:recv_body.browse_response().filenames())
			{
				std::cout<<filename<<std::endl;
			}
			// 发送ack包告诉服务器已收到
			send_header.set_type(fileprotocol::MsgType::ACK);
			memset(send_header_bytes,0,sizeof(send_header_bytes));
			send_header.SerializeToArray(send_header_bytes,sizeof(send_header_bytes));
			send(client_fd,send_header_bytes,sizeof(send_header_bytes),0);
			getchar();
  		}
  		else if (choice == 2) // 上传
  		{
    		
			fileprotocol::MsgBody send_body;
			fileprotocol::FileSummary* file_summary = send_body.mutable_file_summary();
			// fileprotocol::FileSummary file_summary;

			string filename;
    		std::cout<<"please input upload file's name:"<<std::endl;
    		std::cin>>filename;
    		FILE *fp = fopen(filename.c_str(), "rb");
    		fseek( fp, 0, SEEK_END);
    		int totalSize =  ftell(fp);
    		fclose(fp);

			// 发送文件摘要
			send_header.set_type(fileprotocol::MsgType::FILE_UPLOAD_REQUEST);
    		file_summary->set_filename(filename);
			file_summary->set_format(filename.substr(filename.find_last_of('.') + 1));
			file_summary->set_filehash(get_file_md5(filename));
			file_summary->set_filesize(totalSize);
			// send_body.set_allocated_file_summary(&file_summary);
			uint32_t send_body_size = send_body.ByteSize();
			send_header.set_length(send_body_size);
			char* send_body_bytes = new char[send_body_size];
			send_body.SerializeToArray(send_body_bytes,send_body_size);
			char send_header_bytes[HEADER_LEN];
			send_header.SerializeToArray(send_header_bytes,sizeof(send_header_bytes));
			send(client_fd,send_header_bytes,sizeof(send_header_bytes),0);
      		send(client_fd,send_body_bytes,send_body_size,0);
			delete send_body_bytes;

			std::cout<<"filename:"<<file_summary->filename()<<std::endl;
			std::cout<<"fileformat:"<<file_summary->format()<<std::endl;
			std::cout<<"filehash:"<<file_summary->filehash()<<std::endl;
			std::cout<<"filesize:"<<file_summary->filesize()<<std::endl;


			//接收ack
			fileprotocol::MsgHeader recv_header;
			char recv_header_bytes[HEADER_LEN];
			recv(client_fd,recv_header_bytes,sizeof(recv_header_bytes),0);
			recv_header.ParseFromArray(recv_header_bytes,sizeof(recv_header_bytes));
			if(recv_header.type()!=fileprotocol::MsgType::ACK)
			{
				std::cout<<"server maybe refused."<<std::endl;
				break;
			}
			
			// 发送文件内容
			uint32_t filesize = file_summary->filesize();
			string filehash = file_summary->filehash();
			send_header.clear_type();
			send_header.set_type(fileprotocol::MsgType::FILE_TRANSFER);
			send_body.clear_body();
			fileprotocol::FileTransfer* file_transfer = send_body.mutable_file_transfer();
			// fileprotocol::FileTransfer file_transfer;
			file_transfer->set_status(fileprotocol::Status::TRANSFERRING);
			file_transfer->set_filehash(filehash);
			
    		fp = fopen(filename.c_str(), "rb");
    		int  readSize = 0;
    		int  sendTotalSize = 0;
    		char sendBuf[filesize] = {0};
    		while(sendTotalSize<filesize)
    		{
      			readSize = fread(sendBuf, 1, sizeof(sendBuf), fp); 
      			sendTotalSize += readSize;
    		}
    		fclose(fp);
			file_transfer->set_chunk_index(0);
			file_transfer->set_data_len(sizeof(sendBuf));
			file_transfer->set_data(sendBuf,sizeof(sendBuf));

			send_body_size = send_body.ByteSize();
			send_header.clear_length();
			send_header.set_length(send_body_size);
			char* send_body_bytes_ctx = new char[send_body_size];
			send_body.SerializeToArray(send_body_bytes_ctx,send_body_size);
			memset(send_header_bytes,0,sizeof(send_header_bytes));
			send_header.SerializeToArray(send_header_bytes,sizeof(send_header_bytes));
			send(client_fd,send_header_bytes,sizeof(send_header_bytes),0);
      		send(client_fd,send_body_bytes_ctx,send_body_size,0);
			

			// 接收ACK
			memset(recv_header_bytes,0,sizeof(recv_header_bytes));
			recv(client_fd,recv_header_bytes,sizeof(recv_header_bytes),0);
			recv_header.Clear();
			recv_header.ParseFromArray(recv_header_bytes,sizeof(recv_header_bytes));
			if(recv_header.type()==fileprotocol::MsgType::ACK)
			{
				printf("sent %f KB\n", sendTotalSize/(1024.0));
			}
			else
			{
				printf("Error!");
				break;
			}
			delete send_body_bytes_ctx;
    		getchar();
  		}
  		// 客户端下载文件
  		else if (choice == 3)
  		{
    		fileContext rec_fileMsg;
    		char rec_filename[50] ={0};
    		std::cout<<"please input download file's name:"<<std::endl;
    		std::cin>>rec_filename;
			std::cout<<rec_filename<<std::endl;
    		
			if(send(client_fd, rec_filename, sizeof(rec_filename), 0)==-1)
			{
				perror("send error.");
			}

    		char recvBuf[1024*1024] = {0}; 
    		int recvTotalSize = 0;
    		FILE *fp = fopen(rec_filename, "wb");
    		int fileSize = 16;
    		while (recvTotalSize<fileSize)
    		{
      			int recvSize = recv(client_fd,recvBuf,sizeof(recvBuf), 0);
      			recvTotalSize+=recvSize;
      			printf("received %f KB\n", recvTotalSize/(1024.0));
      			fwrite(recvBuf, 1, recvSize, fp);
    		}
    		fclose(fp);
    		getchar();
  		}
  		// 退出
  		else if (choice == 4)
  		{
    		std::cout<<"Bye!"<<std::endl;
    		break;
  		}
  		else
		{
    		std::cout<<"please input valid number!"<<std::endl;
  		}
  		showmenu();
  		std::cin >> choice;
  		std::cin.sync();
 
    }
	return 0;
	
}


// int sendfile(int sockfd)
// {
//     // 输入文件名 并放到缓冲区buffer中等待发送 
// 	int file_fp;
//     char file_name[FILE_SIZE];  
// 	memset( file_name,0, sizeof(file_name) );
//     printf("Please Input File Name On Server:   "); 
//     scanf("%s", file_name); 
   
//     char buffer[BUFFER_SIZE]; 
//     memset( buffer,0, sizeof(buffer) );
//     strncpy(buffer, file_name, strlen(file_name)>sizeof(buffer)?sizeof(buffer):strlen(file_name)); 
     
//     // 向服务器发送buffer中的数据 
//     if(write(sockfd, buffer, sizeof(buffer)) < 0) 
//     { 
//        perror("Send File Name Failed:"); 
//        exit(1); 
//     } 
	
// 	if( strcmp(file_name,"null")==0 )
// 	{
// 		exit(1);
// 		close(sockfd);
// 	}	
// 	 // 打开文件，准备写入 
//      file_fp = open(file_name,O_CREAT|O_RDWR,0777); 
//      if( file_fp<0 ) 
//      { 
//          printf("File:\t%s Can Not Open To Write\n", file_name); 
//          exit(1); 
//      } 
   
//      // 从服务器接收数据到buffer中 
//      // 每接收一段数据，便将其写入文件中，循环直到文件接收完并写完为止 
//     int length = 0; 
// 	memset( buffer,0, sizeof(buffer) );
   
//     while((length = read(sockfd, buffer, sizeof(buffer))) > 0) 
//     { 
//         if( write( file_fp, buffer, length ) < length) 
//         { 
//             printf("File:\t%s Write Failed\n", file_name); 
//             break; 
//         } 
// 		if(length < sizeof(buffer))
// 		{
// 			break;
// 		}
//         memset( buffer,0, sizeof(buffer) );
//     } 
	 
// 	// 接收成功后，关闭文件，关闭socket 
//      printf("Receive File:\t%s From Server IP Successful!\n", file_name); 
//      close(file_fp); 	
// }