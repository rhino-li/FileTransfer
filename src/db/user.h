#ifndef __FILE_USER_H__
#define __FILE_USER_H__

#include <string>
#include "db.h"

namespace filetrans
{
    class User
    {
    private:
        std::string username;
        std::string passwd;
    public:
        User(std::string name="",std::string p=""):username(name),passwd(p){};
        void set_name(std::string name){username=name;};
        void set_pwd(std::string p){passwd=p;};
        std::string get_name(){return username;};
        std::string get_pwd(){return passwd;};
    };

    // 对User表的操作
    class UserOP
    {
    public:
        UserOP();
        bool insert_user(User& user); // 注册用户
        User query_user_info(std::string name); // 根据用户name查找用户信息
        bool delete_user(User user); // 注销用户
    };
    
}


#endif