/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* CCNET定时器 */
// 基于libevent的evtimer实现，功能相当于：
// while (1) {
//    if (func(user_data)) sleep(interval_milliseconds);
// }

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <event2/event.h>
#include <event2/event_compat.h>
#include <event2/event_struct.h>
#else
#include <event.h>
#endif

#include <sys/time.h>

#include "utils.h"

#include "timer.h"

struct CcnetTimer // 定时器结构体，存储了evtimer的上下文
{
    struct event   event; // 事件id
    struct timeval tv; // 时间
    TimerCB        func; // 用户回调函数
    void          *user_data; // 用户数据
    uint8_t        inCallback; // 是否正在回调
};

static void
timer_callback (evutil_socket_t fd, short event, void *vtimer) // evtimer的回调函数，被反复执行
{
    int more;
    struct CcnetTimer *timer = vtimer; // 定时器结构体

    timer->inCallback = 1; // 表示当前正在回调
    more = (*timer->func) (timer->user_data); // 执行用户回调函数
    timer->inCallback = 0;

    if (more) // 如果返回不为零
        evtimer_add (&timer->event, &timer->tv); // 让evtimer在间隔timer->tv后执行回调函数
    else // 返回为零
        ccnet_timer_free (&timer); // 销毁定时器
}

void
ccnet_timer_free (CcnetTimer **ptimer) // 销毁定时器，并让ptimer指针指向NULL
{
    CcnetTimer *timer;

    /* zero out the argument passed in */
g_return_if_fail(ptimer); // 若ptimer指向NULL，则直接返回

timer = *ptimer;
*ptimer = NULL; // 让ptimer指针指向NULL

/* destroy the timer directly or via the command queue */
if (timer && !timer->inCallback) // 不是正在回调
{
    event_del(&timer->event); // 删除事件
    g_free(timer);            // 释放空间
    }
}

CcnetTimer*
ccnet_timer_new (TimerCB         func, // 用户回调函数
                 void           *user_data, // 用户数据
                 uint64_t        interval_milliseconds) // 间隔的毫秒数
{
    CcnetTimer *timer = g_new0 (CcnetTimer, 1); // 申请新的结构体空间

    timer->tv = timeval_from_msec (interval_milliseconds); // 将ms转为timeval，然后设置
    timer->func = func; // 设置用户回调函数
    timer->user_data = user_data; // 设置用户数据

    // 创建新的evtimer，其事件号存储在上下文的event内；回调函数为timer_callback，参数为timer
    evtimer_set (&timer->event, timer_callback, timer);
    evtimer_add (&timer->event, &timer->tv); // 让evtimer在间隔timer->tv后执行回调函数

    return timer;
}
