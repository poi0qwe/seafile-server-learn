/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* 多线程任务管理 */
// 通过管道来表示一个线程是否完成；通过libevent发起完成事件

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <event2/event.h>
#include <event2/event_compat.h>
#else
#include <event.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define MAX_THREADS 50 // 最大线程数
#define MAX_IDLE_THREADS 10 // 最大空闲线程数

#include "utils.h"

#include "job-mgr.h"

struct _CcnetJob { // 任务结构体
    CcnetJobManager *manager; // 所属的任务管理器

    int             id; // 任务id
    ccnet_pipe_t    pipefd[2]; // 任务管道

    JobThreadFunc   thread_func; // 用户任务函数
    JobDoneCallback done_func;  /* called when the thread is done */ // 完成函数
    void           *data; // 用户数据，传递给任务函数

    /* the done callback should only access this field */
    void           *result; // 保存任务函数返回的结果
};


void
ccnet_job_manager_remove_job (CcnetJobManager *mgr, int job_id); // 移除任务

static void
job_thread_wrapper (void *vdata, void *unused) // 任务线程包装函数
{
    CcnetJob *job = vdata;

    job->result = job->thread_func (job->data); // 执行thread_func，然后保存返回值
    if (pipewriten (job->pipefd[1], "a", 1) != 1) { // 向管道写端写入一个字节，表示完成
        g_warning ("[Job Manager] write to pipe error: %s\n", strerror(errno));
    }
}

static void
job_done_cb (evutil_socket_t fd, short event, void *vdata) // 任务线程完成函数
{
    CcnetJob *job = vdata;
    char buf[1];

    if (pipereadn (job->pipefd[0], buf, 1) != 1) { // 从管道读端读一个字符；如果管道无数据，则阻塞
        g_warning ("[Job Manager] read pipe error: %s\n", strerror(errno));
    } // 如果读到了数据，说明任务已完成
    pipeclose (job->pipefd[0]);
    pipeclose (job->pipefd[1]); // 关闭管道
    if (job->done_func) { // 执行done_func
        job->done_func (job->result);
    }

    ccnet_job_manager_remove_job (job->manager, job->id); // 移除任务
}

int
job_thread_create (CcnetJob *job) // 创建任务线程
{
    if (ccnet_pipe (job->pipefd) < 0) { // 创建管道
        g_warning ("pipe error: %s\n", strerror(errno));
        return -1;
    }

    g_thread_pool_push (job->manager->thread_pool, job, NULL); // 将任务加入到线程池
    // 线程池自动调度线程，选择一个线程执行job_thread_wrapper，并向其传入job作为参数

#ifndef UNIT_TEST // 通过libevent的event_once函数，安排一个一次性事件
    event_once (job->pipefd[0], EV_READ, job_done_cb, job, NULL); // 如果管道读端有数据，则调用job_done_cb
#endif

    return 0;
}

CcnetJob *
ccnet_job_new () // 创建新任务
{
    CcnetJob *job;

    job = g_new0 (CcnetJob, 1);
    return job;
}

void
ccnet_job_free (CcnetJob *job) // 销毁任务
{
    g_free (job);
}

CcnetJobManager *
ccnet_job_manager_new (int max_threads) // 创建新的任务管理器
{
    CcnetJobManager *mgr;

    mgr = g_new0 (CcnetJobManager, 1);
    mgr->jobs = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                       NULL, (GDestroyNotify)ccnet_job_free); // 创建任务列表
    mgr->thread_pool = g_thread_pool_new (job_thread_wrapper, // 线程包装函数
                                          NULL,
                                          max_threads, // 最大线程数
                                          FALSE,
                                          NULL); // 创建新的线程池
    /* g_thread_pool_set_max_unused_threads (MAX_IDLE_THREADS); */

    return mgr;
}

void
ccnet_job_manager_free (CcnetJobManager *mgr) // 销毁任务管理器
{
    g_hash_table_destroy (mgr->jobs);
    g_thread_pool_free (mgr->thread_pool, TRUE, FALSE);
    g_free (mgr);
}

int // 向任务管理器加入新的任务，返回任务id
ccnet_job_manager_schedule_job (CcnetJobManager *mgr, // 任务管理器
                               JobThreadFunc func, // 用户任务函数
                               JobDoneCallback done_func, // 完成函数
                               void *data) // 用户数据
{
    CcnetJob *job = ccnet_job_new (); // 创建新的任务
    job->id = mgr->next_job_id++;
    job->manager = mgr;
    job->thread_func = func;
    job->done_func = done_func;
    job->data = data;
    
    g_hash_table_insert (mgr->jobs, (gpointer)(long)job->id, job); // 将任务加入到任务列表

    if (job_thread_create (job) < 0) { // 创建新的任务线程
        g_hash_table_remove (mgr->jobs, (gpointer)(long)job->id); // 失败则移除任务
        return -1;
    }

    return job->id; // 返回任务id
}

void
ccnet_job_manager_remove_job (CcnetJobManager *mgr, int job_id) // 移除任务
{
    g_hash_table_remove (mgr->jobs, (gpointer)(long)job_id); // 根据任务id，从任务列表移除任务
}

#ifdef UNIT_TEST
void
ccnet_job_manager_wait_job (CcnetJobManager *mgr, int job_id)
{
    CcnetJob *job;
    
    job = g_hash_table_lookup (mgr->jobs, (gpointer)(long)job_id);
    /* manually call job_done_cb */
    job_done_cb (0, 0, (void *)job);
}
#endif
