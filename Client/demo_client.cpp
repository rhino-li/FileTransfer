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

using namespace std;
 
#define portnum 12345
#define FILE_SIZE 500 
#define BUFFER_SIZE 1024
 
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

// int sendfile(int sockfd);
 
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
	
	while(1)
	{
		char choice_send[4] = {0};
		sprintf(choice_send,"%d",choice);
  		// itoa(choice,choice_send, 10);
  		send(client_fd, choice_send, sizeof(choice_send), 0);
  		// 浏览服务器上的文件
  		if(choice == 1)
  		{
    		char allfilesname[500];
    		recv(client_fd, allfilesname, sizeof(allfilesname), 0);
    		std::cout<<"Files on the server:"<<std::endl;
    		// 字符串分割
    		char *token;
    		token = strtok(allfilesname,";");

    		while(token!=NULL)
    		{
        		std::cout<<token<<std::endl;
        		token = strtok(NULL,";");
    		}
    		getchar();
  		}
  		else if (choice == 2)
  		{
    		fileContext fileCtx;
    		char filename[50] ={0};
    		std::cout<<"please input upload file's name:"<<std::endl;
    		std::cin>>filename;

    		FILE *fp = fopen(filename, "rb");
    		fseek( fp, 0, SEEK_END );
    		int totalSize =  ftell(fp);
    		fclose(fp);
  
    		// ftp client发送文件长度及文件名
    		char fileSizeStr[20] = {0};
			sprintf(fileSizeStr,"%d",totalSize);
    		// itoa(totalSize, fileSizeStr, 10);
    		fileCtx.filesize = totalSize;
    		strcpy(fileCtx.filename,filename);

    		// send(sockClient, fileSizeStr, strlen(fileSizeStr) + 1, 0);

    		send(client_fd, (char*)&fileCtx, sizeof(fileCtx), 0);


    		// ftp client
  
    		// ftp client发送文件
    		fp = fopen(filename, "rb");
    		int  readSize = 0;
    		int  sendTotalSize = 0;
    		char sendBuf[1024*1024] = {0};
    		while((readSize = fread(sendBuf, 1, sizeof(sendBuf), fp)) > 0)
    		{
      			send(client_fd, sendBuf, readSize, 0); 
      			sendTotalSize += readSize;
  
      			printf("sent %f KB\n", sendTotalSize/(1024.0));
    		}
    		fclose(fp);
  
    		if(sendTotalSize == totalSize) printf("Done!\n");
    		else printf("Error!");
    
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