#include <muduo/net/EventLoop.h>
#include<muduo/net/TcpServer.h>
#include<iostream>
#include<functional>
#include<string>
using namespace std;
using namespace muduo;
using namespace muduo::net;

namespace filetransfer
{
    class MuduoServer
    {
    public:
        MuduoServer(EventLoop* loop, const InetAddress &listenaddr, const string &name) 
        :m_server(loop,listenaddr,name),m_loop(loop)
        {
            m_server.setConnectionCallback(std::bind(&MuduoServer::onConnection,this,_1));
            m_server.setMessageCallback(std::bind(&MuduoServer::onMessage,this,_1,_2,_3));
            m_server.setThreadNum(4);
        }
        void start()
        {
            m_server.start();
        }
    private:
        //处理连接和断开的回调函数
        void onConnection(const TcpConnectionPtr & conn){
            std::cout << "FileServer - " << conn->peerAddress().toIpPort() << " -> "
                        << conn->localAddress().toIpPort() << " is "
                        << (conn->connected() ? "UP" : "DOWN");
            if(conn->connected()){
                cout<<conn->peerAddress().toIpPort()<<"->"<<conn->localAddress().toIpPort()<<"state:online"<<endl;
            }
            else{
                cout<<conn->peerAddress().toIpPort()<<"->"<<conn->localAddress().toIpPort()<<"state:offline"<<endl;
            }
        }
        //处理读写的回调函数
        void onMessage(const TcpConnectionPtr& conn,Buffer* buffer,Timestamp time){
            string buf=buffer->retrieveAllAsString();
            cout<<"recv data:"<<buf<<"time:"<<time.toFormattedString()<<endl;
            conn->send(buf);
        }
        TcpServer m_server;
        EventLoop * m_loop;
};

int main(){
    EventLoop loop;
    InetAddress addr("127.0.0.1",6000);
    MuduoServer server(&loop,addr,"chatserver");
    server.start();
    loop.loop();
    return 0;
}