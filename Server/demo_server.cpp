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

using namespace std;
 
#define portnum 12345
#define FILE_SIZE 500 
#define BUFFER_SIZE 1024

struct fileContext
{
	char filename[50];
	size_t filesize;
};

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

		while(1)
		{
			char status[4] = {0};
			recv(connect_fd,status,sizeof(status),0);
			std::cout<<"recv status from client:"<<status<<std::endl;
			int choice = atoi(status);

			if(choice==1)
			{
				vector<string> fileNames;
      			string path("file");
      			getFileNames(path, fileNames);
      			char testname[500]={0};
      			for (const auto &ph : fileNames) 
				{
        			strcat(testname ,ph.c_str());
        			strcat(testname,";");
	    		}
      			// cout<<testname<<endl;
      			send(connect_fd, testname,sizeof(testname),0);
				break;
			}
			else if (choice==2)
			{
				fileContext fileCtx;
      			size_t fileSize = 0;
      			char filename[50] = {0};
      			// 接收ftp client发送过来的文件长度与文件名 
      			char sizeFileStr[20] = {0};
      			recv(connect_fd, (char*)&fileCtx, sizeof(fileCtx), 0);
      
      			fileSize = fileCtx.filesize;
      			// int fileSize = atoi(sizeFileStr);

      			strcpy(filename, fileCtx.filename);
    
      			// 接收ftp client发送的文件并保存
      			char recvBuf[1024*1024] = {0};  
      			int recvTotalSize = 0;
      			char file[50] = "./file/";
      			strcat(file,filename);
      			FILE *fp = fopen(file, "wb");
      			while(recvTotalSize < fileSize)
      			{
        			int recvSize = recv(connect_fd, recvBuf, sizeof(recvBuf), 0);
        			recvTotalSize += recvSize;
        			printf("received %f KB\n", recvTotalSize/(1024.0));
        			fwrite(recvBuf, 1, recvSize, fp);
      			}
      			fclose(fp);
      			if(recvTotalSize == fileSize) printf("Done!\n");
      			else printf("Error!");
			}
			else if(choice==3)
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
			else if(choice==4)
			{
				break;
			}
			else{
				std::cout<<"please input 1~4"<<std::endl;
			}
			
		}
		
		// int pthread_id;
		// int ret = pthread_create((pthread_t *)&pthread_id,NULL,net_thread,(void *)&new_fd);
		// if(-1==ret)
		// {
		// 	perror("pthread_create");
		// 	close(new_fd);
		// 	continue;
		// }
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