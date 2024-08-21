#include <iostream>
#include "db.h"

namespace filetrans
{
    static std::string server = "127.0.0.1";
    static std::string user = "root";
    static std::string password = "123456";
    static std::string dbname = "FileTransfer";
    
    Mysql::Mysql()
    {
        m_sql = mysql_init(nullptr);
    }
    Mysql::~Mysql()
    {
        if(m_sql)
        {
            mysql_close(m_sql);
        }
    }
    bool Mysql::connection()
    {
        MYSQL* p = mysql_real_connect(m_sql,server.c_str(),user.c_str(),password.c_str(),dbname.c_str(),3306,nullptr,0);
        if(!p) std::cout<<mysql_error(m_sql)<<std::endl<<"mysql connect error."<<std::endl;
        return p;
    }
    bool Mysql::update(std::string sql)
    {
        if(mysql_query(m_sql,sql.c_str()))
        {
            std::cout<<mysql_error(m_sql)<<std::endl<<"mysql update error:"<<sql<<std::endl;
            return false;
        }
        return true;
    }
    MYSQL_RES* Mysql::query(std::string sql)
    {
        if(mysql_query(m_sql,sql.c_str()))
        {
            std::cout<<mysql_error(m_sql)<<std::endl<<"mysql query error:"<<sql<<std::endl;
            return nullptr;
        }
        return mysql_use_result(m_sql);
    }
    MYSQL* Mysql::get_connection()
    {
        return m_sql;
    }
}