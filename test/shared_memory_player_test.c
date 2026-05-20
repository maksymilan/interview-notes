#include <sys/mman.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SHARED_MEMORY_PLAYER_NO_MAIN
#define ENDLIMIT 10000
#define UPDATE_BATCH 128
#define TIMEOUT_MS 1000
#include "../questions/05_shared_memory/shared_memory_player.c"

#define ASSERT_TRUE(expr)                                                        \
    do {                                                                         \
        if (!(expr)) {                                                           \
            fprintf(stderr, "ASSERT_TRUE failed at %s:%d: %s\n", __FILE__,       \
                    __LINE__, #expr);                                            \
            exit(1);                                                             \
        }                                                                        \
    } while (0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ_INT(expected, actual)                                          \
    do {                                                                         \
        int expected_value = (expected);                                         \
        int actual_value = (actual);                                             \
        if (expected_value != actual_value) {                                    \
            fprintf(stderr, "ASSERT_EQ_INT failed at %s:%d: expected %d, got %d\n", \
                    __FILE__, __LINE__, expected_value, actual_value);           \
            exit(1);                                                             \
        }                                                                        \
    } while (0)

static shared_t* create_shared_data(void) {
    shared_t* data = mmap(
        NULL,
        sizeof(shared_t),
        PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_SHARED,
        -1,
        0
    );

    ASSERT_TRUE(data != MAP_FAILED);
    memset(data, 0, sizeof(*data));

    pthread_mutexattr_t attr;
    ASSERT_EQ_INT(0, pthread_mutexattr_init(&attr));
    ASSERT_EQ_INT(0, pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED));
    ASSERT_EQ_INT(0, pthread_mutex_init(&data->lock, &attr));
    ASSERT_EQ_INT(0, pthread_mutexattr_destroy(&attr));

    return data;
}

static void destroy_shared_data(shared_t* data) {
    ASSERT_EQ_INT(0, pthread_mutex_destroy(&data->lock));
    ASSERT_EQ_INT(0, munmap(data, sizeof(shared_t)));
}

static void test_timeout_deadline_can_expire_immediately(void) {
    set_global_timeout_ms(0);
    ASSERT_TRUE(timeout_expired());
}

static void test_child_modifies_shared_mapping_after_fork(void) {
    shared_t* data = create_shared_data();

    pid_t pid = fork();
    ASSERT_TRUE(pid >= 0);

    if (pid == 0) {
        child_process_modify(data);
        _exit(0);
    }

    int status = 0;
    ASSERT_EQ_INT(pid, waitpid(pid, &status, 0));
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ_INT(0, WEXITSTATUS(status));

    pthread_mutex_lock(&data->lock);
    int count = data->count;
    bool done = data->done;
    pthread_mutex_unlock(&data->lock);

    ASSERT_TRUE(done);
    ASSERT_EQ_INT(ENDLIMIT, count);

    destroy_shared_data(data);
}

static void test_parent_detects_finished_child_before_timeout(void) {
    shared_t* data = create_shared_data();
    set_global_timeout_ms(TIMEOUT_MS);

    pid_t pid = fork();
    ASSERT_TRUE(pid >= 0);

    if (pid == 0) {
        child_process_modify(data);
        _exit(0);
    }

    ASSERT_TRUE(parent_process_check(data));

    int status = 0;
    ASSERT_EQ_INT(pid, waitpid(pid, &status, 0));
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ_INT(0, WEXITSTATUS(status));

    destroy_shared_data(data);
}

static void test_parent_reports_timeout_when_data_is_not_done(void) {
    shared_t* data = create_shared_data();
    set_global_timeout_ms(0);

    ASSERT_FALSE(parent_process_check(data));

    pthread_mutex_lock(&data->lock);
    bool done = data->done;
    int count = data->count;
    pthread_mutex_unlock(&data->lock);

    ASSERT_FALSE(done);
    ASSERT_EQ_INT(0, count);

    destroy_shared_data(data);
}

int main(void) {
    test_timeout_deadline_can_expire_immediately();
    test_child_modifies_shared_mapping_after_fork();
    test_parent_detects_finished_child_before_timeout();
    test_parent_reports_timeout_when_data_is_not_done();

    printf("shared_memory_player_test: all tests passed\n");
    return 0;
}
