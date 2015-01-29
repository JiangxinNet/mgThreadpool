#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#include "mgThreadpool.h"

pthread_mutex_t long_task_metux = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t short_task_metux = PTHREAD_MUTEX_INITIALIZER;

int long_task_num = 0;
int short_task_num = 0;

void long_task_test(void *arg)
{
    mgThreadpool *tp = (mgThreadpool *)arg;
    pthread_mutex_lock(&long_task_metux);
    printf("I am long task, thread is %u, times : %d\n", pthread_self(), ++long_task_num);
    pthread_mutex_unlock(&long_task_metux);
    printf_mgthread_pool(tp);
    printf("long task end!\n\n");
    sleep(2);
    return;
}

void short_task_test(void *arg)
{
    mgThreadpool *tp = (mgThreadpool *)arg;
    pthread_mutex_lock(&short_task_metux);
    printf("I am short task, me thread is %u, times %d:\n", pthread_self(), ++short_task_num);
    pthread_mutex_unlock(&short_task_metux);
    printf_mgthread_pool(tp);
    printf("short task end!\n\n");
    sleep(1);
    return;
}

int main()
{
    mgThreadpool *tp = mgThreadpool_init(4, 0);
    printf_mgthread_pool(tp);
    int i = 0;
    for (i = 0; i < 10; i ++)
    {
        if (i % 3 == 0)
        {
            mgThreadpool_add_task(tp, &(long_task_test),(void *)tp, 0);
        }
        mgThreadpool_add_task(tp, &(short_task_test), (void *)tp, 1);
    }
    printf_mgthread_pool(tp);
    sleep(5);
    mgThreadpool_finish(tp);
    return 0;
}
