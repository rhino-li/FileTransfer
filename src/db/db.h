#ifndef __FILE_DB_H__
#define __FILE_DB_H__

#include <mysql/mysql.h>
#include <string>

namespace filetrans
{
    class Mysql
    {
    private:
        MYSQL* m_sql;
    public:
        Mysql();
        ~Mysql();
        bool connection();
        bool update(std::string sql);
        MYSQL_RES* query(std::string sql);
        MYSQL* get_connection();
    };

    
}

#endif