#include <cmath>
#include <iostream>
#include <istream>
#include <string>
#include <sstream>
#include <vector>

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
int main()
{
    char filepath[50] = "./client_file/test.png";
    auto ret = split(filepath,'/');
    for(auto &i:ret)
    {
        std::cout<<i<<std::endl;
    }
    std::string filename = ret.back();
    std::cout<<filename<<std::endl;
    return 0;
}