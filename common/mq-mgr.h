/* 异步消息队列 */

#ifndef SEAF_MQ_MANAGER_H
#define SEAF_MQ_MANAGER_H

#include <jansson.h>

#define SEAFILE_SERVER_CHANNEL_EVENT "seaf_server.event"
#define SEAFILE_SERVER_CHANNEL_STATS "seaf_server.stats"

struct SeafMqManagerPriv;

typedef struct SeafMqManager {
    struct SeafMqManagerPriv *priv;
} SeafMqManager;

SeafMqManager * // 新建
seaf_mq_manager_new ();

int // 消息传给频道
seaf_mq_manager_publish_event (SeafMqManager *mgr, const char *channel, const char *content);

json_t * // 从频道中取出（一个json，格式：{ 'ctime': 创建时间, 'content': 消息内容 }）
seaf_mq_manager_pop_event (SeafMqManager *mgr, const char *channel);

#endif
