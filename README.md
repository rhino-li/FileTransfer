# 文件传输服务器

## 项目介绍
文件传输系统，实现为客户端和服务器，主要功能包括文件的分片传输、下载、上传、暂停、和继续。
### 功能介绍
- 允许多个客户端连接服务器，服务器与每个客户端保持长连接，持续处理客户端业务需求，直至一方主动关闭连接为止。（线程池）
- 客户端可浏览服务器上的可选文件，可从服务器的可选文件中下载指定文件，可向服务器上传文件。（socket）
- 对于大文件，允许客户端在传输过程中主动暂停，在暂停后选择继续传输或取消此次传输。（分片）
- 文件传输时，携带文件内容的md5值，以校验文件的正确性。

## 各模块介绍
### socket模块
- 封装`sockaddr_in`和`sockaddr_in6`结构，实现为IPAddress类，支持创建IPv4/IPv6地址；
- 封装底层socket接口，包括`bind,listen,accept,connect,send,recv`等，实现为Socket类；
- 利用select特性，设置超时时间为0，持续监控socket fd并循环recv缓冲区字节，实现清空socket接收缓冲区的功能；
### 线程同步模块
- 基于`sem_t`封装信号量；
- 基于`pthread_mutex_t`封装互斥量；
- 基于`pthread_rwlock_t`封装读写锁；
### 线程池模块
- 实现为ThreadPool类；
- 在类的构造函数中预先创建n个线程，即为服务器的工作线程。每个线程的入口函数绑定为 持续从任务队列中取任务并执行 的操作。捕获执行任务时子线程出现的异常，避免影响服务进程。
- 当任务队列为空时，工作线程阻塞在条件变量处等待被唤醒。ThreadPool类提供向任务队列中添加任务的接口，每添加一个任务，使用条件变量唤醒一个线程；
- 析构函数中唤醒所有线程以完成剩余任务；
- 本项目中，每个任务即为服务器处理客户端的通用接口`handle_client()`；
### 服务器模块
- 实现为FileServer类，支持监听多个socket，创建线程池；
- 注册信号处理函数，忽略SIGPIPE信号，避免客户端异常退出时导致服务端工作线程异常，进而导致的服务进程异常；
- 服务器启动后，遍历socket监听数组，如果与客户端成功建立连接，则创建新的socket与客户端通信，并向线程池中添加新任务，同时绑定业务处理接口与通信socket。此时主线程持续监听socket，工作线程与客户端保持连接，持续处理客户端发起的业务需求，以此实现服务器的并发响应；
- 提供向客户端发送可选文件列表、发送文件、接收文件三个接口。
### 客户端模块
- 提供向服务器发送业务需求的接口，包括请求可选文件列表、下载文件、上传文件。
### 文件操作模块
- 定义用于文件传输的数据发送包格式和响应包格式。通信双方维护自己的FileCTX结构，以记录接收到的分片id和数据内容；
- 提供服务器端和客户端分片发送与分片接收的方法，默认分片大小为10*1024字节。不完整的分片保存在进程内存空间中，多线程操作时加锁保护。分片收齐后写入文件，并对比文件内容的md5与文件摘要中md5的一致性，不一致则报错终止进程。每一次发送/接收后（包括暂停、完毕），都清空通信双方的socket接收缓冲区，避免历史数据被接收。
- 其中，客户端可主动暂停/继续发送和接收过程，实现为开始发送/接收时，以非阻塞形式启动一个监控线程，持续监控终端输入，当收到终端的暂停信号时，暂停发送/接收，回收监控线程并通知服务端。后双方阻塞等待用户的继续或取消信号。
- 提供计算文件内容md5值的方法；

## 快速开始
- 目前是使用终端与用户通信，`./tests/test_server`为服务端启动程序，绑定ip为127.0.0.1，端口号为1234。`./tests/test_client`为客户端启动程序，启动后需用户输入服务器ip和端口号。
- 服务器和客户端集成为`./tests/test`，在`socketProgramming/tests/`目录下执行`./test`由用户选择启动客户端还是服务器，若服务器未启动时启动客户端将连接失败。
- 编译步骤：
    - `mkdir build;cd build;`
    - `make all -f ../Makefile` 
    - `make install -f ../Makefile` 编译生成静态库和动态库
- 使用：
    - `g++ -o ./tests/client ./tests/test_client.cpp -L./bin -lfiletrans_static -lpthread` 生成客户端
    - `g++ -o ./tests/server ./tests/test_server.cpp -L./bin -lfiletrans_static -lpthread` 生成服务端
    - `g++ ./tests/test.cpp -o ./tests/test` 
    - `cd ./tests; ./test` 启动

