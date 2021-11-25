/* 集群管理 */
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef _ORG_MGR_H_
#define _ORG_MGR_H_

typedef struct _SeafileSession SeafileSession;
typedef struct _CcnetOrgManager CcnetOrgManager;
typedef struct _CcnetOrgManagerPriv CcnetOrgManagerPriv;

struct _CcnetOrgManager // 组织管理器
{
    SeafileSession	*session;

    CcnetOrgManagerPriv	*priv;
};

CcnetOrgManager* ccnet_org_manager_new (SeafileSession *session); // 创建

int
ccnet_org_manager_prepare (CcnetOrgManager *manager); // 准备

void
ccnet_org_manager_start (CcnetOrgManager *manager); // 开始

int
ccnet_org_manager_create_org (CcnetOrgManager *mgr,
                              const char *org_name,
                              const char *url_prefix,
                              const char *creator,
                              GError **error); // 创建组织

int
ccnet_org_manager_remove_org (CcnetOrgManager *mgr,
                              int org_id,
                              GError **error); // 移除组织

GList *
ccnet_org_manager_get_all_orgs (CcnetOrgManager *mgr,
                                int start,
                                int limit); // 移除全部组织

int
ccnet_org_manager_count_orgs (CcnetOrgManager *mgr); // 统计组织个数

CcnetOrganization *
ccnet_org_manager_get_org_by_url_prefix (CcnetOrgManager *mgr,
                                         const char *url_prefix,
                                         GError **error); // 根据url前缀获取组织

CcnetOrganization *
ccnet_org_manager_get_org_by_id (CcnetOrgManager *mgr,
                                 int org_id,
                                 GError **error); // 根据id获取组织

int
ccnet_org_manager_add_org_user (CcnetOrgManager *mgr,
                                int org_id,
                                const char *email,
                                int is_staff,
                                GError **error); // 为组织添加用户

int
ccnet_org_manager_remove_org_user (CcnetOrgManager *mgr,
                                   int org_id,
                                   const char *email,
                                   GError **error); // 为组织移除用户

GList *
ccnet_org_manager_get_orgs_by_user (CcnetOrgManager *mgr,
                                   const char *email,
                                   GError **error); // 获取用户所属的所有组织

GList *
ccnet_org_manager_get_org_emailusers (CcnetOrgManager *mgr,
                                      const char *url_prefix,
                                      int start, int limit); // 根据url前缀获取组织下的所有用户邮箱

int
ccnet_org_manager_add_org_group (CcnetOrgManager *mgr,
                                 int org_id,
                                 int group_id,
                                 GError **error); // 为组织添加群组
int
ccnet_org_manager_remove_org_group (CcnetOrgManager *mgr,
                                    int org_id,
                                    int group_id,
                                    GError **error); // 为组织移除群组

int
ccnet_org_manager_is_org_group (CcnetOrgManager *mgr,
                                int group_id,
                                GError **error); // 判断群组是否在组织内

int
ccnet_org_manager_get_org_id_by_group (CcnetOrgManager *mgr,
                                       int group_id,
                                       GError **error); // 根据群组id获取组织id

GList *
ccnet_org_manager_get_org_group_ids (CcnetOrgManager *mgr,
                                     int org_id,
                                     int start,
                                     int limit); // 获取组织内的群组id列表

GList *
ccnet_org_manager_get_org_groups (CcnetOrgManager *mgr,
                                  int org_id,
                                  int start,
                                  int limit); // 获取组织内的群组列表

GList *
ccnet_org_manager_get_org_groups_by_user (CcnetOrgManager *mgr,
                                          const char *user,
                                          int org_id); // 获取组织内该用户所在的群组

GList *
ccnet_org_manager_get_org_top_groups (CcnetOrgManager *mgr, int org_id, GError **error); // 获取顶级群组

int
ccnet_org_manager_org_user_exists (CcnetOrgManager *mgr,
                                   int org_id,
                                   const char *email,
                                   GError **error); // 判断用户是否在该组织中

char *
ccnet_org_manager_get_url_prefix_by_org_id (CcnetOrgManager *mgr,
                                            int org_id,
                                            GError **error); // 通过组织id获取url前缀

int
ccnet_org_manager_is_org_staff (CcnetOrgManager *mgr,
                                int org_id,
                                const char *email,
                                GError **error); // 判断用户是否是组织的管理员

int
ccnet_org_manager_set_org_staff (CcnetOrgManager *mgr,
                                 int org_id,
                                 const char *email,
                                 GError **error); // 设置组织管理员

int
ccnet_org_manager_unset_org_staff (CcnetOrgManager *mgr,
                                   int org_id,
                                   const char *email,
                                   GError **error); // 撤销组织管理员

int
ccnet_org_manager_set_org_name(CcnetOrgManager *mgr,
                               int org_id,
                               const char *org_name,
                               GError **error); // 设置组织名


#endif /* _ORG_MGR_H_ */
