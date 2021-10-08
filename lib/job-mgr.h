/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* 多线程任务管理 */

/**
 * Job Manager manages long term jobs. These jobs are run in their
 * own threads.
 */

#ifndef JOB_MGR_H
#define JOB_MGR_H

#include <glib.h>

struct _CcnetSession;

typedef struct _CcnetJob CcnetJob; // 任务结构体
typedef struct _CcnetJobManager CcnetJobManager; // 任务管理器结构体

/*
  The thread func should return the result back by
     return (void *)result;
  The result will be passed to JobDoneCallback.
 */
typedef void* (*JobThreadFunc)(void *data); // 任务函数，传入用户数据，返回结果
typedef void (*JobDoneCallback)(void *result); // 完成函数，传入任务函数返回的结果

struct _CcnetJobManager {
    GHashTable      *jobs; // 任务表

    GThreadPool     *thread_pool; // 线程池

    int              next_job_id; // 下一个任务的id（单增）
};

void
ccnet_job_cancel (CcnetJob *job); // 取消某个任务（尚未实现）

CcnetJobManager *
ccnet_job_manager_new (int max_threads); // 创建新的任务管理器，含有max_thread个线程

void
ccnet_job_manager_free (CcnetJobManager *mgr); // 销毁任务管理器

int // 向任务管理器加入新的任务，返回任务id
ccnet_job_manager_schedule_job (CcnetJobManager *mgr, // 任务管理器
                                JobThreadFunc func, // 任务函数
                                JobDoneCallback done_func, // 完成函数
                                void *data); // 用户数据，传递给线程回调函数

/** 
 * Wait a specific job to be done.
 */
void
ccnet_job_manager_wait_job (CcnetJobManager *mgr, int job_id); // 等待某个任务完成


#endif
