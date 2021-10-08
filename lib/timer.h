/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* CCNET定时器 */

#ifndef CCNET_TIMER_H
#define CCNET_TIMER_H

/* return TRUE to reschedule the timer, return FALSE to cancle the timer */
typedef int (*TimerCB) (void *data); // 回调函数，返回int

struct CcnetTimer; // 定时器结构体

typedef struct CcnetTimer CcnetTimer;

/**
 * Calls timer_func(user_data) after the specified interval.
 * The timer is freed if timer_func returns zero.
 * Otherwise, it's called again after the same interval.
 */
CcnetTimer* ccnet_timer_new (TimerCB           func, // 用户回调函数；返回0则销毁此计数器
                             void             *user_data, // 用户数据；作为用户回调函数的参数
                             uint64_t          timeout_milliseconds); // 定时执行的间隔时间

/**
 * Frees a timer and sets the timer pointer to NULL.
 */
void ccnet_timer_free (CcnetTimer **timer); // 销毁定时器，并让timer指针指向NULL


#endif
