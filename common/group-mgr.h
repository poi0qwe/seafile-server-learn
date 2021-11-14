/* 组管理 */
/* 通过数据库维护组、组间、组内成员的关系 */

/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef GROUP_MGR_H
#define GROUP_MGR_H

/* #define MAX_GROUP_MEMBERS	16 */

typedef struct _SeafileSession SeafileSession;
typedef struct _CcnetGroupManager CcnetGroupManager;
typedef struct _CcnetGroupManagerPriv CcnetGroupManagerPriv;

struct _CcnetGroupManager
{
    SeafileSession *session;
    
    CcnetGroupManagerPriv	*priv;
};

CcnetGroupManager* ccnet_group_manager_new (SeafileSession *session); // 创建新的组管理器

int
ccnet_group_manager_prepare (CcnetGroupManager *manager); // 准备

void ccnet_group_manager_start (CcnetGroupManager *manager); // 开始

int ccnet_group_manager_create_group (CcnetGroupManager *mgr,
                                      const char *group_name,
                                      const char *user_name,
                                      int parent_group_id,
                                      GError **error); // 创建组

int ccnet_group_manager_create_org_group (CcnetGroupManager *mgr,
                                          int org_id,
                                          const char *group_name,
                                          const char *user_name,
                                          int parent_group_id,
                                          GError **error); // 创建组并添加到org

int ccnet_group_manager_remove_group (CcnetGroupManager *mgr,
                                      int group_id,
                                      gboolean remove_anyway,
                                      GError **error); // 移除组

int ccnet_group_manager_add_member (CcnetGroupManager *mgr,
                                    int group_id,
                                    const char *user_name,
                                    const char *member_name,
                                    GError **error); // 添加成员

int ccnet_group_manager_remove_member (CcnetGroupManager *mgr,
                                       int group_id,
                                       const char *user_name,
                                       const char *member_name,
                                       GError **error); // 移除成员

int ccnet_group_manager_set_admin (CcnetGroupManager *mgr,
                                   int group_id,
                                   const char *member_name,
                                   GError **error); // 设置管理员

int ccnet_group_manager_unset_admin (CcnetGroupManager *mgr,
                                     int group_id,
                                     const char *member_name,
                                     GError **error); // 解除管理员

int ccnet_group_manager_set_group_name (CcnetGroupManager *mgr,
                                        int group_id,
                                        const char *group_name,
                                        GError **error); // 设置组名

int ccnet_group_manager_quit_group (CcnetGroupManager *mgr,
                                    int group_id,
                                    const char *user_name,
                                    GError **error); // 退出组

GList *
ccnet_group_manager_get_groups_by_user (CcnetGroupManager *mgr,
                                        const char *user_name,
                                        gboolean return_ancestors,
                                        GError **error); // 获取用户的组

CcnetGroup *
ccnet_group_manager_get_group (CcnetGroupManager *mgr, int group_id,
                               GError **error); // 获取组

GList *
ccnet_group_manager_get_group_members (CcnetGroupManager *mgr,
                                       int group_id,
                                       int start,
                                       int limit,
                                       GError **error); // 获取组员列表

GList *
ccnet_group_manager_get_members_with_prefix (CcnetGroupManager *mgr,
                                             int group_id,
                                             const char *prefix,
                                             GError **error); // 获取组员列表，同时组员名前缀为prefix

int
ccnet_group_manager_check_group_staff (CcnetGroupManager *mgr,
                                       int group_id,
                                       const char *user_name,
                                       int in_structure); // 检查组内该成员是否为管理员

int
ccnet_group_manager_remove_group_user (CcnetGroupManager *mgr,
                                       const char *user); // 移除成员

int
ccnet_group_manager_is_group_user (CcnetGroupManager *mgr,
                                   int group_id,
                                   const char *user,
                                   gboolean in_structure); // 判断用户是否在组内

GList*
ccnet_group_manager_list_all_departments (CcnetGroupManager *mgr,
                                          GError **error); // 列举所有部门

GList*
ccnet_group_manager_get_all_groups (CcnetGroupManager *mgr,
                                    int start, int limit, GError **error); // 获取所有组

int
ccnet_group_manager_set_group_creator (CcnetGroupManager *mgr,
                                       int group_id,
                                       const char *user_name); // 获取组的创建者名城

GList*
ccnet_group_manager_search_groups (CcnetGroupManager *mgr,
                                   const char *keyword,
                                   int start, int limit); // 搜索组

GList *
ccnet_group_manager_get_top_groups (CcnetGroupManager *mgr, gboolean including_org, GError **error); // 获取顶级组列表

GList *
ccnet_group_manager_get_child_groups (CcnetGroupManager *mgr, int group_id,
                                      GError **error); // 获取子组列表

GList *
ccnet_group_manager_get_descendants_groups (CcnetGroupManager *mgr, int group_id,
                                            GError **error); // 获取后代组列表

GList *
ccnet_group_manager_get_ancestor_groups (CcnetGroupManager *mgr, int group_id); // 获取祖先组列表

GList *
ccnet_group_manager_get_groups_members (CcnetGroupManager *mgr, const char *group_ids,
                                        GError **error); // 获取多个组的成员

int
ccnet_group_manager_update_group_user (CcnetGroupManager *mgr,
                                       const char *old_email,
                                       const char *new_email); // 更新用户邮箱
#endif /* GROUP_MGR_H */

