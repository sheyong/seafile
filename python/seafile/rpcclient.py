
import ccnet
from pysearpc import searpc_func, SearpcError

class SeafileRpcClient(ccnet.RpcClientBase):

    def __init__(self, ccnet_client_pool, *args, **kwargs):
        ccnet.RpcClientBase.__init__(self, ccnet_client_pool, "seafile-rpcserver",
                                     *args, **kwargs)

    @searpc_func("object", [])
    def seafile_get_session_info():
        pass
    get_session_info = seafile_get_session_info

    @searpc_func("int", ["string"])
    def seafile_calc_dir_size(path):
        pass
    calc_dir_size = seafile_calc_dir_size

    @searpc_func("int64", [])
    def seafile_get_total_block_size():
        pass
    get_total_block_size = seafile_get_total_block_size;

    @searpc_func("string", ["string"])
    def seafile_get_config(key):
        pass
    get_config = seafile_get_config

    @searpc_func("int", ["string", "string"])
    def seafile_set_config(key, value):
        pass
    set_config = seafile_set_config

    ### repo
    @searpc_func("objlist", ["int", "int"])
    def seafile_get_repo_list():
        pass
    get_repo_list = seafile_get_repo_list

    @searpc_func("object", ["string"])
    def seafile_get_repo():
        pass
    get_repo = seafile_get_repo

    @searpc_func("string", ["string", "string", "string", "string", "string", "int"])
    def seafile_create_repo(name, desc, passwd, base, relay_id, keep_history):
        pass
    create_repo = seafile_create_repo

    @searpc_func("int", ["string"])
    def seafile_destroy_repo(repo_id):
        pass
    remove_repo = seafile_destroy_repo

    @searpc_func("objlist", ["string", "string", "string"])
    def seafile_diff():
        pass
    get_diff = seafile_diff

    @searpc_func("object", ["string"])
    def seafile_get_commit(commit_id):
        pass
    get_commit = seafile_get_commit

    @searpc_func("objlist", ["string", "int", "int"])
    def seafile_get_commit_list():
        pass
    get_commit_list = seafile_get_commit_list

    @searpc_func("objlist", ["string"])
    def seafile_list_dir(dir_id):
        pass
    list_dir = seafile_list_dir

    @searpc_func("objlist", ["string", "string"])
    def seafile_list_dir_by_path(commit_id, path):
        pass
    list_dir_by_path = seafile_list_dir_by_path

    @searpc_func("objlist", ["string"])
    def seafile_branch_gets(repo_id):
        pass
    branch_gets = seafile_branch_gets

    @searpc_func("int", ["string", "string"])
    def seafile_branch_add(repo_id, branch):
        pass
    branch_add = seafile_branch_add

    ##### clone related
    @searpc_func("string", ["string", "string"])
    def gen_default_worktree(worktree_parent, repo_name):
        pass

    @searpc_func("string", ["string", "string", "string", "string", "string", "string"])
    def seafile_clone(repo_id, peer_id, repo_name, worktree_parent, token, password):
        pass
    clone = seafile_clone

    @searpc_func("int", ["string"])
    def seafile_cancel_clone_task(repo_id):
        pass
    cancel_clone_task = seafile_cancel_clone_task

    @searpc_func("int", ["string"])
    def seafile_remove_clone_task(repo_id):
        pass
    remove_clone_task = seafile_remove_clone_task

    @searpc_func("objlist", [])
    def seafile_get_clone_tasks():
        pass
    get_clone_tasks = seafile_get_clone_tasks

    @searpc_func("object", ["string"])
    def seafile_find_transfer_task(repo_id):
        pass
    find_transfer_task = seafile_find_transfer_task
 
    @searpc_func("object", ["string"])
    def seafile_get_checkout_task(repo_id):
        pass
    get_checkout_task = seafile_get_checkout_task
    
    ### sync
    @searpc_func("int", ["string", "string"])
    def seafile_sync(repo_id, peer_id):
        pass
    sync = seafile_sync

    @searpc_func("object", ["string"])
    def seafile_get_repo_sync_task():
        pass
    get_repo_sync_task = seafile_get_repo_sync_task

    @searpc_func("object", ["string"])
    def seafile_get_repo_sync_info():
        pass
    get_repo_sync_info = seafile_get_repo_sync_info


    ###### Property Management #########

    @searpc_func("int", ["string", "string"])
    def seafile_set_repo_passwd(repo_id, passwd):
        pass
    set_repo_passwd = seafile_set_repo_passwd

    @searpc_func("int", ["string", "string", "string"])
    def seafile_set_repo_property(repo_id, key, value):
        pass
    set_repo_property = seafile_set_repo_property

    @searpc_func("string", ["string", "string"])
    def seafile_get_repo_property(repo_id, key):
        pass
    get_repo_property = seafile_get_repo_property

    @searpc_func("int", ["string", "string"])
    def seafile_set_repo_token(repo_id, token):
        pass
    set_repo_token = seafile_set_repo_token

    @searpc_func("string", ["string"])
    def seafile_get_repo_token(repo_id):
        pass
    get_repo_token = seafile_get_repo_token


class SeafileThreadedRpcClient(ccnet.RpcClientBase):

    def __init__(self, ccnet_client_pool, *args, **kwargs):
        ccnet.RpcClientBase.__init__(self, ccnet_client_pool, 
                                     "seafile-threaded-rpcserver", 
                                     *args, **kwargs)

    @searpc_func("int", ["string", "string", "string"])
    def seafile_edit_repo():
        pass
    edit_repo = seafile_edit_repo

    @searpc_func("int", ["string", "string"])
    def seafile_reset(repo_id, commit_id):
        pass
    reset = seafile_reset

    @searpc_func("int", ["string", "string"])
    def seafile_revert(repo_id, commit_id):
        pass
    revert = seafile_revert

    @searpc_func("int", ["string", "string"])
    def seafile_add(repo_id, path):
        pass
    add = seafile_add

    @searpc_func("int", ["string", "string"])
    def seafile_rm():
        pass
    rm = seafile_rm

    @searpc_func("string", ["string", "string"])
    def seafile_commit(repo_id, description):
        pass
    commit = seafile_commit


class MonitorRpcClient(ccnet.RpcClientBase):

    def __init__(self, ccnet_client_pool):
        ccnet.RpcClientBase.__init__(self, ccnet_client_pool, "monitor-rpcserver")

    @searpc_func("int", ["string"])
    def monitor_get_repos_size(repo_ids):
        pass
    get_repos_size = monitor_get_repos_size


class SeafServerRpcClient(ccnet.RpcClientBase):

    def __init__(self, ccnet_client_pool, *args, **kwargs):
        ccnet.RpcClientBase.__init__(self, ccnet_client_pool, "seafserv-rpcserver",
                                     *args, **kwargs)

    # token for web access to repo
    @searpc_func("int", ["string", "string", "string", "string", "string"])
    def seafile_web_save_access_token(token, repo_id, obj_id, op, username):
        pass
    web_save_access_token = seafile_web_save_access_token
    
    @searpc_func("object", ["string"])
    def seafile_web_query_access_token(token):
        pass
    web_query_access_token = seafile_web_query_access_token

    ###### GC    ####################
    @searpc_func("int", [])
    def seafile_gc():
        pass
    gc = seafile_gc

    @searpc_func("int", [])
    def seafile_gc_get_progress():
        pass
    gc_get_progress = seafile_gc_get_progress

    # password management
    @searpc_func("int", ["string", "string"])
    def seafile_is_passwd_set(repo_id, user):
        pass
    is_passwd_set = seafile_is_passwd_set

    @searpc_func("object", ["string", "string"])
    def seafile_get_decrypt_key(repo_id, user):
        pass
    get_decrypt_key = seafile_get_decrypt_key
    
class SeafServerThreadedRpcClient(ccnet.RpcClientBase):

    def __init__(self, ccnet_client_pool, *args, **kwargs):
        ccnet.RpcClientBase.__init__(self, ccnet_client_pool,
                                     "seafserv-threaded-rpcserver",
                                     *args, **kwargs)

    # repo manupulation 
    @searpc_func("object", ["string"])
    def seafile_get_repo(repo_id):
        pass
    get_repo = seafile_get_repo

    @searpc_func("int", ["string"])
    def seafile_destroy_repo(repo_id):
        pass
    remove_repo = seafile_destroy_repo

    @searpc_func("objlist", ["int", "int"])
    def seafile_get_repo_list(start, limit):
        pass
    get_repo_list = seafile_get_repo_list

    @searpc_func("objlist", ["int", "int", "int"])
    def seafile_get_org_repo_list(org_id, start, limit):
        pass
    get_org_repo_list = seafile_get_org_repo_list

    @searpc_func("int", ["int"])
    def seafile_remove_org_repo_by_org_id(org_id):
        pass
    remove_org_repo_by_org_id = seafile_remove_org_repo_by_org_id
    
    @searpc_func("int", ["string", "string"])
    def seafile_is_repo_owner(user_id, repo_id):
        pass
    is_repo_owner = seafile_is_repo_owner

    @searpc_func("string", ["string"])
    def seafile_get_repo_owner(repo_id):
        pass
    get_repo_owner = seafile_get_repo_owner
    
    @searpc_func("objlist", ["string"])
    def seafile_list_owned_repos(user_id):
        pass
    list_owned_repos = seafile_list_owned_repos

    @searpc_func("int64", ["string"])
    def seafile_server_repo_size(repo_id):
        pass
    server_repo_size = seafile_server_repo_size
    
    @searpc_func("int", ["string", "string"])
    def seafile_repo_set_access_property(repo_id, role):
        pass
    repo_set_access_property = seafile_repo_set_access_property
    
    @searpc_func("string", ["string"])
    def seafile_repo_query_access_property(repo_id):
        pass
    repo_query_access_property = seafile_repo_query_access_property

    @searpc_func("int",  ["string", "string", "string"])
    def seafile_revert_on_server(repo_id, commit_id, user_name):
        pass
    revert_on_server = seafile_revert_on_server

    @searpc_func("objlist", ["string", "string", "string"])
    def seafile_diff():
        pass
    get_diff = seafile_diff

    @searpc_func("int", ["string", "string", "string", "string", "string"])
    def seafile_post_file(repo_id, tmp_file_path, parent_dir, filename, user):
        pass
    post_file = seafile_post_file 

    @searpc_func("int", ["string", "string", "string", "string"])
    def seafile_post_dir(repo_id, parent_dir, new_dir_name, user):
        pass
    post_dir = seafile_post_dir 

    @searpc_func("int", ["string", "string", "string", "string", "string"])
    def seafile_put_file(repo_id, tmp_file_path, parent_dir, filename, user):
        pass
    put_file = seafile_put_file 

    @searpc_func("int", ["string", "string", "string", "string"])
    def seafile_del_file(repo_id, parent_dir, filename, user):
        pass
    del_file = seafile_del_file 

    @searpc_func("int", ["string", "string", "string", "string", "string", "string", "string"])
    def seafile_copy_file(src_repo, src_dir, src_filename, dst_repo, dst_dir, dst_filename, user):
        pass
    copy_file = seafile_copy_file 

    @searpc_func("int", ["string", "string", "string", "string", "string", "string", "string"])
    def seafile_move_file(src_repo, src_dir, src_filename, dst_repo, dst_dir, dst_filename, user):
        pass
    move_file = seafile_move_file

    @searpc_func("int", ["string", "string", "string", "string", "string"])
    def seafile_rename_file(repo_id, parent_dir, oldname, newname, user):
        pass
    rename_file = seafile_rename_file 

    @searpc_func("int", ["string", "string"])
    def seafile_is_valid_filename(repo_id, filename):
        pass
    is_valid_filename = seafile_is_valid_filename 

    @searpc_func("object", ["string"])
    def seafile_get_commit(commit_id):
        pass
    get_commit = seafile_get_commit

    @searpc_func("objlist", ["string"])
    def seafile_list_dir(dir_id):
        pass
    list_dir = seafile_list_dir

    @searpc_func("int64", ["string"])
    def seafile_get_file_size(file_id):
        pass
    get_file_size = seafile_get_file_size

    @searpc_func("objlist", ["string", "string"])
    def seafile_list_dir_by_path(commit_id, path):
        pass
    list_dir_by_path = seafile_list_dir_by_path

    @searpc_func("string", ["string", "string"])
    def seafile_get_file_by_path(repo_id, path):
        pass
    get_file_by_path = seafile_get_file_by_path

    @searpc_func("objlist", ["string", "string"])
    def seafile_list_file_revisions(repo_id, path):
        pass
    list_file_revisions = seafile_list_file_revisions

    @searpc_func("int", ["string", "string", "string", "string"])
    def seafile_revert_file(repo_id, commit_id, path, user):
        pass
    revert_file = seafile_revert_file

    # share repo to user
    @searpc_func("string", ["string", "string", "string", "string"])
    def seafile_add_share(repo_id, from_email, to_email, permission):
        pass
    add_share = seafile_add_share

    @searpc_func("objlist", ["string", "string", "int", "int"])
    def seafile_list_share_repos(email, query_col, start, limit):
        pass
    list_share_repos = seafile_list_share_repos

    @searpc_func("int", ["string", "string", "string"])
    def seafile_remove_share(repo_id, from_email, to_email):
        pass
    remove_share = seafile_remove_share

    # share repo to group
    @searpc_func("int", ["string", "int", "string", "string"])
    def seafile_group_share_repo(repo_id, group_id, user_name, permisson):
        pass
    group_share_repo = seafile_group_share_repo
    
    @searpc_func("int", ["string", "int", "string"])
    def seafile_group_unshare_repo(repo_id, group_id, user_name):
        pass
    group_unshare_repo = seafile_group_unshare_repo

    @searpc_func("string", ["int"])
    def seafile_get_group_repoids(group_id):
        pass
    get_group_repoids = seafile_get_group_repoids

    @searpc_func("objlist", ["string"])
    def seafile_get_group_my_share_repos(user_name):
        pass
    get_group_my_share_repos = seafile_get_group_my_share_repos

    @searpc_func("string", ["string"])
    def seafile_get_group_repo_share_from(repo_id):
        pass
    get_group_repo_share_from = seafile_get_group_repo_share_from

    @searpc_func("int", ["int", "string"])
    def seafile_remove_repo_group(group_id, user_name):
        pass
    remove_repo_group = seafile_remove_repo_group
    
    # branch and commit
    @searpc_func("objlist", ["string"])
    def seafile_branch_gets(repo_id):
        pass
    branch_gets = seafile_branch_gets

    @searpc_func("objlist", ["string", "int", "int"])
    def seafile_get_commit_list(repo_id, offset, limit):
        pass
    get_commit_list = seafile_get_commit_list


    ###### Token ####################

    @searpc_func("int", ["string", "string", "string"])
    def seafile_set_repo_token(repo_id, email, token):
        pass
    set_repo_token = seafile_set_repo_token

    @searpc_func("string", ["string", "string", "init"])
    def seafile_get_repo_token_nonnull(repo_id, email):
        """Get the token of the repo for the email user. If the token does not
        exist, a new one is generated and returned.

        """
        pass
    get_repo_token_nonnull = seafile_get_repo_token_nonnull


    ###### quota ##########
    @searpc_func("int64", ["string"])
    def seafile_get_user_quota_usage(user_id):
        pass
    get_user_quota_usage = seafile_get_user_quota_usage

    @searpc_func("int", ["string", "int64"])
    def set_user_quota(user, quota):
        pass

    @searpc_func("int64", ["string"])
    def get_user_quota(user):
        pass

    @searpc_func("int", ["int", "int64"])
    def set_org_quota(org_id, quota):
        pass

    @searpc_func("int64", ["int"])
    def get_org_quota(org_id):
        pass

    @searpc_func("int", ["int", "string", "int64"])
    def set_org_user_quota(org_id, user, quota):
        pass

    @searpc_func("int64", ["int", "string"])
    def get_org_user_quota(org_id, user):
        pass

    # password management
    @searpc_func("int", ["string", "string", "string"])
    def seafile_set_passwd(repo_id, user, passwd):
        pass
    set_passwd = seafile_set_passwd

    @searpc_func("string", ["string", "string", "string", "string"])
    def seafile_create_repo(name, desc, owner_email, passwd):
        pass

    create_repo = seafile_create_repo

    @searpc_func("string", ["string", "string", "string", "string", "int"])
    def seafile_create_org_repo(name, desc, user, passwd, org_id):
        pass
    create_org_repo = seafile_create_org_repo

    @searpc_func("int", ["string"])
    def seafile_get_org_id_by_repo_id(repo_id):
        pass
    get_org_id_by_repo_id = seafile_get_org_id_by_repo_id    
    
