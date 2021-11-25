/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef CCNET_USER_MGR_H
#define CCNET_USER_MGR_H

#include <glib.h>
#include <glib-object.h>

#define CCNET_TYPE_USER_MANAGER                  (ccnet_user_manager_get_type ())
#define CCNET_USER_MANAGER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CCNET_TYPE_USER_MANAGER, CcnetUserManager))
#define CCNET_IS_USER_MANAGER(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CCNET_TYPE_USER_MANAGER))
#define CCNET_USER_MANAGER_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CCNET_TYPE_USER_MANAGER, CcnetUserManagerClass))
#define CCNET_IS_USER_MANAGER_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CCNET_TYPE_USER_MANAGER))
#define CCNET_USER_MANAGER_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CCNET_TYPE_USER_MANAGER, CcnetUserManagerClass))


typedef struct _SeafileSession SeafileSession;
typedef struct _CcnetUserManager CcnetUserManager;
typedef struct _CcnetUserManagerClass CcnetUserManagerClass;

typedef struct CcnetUserManagerPriv CcnetUserManagerPriv;


struct _CcnetUserManager // 用户管理器
{
    GObject         parent_instance; // 父实例

    SeafileSession   *session;
    
    char           *userdb_path; // 用户数据库路径
    GHashTable     *user_hash; // 用户哈希

#ifdef HAVE_LDAP
    /* LDAP related */
    gboolean        use_ldap;
    char           *ldap_host;
#ifdef WIN32
    gboolean        use_ssl;
#endif
    char           **base_list;  /* base DN from where all users can be reached */
    char           *filter;     /* Additional search filter */
    char           *user_dn;    /* DN of the admin user */
    char           *password;   /* password for admin user */
    char           *login_attr;  /* attribute name used for login */
    gboolean        follow_referrals; /* Follow referrals returned by the server. */
#endif

    int passwd_hash_iter;

    CcnetUserManagerPriv *priv;
};

struct _CcnetUserManagerClass
{
    GObjectClass    parent_class;
};

GType ccnet_user_manager_get_type  (void);

CcnetUserManager* ccnet_user_manager_new (SeafileSession *); // 创建新的用户管理器
int ccnet_user_manager_prepare (CcnetUserManager *manager); // 准备

void ccnet_user_manager_free (CcnetUserManager *manager); // 释放

void ccnet_user_manager_start (CcnetUserManager *manager); // 开始

void
ccnet_user_manager_set_max_users (CcnetUserManager *manager, gint64 max_users); // 设置最大用户数量

int
ccnet_user_manager_add_emailuser (CcnetUserManager *manager,
                                  const char *email,
                                  const char *encry_passwd,
                                  int is_staff, int is_active); // 增加用户

int
ccnet_user_manager_remove_emailuser (CcnetUserManager *manager,
                                     const char *source,
                                     const char *email); // 移除用户

int
ccnet_user_manager_validate_emailuser (CcnetUserManager *manager,
                                       const char *email,
                                       const char *passwd); // 检查用户密码

CcnetEmailUser*
ccnet_user_manager_get_emailuser (CcnetUserManager *manager, const char *email); // 获取用户

CcnetEmailUser*
ccnet_user_manager_get_emailuser_with_import (CcnetUserManager *manager,
                                              const char *email); // 导入用户
CcnetEmailUser*
ccnet_user_manager_get_emailuser_by_id (CcnetUserManager *manager, int id); // 根据id获取用户

/*
 * @source: "DB" or "LDAP".
 * @status: "", "active", or "inactive". returns all users when this argument is "".
 */
GList*
ccnet_user_manager_get_emailusers (CcnetUserManager *manager,
                                   const char *source,
                                   int start, int limit,
                                   const char *status); // 根据邮箱获取用户列表

GList*
ccnet_user_manager_search_emailusers (CcnetUserManager *manager,
                                      const char *source,
                                      const char *keyword,
                                      int start, int limit); // 搜索用户

GList*
ccnet_user_manager_search_ldapusers (CcnetUserManager *manager,
                                     const char *keyword,
                                     int start, int limit); // 搜索ldap用户

gint64
ccnet_user_manager_count_emailusers (CcnetUserManager *manager, const char *source); // 统计用户数目

gint64
ccnet_user_manager_count_inactive_emailusers (CcnetUserManager *manager, const char *source); // 统计不活跃用户数目

GList*
ccnet_user_manager_filter_emailusers_by_emails(CcnetUserManager *manager,
                                               const char *emails); // 根据邮箱过滤用户

int
ccnet_user_manager_update_emailuser (CcnetUserManager *manager,
                                     const char *source,
                                     int id, const char* passwd,
                                     int is_staff, int is_active); // 更新用户信息

int
ccnet_user_manager_update_role_emailuser (CcnetUserManager *manager,
                                     const char* email, const char* role); // 更新用户角色

GList*
ccnet_user_manager_get_superusers(CcnetUserManager *manager); // 获取超级用户

int
ccnet_user_manager_add_binding (CcnetUserManager *manager, const char *email,
                                const char *peer_id); // 绑定peer用户到email用户

/* Remove all bindings to an email */
int
ccnet_user_manager_remove_binding (CcnetUserManager *manager, const char *email); // 解除全部绑定

/* Remove one specific peer-id binding to an email */
int
ccnet_user_manager_remove_one_binding (CcnetUserManager *manager,
                                       const char *email,
                                       const char *peer_id); // 解除一个绑定

char *
ccnet_user_manager_get_binding_email (CcnetUserManager *manager, const char *peer_id); // 获取所有绑定用户

GList *
ccnet_user_manager_get_binding_peerids (CcnetUserManager *manager, const char *email); // 获取所有绑定用户的id

int
ccnet_user_manager_set_reference_id (CcnetUserManager *manager,
                                     const char *primary_id,
                                     const char *reference_id,
                                     GError **error); // 设置引用id

char *
ccnet_user_manager_get_primary_id (CcnetUserManager *manager,
                                   const char *email); // 获取首选id

char *
ccnet_user_manager_get_login_id (CcnetUserManager *manager,
                                 const char *primary_id); // 获取登录id

GList *
ccnet_user_manager_get_emailusers_in_list (CcnetUserManager *manager,
                                           const char *source,
                                           const char *user_list,
                                           GError **error); // 获取用户列表下的用户

int
ccnet_user_manager_update_emailuser_id (CcnetUserManager *manager,
                                        const char *old_email,
                                        const char *new_email,
                                        GError **error); // 更新用户id
#endif
