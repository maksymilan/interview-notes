#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef ENDLIMIT
#define ENDLIMIT 1000000000
#endif

#ifndef UPDATE_BATCH
#define UPDATE_BATCH 1000000
#endif

#ifndef TIMEOUT_MS
#define TIMEOUT_MS 3000
#endif

typedef struct SharedData
{
    pthread_mutex_t lock;
    int count;
    bool done;
} shared_t;

static struct timespec g_deadline;

static void set_global_timeout_ms(long timeout_ms) {
    clock_gettime(CLOCK_MONOTONIC, &g_deadline);

    g_deadline.tv_sec += timeout_ms / 1000;
    g_deadline.tv_nsec += (timeout_ms % 1000) * 1000000L;
    if (g_deadline.tv_nsec >= 1000000000L) {
        g_deadline.tv_sec++;
        g_deadline.tv_nsec -= 1000000000L;
    }
}

static bool timeout_expired(void) {
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec > g_deadline.tv_sec ||
           (now.tv_sec == g_deadline.tv_sec && now.tv_nsec >= g_deadline.tv_nsec);
}

static void sleep_for_poll_interval(void) {
    struct timespec interval = {
        .tv_sec = 0,
        .tv_nsec = 10 * 1000000L,
    };

    while (nanosleep(&interval, &interval) == -1 && errno == EINTR) {
    }
}

void child_process_modify(shared_t* data) {
    int pending = 0;

    for (int i = 0; i < ENDLIMIT; ++i) {
        pending++;
        if (pending == UPDATE_BATCH) {
            pthread_mutex_lock(&data->lock);
            data->count += pending;
            pthread_mutex_unlock(&data->lock);
            pending = 0;
        }
    }

    pthread_mutex_lock(&data->lock);
    data->count += pending;
    data->done = true;
    pthread_mutex_unlock(&data->lock);
}

bool parent_process_check(shared_t* data) {
    bool done = false;
    int count = 0;

    while (!timeout_expired()) {
        pthread_mutex_lock(&data->lock);
        done = data->done;
        count = data->count;
        pthread_mutex_unlock(&data->lock);

        if (done) {
            printf("child process finishes the work, count=%d\n", count);
            return true;
        }

        sleep_for_poll_interval();
    }

    pthread_mutex_lock(&data->lock);
    count = data->count;
    pthread_mutex_unlock(&data->lock);

    printf("child process fails to finish the work before timeout, count=%d\n", count);
    return false;
}

int run_shared_memory_player(void) {
    // mmap the shared memory
    shared_t* data = mmap(
        NULL, 
        sizeof(shared_t), 
        PROT_READ | PROT_WRITE, 
        MAP_ANONYMOUS | MAP_SHARED, 
        -1, 
        0
    );

    if (data == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }

    memset(data, 0, sizeof(*data));

    pthread_mutexattr_t attr; // mutex属性对象，用于高速pthread_mutex_init()，这个锁按照什么规则初始化
    pthread_mutexattr_init(&attr);// 初始化锁配置
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED); // 配置为进程间共享的锁
    pthread_mutex_init(&data->lock, &attr); // 按照配置初始化锁
    pthread_mutexattr_destroy(&attr); // 配置使用完了，可以销毁

    set_global_timeout_ms(TIMEOUT_MS);

    pid_t pid = fork();
    if (pid == 0) {
        // 子进程
        child_process_modify(data);
        _exit(0);
    } else if (pid < 0) {
        perror("fork failed");
        pthread_mutex_destroy(&data->lock);
        munmap(data, sizeof(shared_t));
        exit(1);
    } else {
        // 父进程
        bool finished = parent_process_check(data);
        if (!finished) {
            kill(pid, SIGTERM);
        }
        waitpid(pid, NULL, 0);
        pthread_mutex_destroy(&data->lock);
        munmap(data, sizeof(shared_t));
        return 0;
    }
}

#ifndef SHARED_MEMORY_PLAYER_NO_MAIN
int main(void) {
    return run_shared_memory_player();
}
#endif
