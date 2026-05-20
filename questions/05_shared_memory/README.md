# mmap 共享内存示例

这个目录里的 `shared_memory_player.c` 演示了一个典型的 POSIX/Linux 用户态系统编程场景：

1. 父进程用 `mmap()` 创建一块匿名共享内存。
2. 共享内存中放一个 `struct SharedData`。
3. 父进程初始化这块内存和进程共享 mutex。
4. `fork()` 创建子进程。
5. 子进程修改共享 `struct`。
6. 父进程监控 `struct` 中的状态，并使用全局 deadline 做超时判断。
7. 父进程回收子进程、销毁 mutex、释放 mmap 内存。

## 涉及的头文件

```c
#include <sys/mman.h>   // mmap, munmap, PROT_READ, PROT_WRITE, MAP_SHARED, MAP_ANONYMOUS
#include <sys/types.h>  // pid_t
#include <sys/wait.h>   // waitpid, WIFEXITED, WEXITSTATUS
#include <errno.h>      // errno, EINTR
#include <stdbool.h>    // bool, true, false
#include <pthread.h>    // pthread_mutex_t, pthread_mutexattr_t
#include <signal.h>     // kill, SIGTERM
#include <stdio.h>      // printf, perror
#include <stdlib.h>     // exit
#include <string.h>     // memset
#include <time.h>       // clock_gettime, nanosleep, CLOCK_MONOTONIC
#include <unistd.h>     // fork, _exit
```

## mmap 相关知识

本示例使用：

```c
mmap(NULL,
     sizeof(shared_t),
     PROT_READ | PROT_WRITE,
     MAP_ANONYMOUS | MAP_SHARED,
     -1,
     0);
```

关键宏：

- `PROT_READ`：映射出来的内存可读。
- `PROT_WRITE`：映射出来的内存可写。
- `MAP_SHARED`：父子进程对这块映射的修改彼此可见。
- `MAP_ANONYMOUS`：不基于真实文件创建映射，适合父子进程之间的临时共享内存。

因为用了 `MAP_ANONYMOUS`，所以文件描述符传 `-1`，偏移量传 `0`。

如果使用 `MAP_PRIVATE`，父子进程会触发写时复制，子进程改 `count` 时父进程看不到最终结果。这里必须使用 `MAP_SHARED`。

## fork 之后为什么能共享

`fork()` 后，子进程继承父进程的虚拟地址空间。普通内存通常是 copy-on-write，而这块 mmap 内存被标记成了 `MAP_SHARED`，所以父子进程看到的是同一块共享映射。

这也是为什么代码里可以在 `fork()` 前初始化：

```c
shared_t* data = mmap(...);
memset(data, 0, sizeof(*data));
```

然后在 `fork()` 后父子进程继续使用同一个 `data` 指针。

## 进程间 mutex

普通 `pthread_mutex_t` 默认只能用于同一进程内的线程同步。要放进共享内存并跨进程使用，需要设置：

```c
pthread_mutexattr_t attr;
pthread_mutexattr_init(&attr);
pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
pthread_mutex_init(&data->lock, &attr);
pthread_mutexattr_destroy(&attr);
```

关键宏：

- `PTHREAD_PROCESS_PRIVATE`：默认值，只能进程内使用。
- `PTHREAD_PROCESS_SHARED`：允许多个进程通过共享内存里的 mutex 同步。

本示例中 `count` 和 `done` 都是父子进程共享数据，因此读写时使用同一把锁保护。

## timeout 的设计

示例里使用全局变量保存截止时间：

```c
static struct timespec g_deadline;
```

初始化 deadline：

```c
set_global_timeout_ms(TIMEOUT_MS);
```

判断是否超时：

```c
timeout_expired();
```

这里使用 `CLOCK_MONOTONIC`，而不是系统墙上时间。原因是系统时间可能被 NTP 或人工调整，`CLOCK_MONOTONIC` 更适合做超时、耗时统计和 deadline 判断。

## _exit 和 exit

子进程完成工作后调用：

```c
_exit(0);
```

`_exit()` 会直接退出当前进程，不执行 C 标准库层面的清理，也不会 flush stdio 缓冲。`fork()` 后的子进程常用 `_exit()`，可以避免重复 flush 父进程继承来的用户态缓冲区。

父进程出错或主流程返回时可以使用 `exit()` 或 `return`。

## 父进程的资源回收顺序

父进程负责最终清理：

```c
waitpid(pid, NULL, 0);
pthread_mutex_destroy(&data->lock);
munmap(data, sizeof(shared_t));
```

顺序上先 `waitpid()`，确保子进程已经退出，不再访问共享内存。然后销毁 mutex，最后 `munmap()` 释放映射。

如果父进程检测到超时，本示例会：

```c
kill(pid, SIGTERM);
waitpid(pid, NULL, 0);
```

这样可以避免子进程继续运行，而父进程已经释放共享内存。

## 测试样例

测试文件在：

```text
test/shared_memory_player_test.c
```

覆盖了这些行为：

1. `set_global_timeout_ms(0)` 后 timeout 会立即过期。
2. 子进程通过 `fork()` 修改 mmap 共享内存后，父进程能看到 `done=true` 和正确的 `count`。
3. 父进程能在 timeout 前检测到子进程完成。
4. 当共享数据一直没有完成时，父进程能报告 timeout。

测试中会覆盖示例里的宏：

```c
#define ENDLIMIT 10000
#define UPDATE_BATCH 128
#define TIMEOUT_MS 1000
```

这样测试不会跑完整的 `1000000000` 次循环，速度更适合 CTest。

## 构建和运行

从仓库根目录执行：

```bash
cmake -S . -B build
cmake --build build --target shared_memory_player shared_memory_player_test
ctest --test-dir build --output-on-failure
```

也可以单独运行示例：

```bash
./build/shared_memory_player
```

在 Linux 上手动编译时通常需要链接 pthread：

```bash
cc -Wall -Wextra -std=c11 shared_memory_player.c -pthread -o shared_memory_player
```
