#include <iostream>
#include <stdlib.h>

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
    char* cmd;
    if(user=="c")
    {
        cmd = "./client";
    }
    else
    {
        cmd = "./server";
    }
    system(cmd);
    return 0;
    
}



