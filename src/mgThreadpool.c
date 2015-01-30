#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mgStd/mglist.h>
#include "mgThreadpool.h"

/*
 *空闲任务缓存初始化
 */
static void mgThreadpool_idle_task_init(mgThreadpool *thread_pool)
{
    if (thread_pool->idle_task_count >= MGTHREADPOOL_DEFAULT_MAX_TASK)
        return; 

    if (thread_pool->idle_task_num >= thread_pool->idle_task_count && thread_pool->idle_task_count > 0)
    {
        pthread_mutex_lock(&thread_pool->idle_task_mutex);
        thread_pool->idle_task_count *= 2;
        thread_pool->idle_task = (mgThreadpool_task **)realloc(thread_pool->idle_task, 
                                    sizeof(mgThreadpool_task *) * thread_pool->idle_task_count);
        pthread_mutex_unlock(&thread_pool->idle_task_mutex);
    }
    else
    {
        pthread_mutex_lock(&thread_pool->idle_task_mutex);
        thread_pool->idle_task_count = 50;
        thread_pool->idle_task = 
            (mgThreadpool_task **)calloc(sizeof(mgThreadpool_task*), thread_pool->idle_task_count);
        pthread_mutex_unlock(&thread_pool->idle_task_mutex);
    }
    return;
}

/*
 *从空闲任务链表缓存中获取空闲任务指针
 */
static mgThreadpool_task *mgThreadpool_get_idle_task(mgThreadpool *thread_pool)
{
    mgThreadpool_task *idle_task = NULL;
    if (thread_pool->idle_task_num <= 0)
    {
        idle_task = (mgThreadpool_task *)malloc(sizeof(mgThreadpool_task));
    }
    else
    {
        pthread_mutex_lock(&thread_pool->idle_task_mutex);
        idle_task = thread_pool->idle_task[--thread_pool->idle_task_num];        
        pthread_mutex_unlock(&thread_pool->idle_task_mutex);
    }
    if (idle_task)
    {
        idle_task->task_function = NULL;
        idle_task->task_arg = NULL;
        mglist_init(&(idle_task->list_node));
    }
    return idle_task;
}

/*
 *将已执行完毕的任务添加到空闲任务链表缓存中去
 */
static void mgThreadpool_put_idle_task(mgThreadpool *thread_pool, mgThreadpool_task *task)
{
    if (!task)
        return;
    task->task_function = NULL;
    task->task_arg = NULL;
    mglist_init(&(task->list_node));

    if (thread_pool->idle_task_num >= MGTHREADPOOL_DEFAULT_MAX_TASK)
    {
        free(task);
        return;
    }

    if (thread_pool->idle_task_num >= thread_pool->idle_task_count)
        mgThreadpool_idle_task_init(thread_pool);
    
    pthread_mutex_lock(&thread_pool->idle_task_mutex);
    thread_pool->idle_task[thread_pool->idle_task_num++] = task;
    pthread_mutex_unlock(&thread_pool->idle_task_mutex);
    return;
}

/*
 *清除空闲任务链表缓存
 */
static void mgThreadpool_idle_task_finish(mgThreadpool *thread_pool)
{
    int i = 0;
    if (thread_pool->idle_task && thread_pool->idle_task_num > 0)
    {
        for (i = 0; i < thread_pool->idle_task_num; i++)
        {
            if (thread_pool->idle_task[i])
            {
                free(thread_pool->idle_task[i]);
                thread_pool->idle_task[i] = NULL;
            }
        }
        free(thread_pool->idle_task);
        thread_pool->idle_task = NULL;
    }
    return;
}

/*
 *从链表中获取任务指针
 */
static mgThreadpool_task *get_task_from_list(mglist_t *head)
{
    if (mglist_empty_careful(head))
        return NULL;
    mgThreadpool_task *task = mglist_entry(head->prev, mgThreadpool_task, list_node);
    mglist_del_init(head->prev);
    return task;
}

/*
 *将任务添加到链表中去
 */
static void put_task_from_list(mglist_t *head, mgThreadpool_task *task)
{
    mglist_add(&(task->list_node), head);
    return;
}

/*
 *线程运行函数
 */
static void* mgThreadpool_thread_run(void *arg)
{
    mgThreadpool *thread_pool = (mgThreadpool *)arg;
    mgThreadpool_thread *thread = NULL;
    mgThreadpool_task *task = NULL;
    pthread_t self_id = pthread_self();
#ifdef _MG_THREAD_POOL_CPU_AFFINITY_
    sched_setaffinity(0, sizeof(thread_pool->cpuset), &(thread_pool->cpuset));
#endif
    int i = 0;
    for (i = 0; i < thread_pool->thread_num; i++)
    {
        if (self_id == thread_pool->thread[i].thread_id)
        {
            thread = &(thread_pool->thread[i]);
            break;
        }
    }
    if (!thread)
        return;
    while(!thread_pool->close_flag)
    {
        pthread_mutex_lock(&(thread_pool->task_process_mutex));
        while(!thread_pool->close_flag && 
                ( mglist_empty_careful(&(thread_pool->long_task_queue)) && 
                  mglist_empty_careful(&(thread->thread_task_list))))
        {
            pthread_cond_wait(&(thread_pool->task_process_cond), &(thread_pool->task_process_mutex));
        }
        pthread_mutex_unlock(&(thread_pool->task_process_mutex));
        if (mglist_empty_careful(&(thread->thread_task_list)))//优先处理短任务，如果短任务为空，则处理长任务
        {
            thread->is_busy = 1;
            pthread_mutex_lock(&(thread_pool->long_task_mutex));
            task = get_task_from_list(&(thread_pool->long_task_queue));
            if (task != NULL) thread_pool->long_task_num--;
            pthread_mutex_unlock(&(thread_pool->long_task_mutex));
        }
        else
        {
            pthread_mutex_lock(&(thread->thread_mutex));
            task = get_task_from_list(&(thread->thread_task_list));
            if (task != NULL) thread->thread_task_num--;
            pthread_mutex_unlock(&(thread->thread_mutex));
        }
        if (task)
        {
            (*(task->task_function))(task->task_arg);
            mgThreadpool_put_idle_task(thread_pool, task);
        }
        thread->is_busy = 0;
    }
    return NULL;
}

/*
 *获取任务最少的线程
 */
static mgThreadpool_thread *get_least_task_thread(mgThreadpool *thread_pool)
{
    mgThreadpool_thread *least_task_thread = &(thread_pool->thread[0]);
    int i = 0;
    for (i = 1; i < thread_pool->thread_num; i++)
    {
        if (thread_pool->thread[i].thread_task_num < least_task_thread->thread_task_num &&
                (!thread_pool->thread[i].is_busy))
            least_task_thread = &(thread_pool->thread[i]);
    }
    return least_task_thread;
}

/*
 *线程池初始化
 */
mgThreadpool* mgThreadpool_init(int thread_num, int max_task_num, int cpu_id)
{
    mgThreadpool *thread_pool = (mgThreadpool *)malloc(sizeof(mgThreadpool));
    int i;
    if (thread_pool)
    {
        thread_pool->thread_num = thread_num;
        thread_pool->max_task_num = max_task_num;
        
        int cpu_num = (int)(sysconf(_SC_NPROCESSORS_CONF));
        if (thread_num < cpu_num)
            thread_pool->thread_num = cpu_num;

        if (thread_pool->thread_num > MGTHREADPOOL_MAX_THREAD)
            thread_pool->thread_num = MGTHREADPOOL_MAX_THREAD;

        if (max_task_num <= 0)
            thread_pool->max_task_num = MGTHREADPOOL_DEFAULT_MAX_TASK;

        mglist_init(&thread_pool->long_task_queue);
        thread_pool->long_task_num = 0;

        pthread_mutex_init(&(thread_pool->long_task_mutex), NULL);
        pthread_mutex_init(&(thread_pool->task_process_mutex), NULL);
        pthread_mutex_init(&(thread_pool->idle_task_mutex), NULL);

        pthread_cond_init(&(thread_pool->task_process_cond), NULL);

        thread_pool->idle_task_count = 0;
        mgThreadpool_idle_task_init(thread_pool);

        thread_pool->close_flag = 0;

#ifdef _MG_THREAD_POOL_CPU_AFFINITY_
        CPU_ZERO(&(thread_pool->cpuset));
        CPU_SET(cpu_id%cpu_num, &(thread_pool->cpuset));
#endif

        thread_pool->thread = 
            (mgThreadpool_thread *)malloc(sizeof(mgThreadpool_thread) * thread_pool->thread_num);
        if (!thread_pool->thread)
        {
            mgThreadpool_idle_task_finish(thread_pool);
            free(thread_pool);
            thread_pool = NULL;
            return NULL;
        }
        for (i = 0; i < thread_pool->thread_num; i++)
        {
            thread_pool->thread[i].is_busy = 0;
            thread_pool->thread[i].thread_task_num = 0;
            pthread_mutex_init(&(thread_pool->thread[i].thread_mutex), NULL);
            mglist_init(&(thread_pool->thread[i].thread_task_list));
            pthread_create(&(thread_pool->thread[i].thread_id), NULL, 
                    mgThreadpool_thread_run, (void *)(thread_pool));
        }
        
    }
    return thread_pool;
}

int mgThreadpool_add_task(mgThreadpool *thread_pool, mgThreadpool_task_fun *fun, void *arg, int priority_level)
{
    mgThreadpool_task *task = mgThreadpool_get_idle_task(thread_pool);
    if (!thread_pool || !task )
        return -1;
    task->task_function = fun;
    task->task_arg = arg;
    mglist_init(&task->list_node);
    
    if (priority_level)//0为长任务, 非0为短任务
    {
        mgThreadpool_thread *thread = get_least_task_thread(thread_pool);
        if (thread->thread_task_num < thread_pool->max_task_num)
        {
            pthread_mutex_lock(&(thread->thread_mutex));
            put_task_from_list(&(thread->thread_task_list), task);
            thread->thread_task_num++;
            pthread_mutex_unlock(&(thread->thread_mutex));
        }
        else
            return -2;
    }
    else
    {
        if (thread_pool->long_task_num < thread_pool->max_task_num)
        {
            pthread_mutex_lock(&(thread_pool->long_task_mutex));
            put_task_from_list(&(thread_pool->long_task_queue), task);
            thread_pool->long_task_num++;
            pthread_mutex_unlock(&(thread_pool->long_task_mutex));
        }
        else
            return -3;
    }
    pthread_mutex_lock(&(thread_pool->task_process_mutex));
    pthread_cond_broadcast(&(thread_pool->task_process_cond));
    pthread_mutex_unlock(&(thread_pool->task_process_mutex));
    return 0;
}

void mgThreadpool_finish(mgThreadpool *thread_pool)
{
    if (!thread_pool || thread_pool->close_flag)
        return;
    int i = 0, j = 0; 
    mglist_t *pos = NULL, *n = NULL;
    mgThreadpool_task *task = NULL;

    //通知所有线程结束处理
    thread_pool->close_flag = 1;
    pthread_mutex_lock(&(thread_pool->task_process_mutex));
    pthread_cond_broadcast(&(thread_pool->task_process_cond));
    pthread_mutex_unlock(&(thread_pool->task_process_mutex));
    
    for ( i = 0; i < thread_pool->thread_num; i ++)
    {
        //等待所有线程退出
        pthread_join(thread_pool->thread[i].thread_id, NULL);
    }
   
    printf_mgthread_pool(thread_pool);

    //清理空闲任务链表缓存中的任务
    if (thread_pool->idle_task)
        mgThreadpool_idle_task_finish(thread_pool);
    
    
    //如何长任务中的任务没有处理完，在销毁时必须将其清空
    mglist_for_each_safe(pos, n, &(thread_pool->long_task_queue))
    {
        task = mglist_entry(pos, mgThreadpool_task, list_node);
        free(task);
    }
    
    //清理所有线程中的短任务
    if (thread_pool->thread)
    {
        for ( i=0; i < thread_pool->thread_num; i++)
        {
            mglist_for_each_safe(pos, n, &(thread_pool->thread[i].thread_task_list))
            {
                task = mglist_entry(pos, mgThreadpool_task, list_node);
                free(task);
            }
            pthread_mutex_destroy(&(thread_pool->thread[i].thread_mutex));
        }
        free(thread_pool->thread);
        thread_pool->thread = NULL;
    }
    pthread_mutex_destroy(&(thread_pool->long_task_mutex));
    pthread_mutex_destroy(&(thread_pool->idle_task_mutex));
    pthread_mutex_destroy(&(thread_pool->task_process_mutex));
    pthread_cond_destroy(&(thread_pool->task_process_cond));
    free(thread_pool);
    thread_pool = NULL;
    return;
}

void printf_mgthread_pool(mgThreadpool *thread_pool)
{
    printf("=============mgThreadpool===============\n");
    printf("== mgThreadpool : %p \n", thread_pool);
    if (thread_pool)
    {
        printf("== thread pool num : %d \n", thread_pool->thread_num);
        printf("== thread pool max_task_num : %d\n", thread_pool->max_task_num);
        printf("== thread pool long_task_num : %d\n", thread_pool->long_task_num);
        printf("== thread pool idle_task_count : %d\n", thread_pool->idle_task_count); 
        printf("== thread pool idle_task_num : %d\n", thread_pool->idle_task_num); 
        int i = 0;
        for ( i = 0; i < thread_pool->thread_num; i ++)
        {
            printf("== thread pool %d thread - -:\n", i);
            printf("  -- thread id : %u - -\n", thread_pool->thread[i].thread_id);
            printf("  -- thread is busy : %s - -\n", thread_pool->thread[i].is_busy?"YES":"NO");
            printf("  -- thread task num : %d - -\n", thread_pool->thread[i].thread_task_num);
        }
        printf("=======================================\n\n");
    }
}

//sysconf
//getrlimit
