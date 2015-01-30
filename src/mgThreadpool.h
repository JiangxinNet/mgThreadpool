#ifndef __MG_THREAD_POOL_H_
#define __MG_THREAD_POOL_H_

//#define _MG_THREAD_POOL_CPU_AFFINITY_

#ifdef _MG_THREAD_POOL_CPU_AFFINITY_
#ifndef __USE_GNU//_GNU_SOURCE
#define __USE_GNU//_GNU_SOURCE 
#endif
#endif

#include <sched.h>
#include <pthread.h>
#include <mgStd/mglist.h>

#define MGTHREADPOOL_MAX_THREAD             256
#define MGTHREADPOOL_DEFAULT_MAX_TASK       10000

typedef void mgThreadpool_task_fun(void *);

typedef struct mgThreadpool_task
{
    mgThreadpool_task_fun   *task_function;
    void                    *task_arg;
    mglist_t                list_node;
}mgThreadpool_task;

typedef struct mgThreadpool_thread
{
    pthread_t               thread_id;
    int                     is_busy;
    int                     thread_task_num;
    mglist_t                thread_task_list;
    pthread_mutex_t         thread_mutex;
}mgThreadpool_thread;

typedef struct mgThreadpool
{
    int                     thread_num;             //线程数

    int                     max_task_num;           //任务处理最大数
    int                     long_task_num;          //长任务数
    mglist_t                long_task_queue;        //长任务队列
    pthread_mutex_t         long_task_mutex;        //长任务锁
    
    pthread_cond_t          task_process_cond;      //任务处理变量，用来唤醒处理线程
    pthread_mutex_t         task_process_mutex;     //任务处理变量锁
    
    mgThreadpool_thread     *thread;                //处理线程

    mgThreadpool_task       **idle_task;            //空闲任务缓存
    int                     idle_task_count;        //空闲任务总量
    int                     idle_task_num;          //剩余空闲任务数
    pthread_mutex_t         idle_task_mutex;        //空闲任务锁

    int                     close_flag;             //关闭标志
#ifdef _MG_THREAD_POOL_CPU_AFFINITY_
    cpu_set_t               cpuset;
#endif

}mgThreadpool;

mgThreadpool* mgThreadpool_init(int thread_num, int max_task_num, int cpu_id);

int mgThreadpool_add_task(mgThreadpool *thread_pool, mgThreadpool_task_fun *fun, void *arg, int priority_level);

void mgThreadpool_finish(mgThreadpool *thread_pool);

void printf_mgthread_pool(mgThreadpool *thread_pool);

#endif

