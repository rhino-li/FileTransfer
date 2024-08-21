#include "../include/file_server.h"

int main()
{
    filetrans::FileServer::file_server_ptr myserver(new filetrans::FileServer());
    uint16_t open_port = 1234;
    filetrans::IPAddress::ipaddr_ptr myserver_ipaddr(new filetrans::IPAddress(4,INADDR_ANY,open_port));
    myserver->start_server(myserver_ipaddr);
    return 0;
}