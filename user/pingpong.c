// pingpong.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[]){
    // 创建管道会获得一个长度为2的数组
    // 其中 0 为用于从管道读取数据的文件描述符，1 为用于向管道写入数据的文件描述符
    int pp2c[2], pc2p[2];
    pipe(pp2c);     // 创建parent 2 child的管道
    pipe(pc2p);     // 创建child 2 parent的管道

    int pid = fork();       // 创建子进程
    if (pid != 0){          // parent process
        write(pp2c[1], "r", 1);                     // 1.父进程发送请求'!'
        char buf;
        read(pc2p[0], &buf, sizeof(buf));           // 2.父进程发送完成后，开始等待子进程的回复
        printf("%d: received pong\n", getpid());    // 5.父进程处理请求
        wait(0);
    } else {                // child process
        char buf;
        read(pp2c[0], &buf, sizeof(buf));           // 3.子进程读取请求
        printf("%d: received ping\n", getpid());    // 4.子进程处理请求
        write(pc2p[1], &buf, 1);
    }
    exit(0);
}