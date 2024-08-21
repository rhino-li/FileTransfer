#include "user.h"

namespace filetrans
{

    UserOP::UserOP()
    {
        char sql[500] = "SHOW TABLES LIKE 'user'";
        Mysql mysql; 
        if (mysql.connection())
        {
            MYSQL_RES *res = mysql.query(sql);
            bool exists = (mysql_fetch_row(res)!=nullptr);
            mysql_free_result(res);
            if (!exists)
            {
                char sqlCreate[200] = "CREATE TABLE user ( "
                           "username VARCHAR(255) NOT NULL, "
                           "password VARCHAR(255) NOT NULL, "
                           "PRIMARY KEY (username) "
                           ") ENGINE=InnoDB";
                if(!mysql.update(sqlCreate))
                {
                    printf("create user table error.");
                    exit(1);
                }
            }
        }
    }
    bool UserOP::insert_user(User& user)
    {
        char sql[500] = {0};
        sprintf(sql, "INSERT INTO user(username,password) values('%s','%s')", user.get_name().c_str(), user.get_pwd().c_str());
        Mysql mysql; 
        if (mysql.connection())
        {
            if (mysql.update(sql))
            {                                                      
                // user.set_id(mysql_insert_id(mysql.get_connection())); 
                return true;
            }
        }
        return false;
    }
    User UserOP::query_user_info(std::string name)
    {
        char sql[500] = {0};
        sprintf(sql, "SELECT * FROM user WHERE username='%s'", name.c_str());
        Mysql mysql;
        if (mysql.connection())
        {
            MYSQL_RES *res = mysql.query(sql);
            if (res != nullptr)
            {
                MYSQL_ROW row = mysql_fetch_row(res);
                if (row != nullptr)
                {
                    User user(row[0],row[1]); 
                    return user;
                }
            }
        }
        return User();
    } 
    bool UserOP::delete_user(User user)
    {
        char sql[500] = {0};
        sprintf(sql, "DELETE FROM user WHERE username='%s'", user.get_name().c_str());
        Mysql mysql; 
        if (mysql.connection())
        {
            if (mysql.update(sql)) return true;
        }
        return false;
    }
}