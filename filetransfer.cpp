#include <iostream>
#include "include/file_client.h"
#include "include/file_server.h"

int main()
{
    std::string user;
    std::cout<<"Client(c) or Server(s)? ";
    std::cin>>user;
    while(1)
    {
        if(user=="c" || user=="s") break;
        else std::cout<<"c or s please. ";
    }
    if(user=="c")
    {
        filetrans::FileClient myclient;
        myclient.start_client();
    }
    else
    {
        std::cout<<"server default port 2024"<<std::endl;
        filetrans::FileServer::file_server_ptr myserver(new filetrans::FileServer());
        uint16_t open_port = 2024;
        filetrans::IPAddress::ipaddr_ptr myserver_ipaddr(new filetrans::IPAddress(4,INADDR_ANY,open_port));
        myserver->start_server(myserver_ipaddr);
    }
    return 0;
}



