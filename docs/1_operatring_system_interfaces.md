## 1. 操作系统接口

[toc]

### 1.1 内存和进程

![img](/home/sjh/Documents/Markdown_Note/MIT6.S801.assets/1_1.png)

### 1.2 I/O与文件描述符

在 Unix/Linux 系统中，文件描述符是一个整数，用于指向一个已打开的文件或输入/输出流（例如文件、管道、网络套接字等）。通常，文件描述符 0、1、2 分别对应标准输入（`stdin`）、标准输出（`stdout`）和标准错误（`stderr`）



### 1.3 Unix系统常用的system call

`fork()` 是 Unix/Linux 操作系统中一个关键的系统调用，用于创建一个新进程。这个新进程是调用 `fork()` 的进程的副本，称为子进程，而原来的进程称为父进程。

```c
pid_t fork(void);
```

`fork()` 的功能

- 当一个进程调用 `fork()` 时，操作系统创建一个新的进程，该进程几乎与父进程完全相同。子进程继承了父进程的地址空间、文件描述符、环境变量等，但它们是独立的进程。
- 子进程从 `fork()` 调用返回后开始执行，与父进程并行运行。

 `fork()` 的返回值

- ```
  fork()
  ```

   在父进程和子进程中返回不同的值：

  - 在父进程中，`fork()` 返回子进程的进程 ID（一个正整数）。
  - 在子进程中，`fork()` 返回 0。
  - 如果 `fork()` 失败（例如系统资源不足），它在父进程中返回 -1，并设置 `errno` 来指示错误类型。

`fork()` 的工作机制

- 进程复制：`fork()` 会复制父进程的内存空间，包括代码段、堆、栈、文件描述符表等。因此，父子进程的代码是相同的，但它们拥有独立的内存空间。
- 进程 ID：每个进程都有一个唯一的进程 ID（PID）。父进程的 PID 通过 `getpid()` 获得，而子进程的 PID 是 `fork()` 的返回值。

一个简单的示例，如何创建子进程并且区分父进程

```c
#include <stdio.h>
#include <unistd.h>

int main(){
    int pid, status;
    if (pid == 0){
        char *argv[] = {"echo", "THIS", "IS", "ECHO", 0};
        exec("echo", argv);
        printf("exec failed!\n");
        exit(1);
    } else {
        printf("parent waiting!\n");
        wait(&status);
        printf("the child exited with status %d\n", status);
    }
    exit(0);
}
```

`dup` 是一个用于复制文件描述符的系统调用，在 Unix/Linux 系统中非常重要。它可以创建一个新的文件描述符，该文件描述符与现有的文件描述符指向相同的文件或资源。这在实现文件重定向和管理文件描述符时非常有用。

```c
int dup(int oldfd);
```

- 参数：
  - `oldfd`: 需要复制的现有文件描述符。
- 返回值：
  - 成功时，`dup` 返回一个新的文件描述符（这是系统中当前可用的最小的未被占用的文件描述符），该文件描述符与 `oldfd` 共享同一个文件表项。
  - 失败时，返回 -1，并设置 `errno` 以指示错误类型。

`dup` 的功能

- `dup` 将 `oldfd` 复制为一个新的文件描述符，新旧文件描述符共享同一文件表项（file table entry），因此它们拥有相同的文件偏移量、文件状态标志（如只读、只写等）和访问模式。
- 由于新文件描述符指向与旧文件描述符相同的文件或资源，因此对任一文件描述符执行的读写操作都会影响另一个文件描述符。

一个简单的例子，展示如何使用`dup`复制文件描述符`fd`

```c
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main() {
    int fd = open("example.txt", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    int new_fd = dup(fd);
    if (new_fd < 0) {
        perror("dup");
        close(fd);
        return 1;
    }
    printf("Original file descriptor: %d\n", fd);
    printf("Duplicated file descriptor: %d\n", new_fd);

    // 关闭文件描述符
    close(fd);
    close(new_fd);
    return 0;
}
```



#### 文件操作系统调用

| 系统调用 | 描述                                 |
| -------- | ------------------------------------ |
| `open`   | 打开文件或设备，并返回文件描述符     |
| `close`  | 关闭文件描述符                       |
| `read`   | 从文件描述符中读取数据               |
| `write`  | 向文件描述符中写入数据               |
| `lseek`  | 调整文件描述符的读写位置             |
| `stat`   | 获取文件的状态信息                   |
| `fstat`  | 获取与文件描述符关联的文件的状态信息 |
| `lstat`  | 获取符号链接的状态信息               |
| `dup`    | 复制一个文件描述符                   |
| `dup2`   | 复制一个文件描述符到指定的文件描述符 |
| `unlink` | 删除一个文件                         |
| `mkdir`  | 创建一个目录                         |
| `rmdir`  | 删除一个目录                         |
| `rename` | 重命名文件或目录                     |
| `chmod`  | 更改文件权限                         |
| `chown`  | 更改文件所有者                       |
| `umask`  | 设置文件模式创建掩码                 |
| `access` | 检查文件的可访问权限                 |
| `pipe`   | 创建一个管道（用于进程间通信）       |

#### 进程管理系统调用

| 系统调用  | 描述                     |
| --------- | ------------------------ |
| `fork`    | 创建一个子进程           |
| `execve`  | 执行一个新程序           |
| `exit`    | 终止进程并返回状态       |
| `wait`    | 等待子进程终止           |
| `waitpid` | 等待特定的子进程终止     |
| `getpid`  | 获取当前进程的进程 ID    |
| `getppid` | 获取父进程的进程 ID      |
| `kill`    | 向进程发送信号           |
| `getuid`  | 获取用户 ID              |
| `getgid`  | 获取组 ID                |
| `setuid`  | 设置用户 ID              |
| `setgid`  | 设置组 ID                |
| `nice`    | 改变进程的优先级         |
| `alarm`   | 设置一个闹钟信号         |
| `pause`   | 暂停进程，直到接收到信号 |



