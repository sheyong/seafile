#!/usr/bin/env python
# encoding: utf-8

import gettext
import httplib
import locale
import os
import simplejson as json
import subprocess
import sys
import urllib
import web
from web.contrib.template import render_mako

import settings

from seaserv import CCNET_CONF_PATH, get_default_relay
from seaserv import ccnet_rpc, seafile_rpc, applet_rpc
from seaserv import get_peers_by_role, \
    send_command, get_peers
from seaserv import get_repos, get_repo, get_commits, \
    get_branches, open_dir, get_diff, \
    list_dir, remove_repos_on_relay, get_default_seafile_worktree, \
    get_current_prefs

from seaserv import TaskType

from pysearpc import SearpcError


urls = (
    '/', 'repos',
    '/opendir/', 'open_directory',
    '/home/', 'repos',
    '/repos/', 'repos',
    '/repo/', 'repo',
    '/repo/history/', 'repo_history',
    '/repo/setting/', 'repo_setting',
    '/repo/sync-status/', 'repo_sync_status',
    '/repo/transfer/', 'repo_transfer',
    '/repos/download-tasks/', 'CloneTasks',
    '/repos/clone-tasks/', 'clone_tasks',
    '/repo/download/', 'repo_download',
    '/repos/operation/', 'repo_operation',
    '/procs/', 'procs',
    '/settings/', 'settings_page',
    '/i18n/', 'i18n',
    '/seafile_access_check/', 'seafile_access_check',
    )

# See http://www.py2exe.org/index.cgi/WhereAmI
if 'win32' in sys.platform and hasattr(sys, 'frozen'):
    __file__ = sys.executable

curdir = os.path.abspath(os.path.dirname(__file__))
localedir = os.path.join(curdir, 'i18n')

if "darwin" == sys.platform and hasattr(sys, 'frozen'):
    sys.path.append(curdir)
    
NET_STATE_CONNECTED = 1

lang_in_use = 'zh_CN'

gettext.install('messages', localedir, unicode=True)
gettext.translation('messages', localedir,
                    languages=[lang_in_use]).install(True)

render = render_mako(directories=['templates'],
                     output_encoding='utf-8', input_encoding='utf-8',
                     default_filters=['decode.utf8'])

app = web.application(urls, globals())

SEAFILE_VERSION = '0.9.5'
default_options = { "confdir": CCNET_CONF_PATH,
                    'web_ctx': web.ctx, 
                    'seafile_version': SEAFILE_VERSION,
                    'lang': lang_in_use,
                    'settings': settings,
                    }

if not settings.DEBUG:
    # if not debug, print logs to file
    logdir = CCNET_CONF_PATH + os.sep + "logs" + os.sep
    sys.stdout = open(logdir + "ccnetweb.log", "a")
    sys.stderr = sys.stdout


def get_relay_of_repo(repo):
    if not repo:
        return None
    relay = None
    try:
        if repo.props.relay_id:
            relay = ccnet_rpc.get_peer(repo.props.relay_id)
    except:
        return None
    return relay


def get_dir_nav_links(repo, commit_id, path):
    """Get every folder on the path from repo root to [path]. Return value is
    in this format:
    
        [(root, href-to-root), (level-1-folder, href-to-level-1), ... (path, href-to-path)]

    """
    names = [] 
    links = []

    if path != u'/':
        names = path[1:].split(u'/')
        for idx,name in enumerate(names):
            current_path = u'/' + u'/'.join(names[:idx+1])
            quoted_path = urllib.quote(current_path.encode('utf-8'))
            href = "/repos/operation/?repo=%s&commit_id=%s&path=%s&op=dir" \
                   % (repo.props.id, commit_id, quoted_path)
            links.append(href)

    # insert root link in the front
    names.insert(0, repo.props.name)

    href = "/repos/operation/?repo=%s&commit_id=%s&op=dir" % (repo.props.id, commit_id)
    links.insert(0, href)

    return zip(names, links)

    
class open_directory:
    
    def GET(self):
        path = web.webapi.input(path='').path
        if path:
            open_dir(path)
        
        referer = web.ctx.env.get('HTTP_REFERER', '/home/')
        raise web.seeother(referer)


def prepare_repo_info(repo):
    """Get various types of information belong to the repo."""

    ### get branch information
    repo.branches = get_branches(repo.props.id)
    repo.current_branch = None
    repo.master_branch = None
    repo.local_branch = None
    for branch in repo.branches:
        if branch.props.name == "master":
            repo.master_branch = branch
        elif branch.props.name == "local":
            repo.local_branch = branch

        if branch.props.name == repo.props.head_branch:
            repo.current_branch = branch

    ### transfer task information and sync info
    repo.sync_info = seafile_rpc.get_repo_sync_info(repo.props.id)

class repos:

    def show_repos(self):
        # relay info
        sinfo = ccnet_rpc.get_session_info()
        relays = get_peers_by_role ("MyRelay")
        drelay = None
        if sinfo.props.default_relay:
            for relay in relays:
                if relay.props.id == sinfo.props.default_relay:
                    drelay = relay
                    break

        # get repos info
        repos = get_repos()
        for repo in repos:
            # is_broken is not used now, we should clean it later
            repo.is_broken = False
            try:
                prepare_repo_info(repo)
            except SearpcError, e:
                repo.is_broken = True
                repo.error_msg = e.msg

        for relay in relays:
            relay.repos = []
            for repo in repos:
                if relay.props.id == repo.props.relay_id:
                    relay.repos.append(repo)

        repos.sort(key=lambda x: x.props.last_modify, reverse=True)

        return render.repos(repos=repos,
                            drelay=drelay,
                            relays=relays,
                            **default_options)


    def GET(self):
        return self.show_repos()


class repo:
    """Show a specific repo."""
    
    def show_repo(self, repo_id):
        repo = seafile_rpc.get_repo(repo_id)
        if not repo:
            return render.repo_missing(repo_id=repo_id, **default_options)

        try:
            prepare_repo_info(repo)
            recent_commits = get_commits(repo_id, 0, 3)
            repo.is_broken = False
        except SearpcError, e:
            repo.is_broken = True
            recent_commits = []
            repo.error_msg = e.msg

        relay = get_relay_of_repo(repo)

        return render.repo(repo=repo,
                           recent_commits=recent_commits,
                           relay=relay,
                           **default_options)
    

    def GET(self):
        inputs = web.webapi.input(repo='')
        return self.show_repo(inputs.repo)


class repo_history:

    def show_repo_history(self, repo_id):
        repo = seafile_rpc.get_repo(repo_id)
        prepare_repo_info(repo)
        inputs = web.webapi.input(page="1", per_page="25")
        current_page = int(inputs.page)
        per_page = int(inputs.per_page)
        commits_all = get_commits(repo_id, per_page * (current_page - 1), per_page + 1)
        commits = commits_all[:per_page]
        if len(commits_all) == per_page + 1:
            page_next = True
        else:
            page_next = False

        return render.repo_history(repo=repo, 
                           commits=commits,
                           current_page=current_page,
                           per_page=per_page,
                           page_next=page_next,
                           **default_options)

    def GET(self):
        inputs = web.webapi.input(repo='')
        return self.show_repo_history(inputs.repo)
 

class repo_transfer:

    def GET(self):
        inputs = web.webapi.input(repo='')
        task = {}
        t = seafile_rpc.find_transfer_task(inputs.repo)
        if t:
            task['ttype'] = t.props.ttype
            task['state'] = t.props.state
            task['rt_state'] = t.props.rt_state
            task['block_done'] = t.props.block_done
            task['block_total'] = t.props.block_total
            task['rate'] = t.props.rate
            task['error_str'] = t.props.error_str

        return json.dumps(task)

        
class repo_sync_status:

    def GET(self):
        inputs = web.webapi.input(repo='')
        sync_status = {}
        repo = get_repo(inputs.repo)
        if not repo or not repo.props.worktree or not repo.props.head_branch: 
            return json.dumps(sync_status)

        relay = get_relay_of_repo(repo)
        if relay:
            if relay.props.net_state != NET_STATE_CONNECTED:
                sync_status['state'] = 'relay not connected'
                return json.dumps(sync_status)

        t = seafile_rpc.get_repo_sync_task(inputs.repo)
        if t:
            if t.props.state == 'error' and t.props.error == 'relay not connected':
                # Hide the 'relay not connected' error from daemon when relay
                # is actually connected, but the check sync pulse has not come yet
                sync_status['state'] = 'waiting for sync'
                return json.dumps(sync_status)
            elif t.props.state == 'canceled' or t.props.state == 'cancel pending':
                sync_status['state'] = 'waiting for sync'
            else:
                sync_status['state'] = t.props.state

            sync_status['is_sync_lan'] = t.props.is_sync_lan
            sync_status['error'] = t.props.error
        else:
            # No sync task yet: seafile maybe have just been started 
            sync_status['state'] = 'waiting for sync'

        if sync_status['state'] == 'waiting for sync' and not repo.props.auto_sync:
            sync_status['state'] = 'auto sync is turned off'

        return json.dumps(sync_status)


class repo_operation:

    def perform_operation_get(self, op, repo_id):
        repo = get_repo(repo_id)
        if not repo:
            raise web.seeother('/repos/')

        if op == 'sync':
            try:
                seafile_rpc.sync(repo.props.id, None)
            except:
                pass

        elif op == 'open' and repo.props.worktree:
            try:
                open_dir(repo.props.worktree.encode('utf-8'))
            except:
                pass
            referer = web.ctx.env.get('HTTP_REFERER', '/home/')
            raise web.seeother(referer)

        elif op == 'open_file':
            quote_file_path = web.webapi.input(quote_file_path='').file_path
            file_path = quote_file_path.encode('utf-8')
            dir_path = file_path
            if os.path.exists(file_path) and os.path.isfile(file_path):
                dir_path = os.path.dirname(file_path)

            try:
                open_dir(dir_path)
            except:
                pass
            return render.checkout_msg(repo=repo, file_path=file_path, **default_options)

        elif op == 'diff':
            inputs = web.webapi.input(old='', new='')
            new_commit = seafile_rpc.get_commit(inputs.new)
            if inputs.old != '':
                old_commit = seafile_rpc.get_commit(inputs.old)
            else:
                old_commit = None
            (new, removed, renamed, modified, newdir, deldir) = get_diff(repo_id, inputs.old, inputs.new)
            return render.repo_diff(repo=repo, 
                                    new=new, removed=removed, 
                                    renamed=renamed, modified=modified,
                                    newdir=newdir, deldir=deldir,
                                    new_commit=new_commit, old_commit=old_commit,
                                    **default_options)
        elif op == 'lsch':
            inputs = web.webapi.input(old='', new='')
            (new, removed, renamed, modified, newdir, deldir) = get_diff(repo_id, inputs.old, inputs.new)
            ch = {}
            ch['new'] = new
            ch['removed'] = removed
            ch['renamed'] = renamed
            ch['modified'] = modified
            ch['newdir'] = newdir
            ch['deldir'] = deldir
            return json.dumps(ch)

        elif op == 'dir':
            inputs = web.webapi.input(commit_id='', path='/')
            dirs = seafile_rpc.list_dir_by_path(inputs.commit_id, inputs.path.encode('utf-8'))
            navs = get_dir_nav_links(repo, inputs.commit_id, inputs.path)
            try:
                commit = seafile_rpc.get_commit(inputs.commit_id)
            except SearpcError,e :
                raise web.seeother('/repo/?repo=%s' % repo_id)
                
            return render.repo_dir(repo=repo, dirs=dirs, commit_id=inputs.commit_id,
                                   commit=commit,
                                   navs=navs,
                                   path=inputs.path,
                                   **default_options)

        elif op == 'remove':
            seafile_rpc.remove_repo(repo_id)
            raise web.seeother('/repos/')

        elif op == 'set-auto-sync':
            seafile_rpc.set_repo_property(repo_id, "auto-sync", "true")
            auto_sync = {}
            auto_sync['start'] = True
            return json.dumps(auto_sync)

        elif op == 'set-manual-sync':
            seafile_rpc.set_repo_property(repo_id, "auto-sync", "false")
            auto_sync = {}
            auto_sync['start'] = False
            return json.dumps(auto_sync)

        referer = web.ctx.env.get('HTTP_REFERER', '/home/')
        raise web.seeother(referer)


    def perform_operation_post(self, op, repo_id):
        repo = get_repo(repo_id)
        if not repo:
            raise web.seeother('/repos/')

        if op == 'modify-relay':
            relay_id = web.webapi.input(relay_id="").relay_id
            if relay_id != repo.props.relay_id:
                seafile_rpc.set_repo_property(repo.props.id,
                                              "relay-id", relay_id)
        elif op == 'set-passwd':
            passwd = web.webapi.input(passwd="").passwd
            if passwd:
                seafile_rpc.set_repo_passwd(repo.props.id, passwd)
  
        referer = web.ctx.env.get('HTTP_REFERER', '/home/')
        raise web.seeother(referer)

    def GET(self):
        inputs = web.webapi.input(op='', repo='')
        if inputs.op and inputs.repo:
            return self.perform_operation_get(inputs.op, inputs.repo)
        raise web.seeother('/repos/')

    def POST(self):
        inputs = web.webapi.input(op='', repo='')
        if inputs.op and inputs.repo:
            return self.perform_operation_post(inputs.op, inputs.repo)

        raise web.seeother('/repos/')


class CloneTasks:

    def GET(self):
        inputs = web.webapi.input(op='', repo_id='')
        if inputs.op and inputs.repo_id:
            if inputs.op == "remove":
                seafile_rpc.remove_clone_task(inputs.repo_id)
            elif inputs.op == "cancel":
                seafile_rpc.cancel_clone_task(inputs.repo_id)
            raise web.seeother('/repos/download-tasks/')

        tasks = seafile_rpc.get_clone_tasks()

        return render.clone_tasks(tasks=tasks,
                                  **default_options)
        

class clone_tasks:
   
    def GET(self):
        ts = []
        tasks = seafile_rpc.get_clone_tasks()
        for task in tasks:
            t = {}
            t['repo_id'] = task.props.repo_id
            t['repo_name'] = task.props.repo_name
            t['state'] = task.props.state
            t['error_str'] = task.props.error_str
            t['worktree'] = task.props.worktree

            tx_task = False
            checkout_task = False
            if task.props.state == "fetch":
                tx_task = seafile_rpc.find_transfer_task(task.props.repo_id)
                t['tx_block_done'] = tx_task.props.block_done
                t['tx_block_total'] = tx_task.props.block_total
            elif task.props.state == "checkout":
                checkout_task = seafile_rpc.get_checkout_task(task.props.repo_id)
                t['checkout_finished_files'] = checkout_task.props.finished_files
                t['checkout_total_files'] = checkout_task.props.total_files
            elif task.props.state == "error" and task.props.error_str == "fetch":
                tx_task = seafile_rpc.find_transfer_task(task.props.repo_id)
                t['tx_error_str'] = tx_task.props.error_str
            elif task.props.state == "error" and task.props.error_str == "password":
                t['relay_id'] = task.props.peer_id

            ts.append(t)

        Tasks = {}
        Tasks['tasks'] = ts
        return json.dumps(Tasks)
        

class repo_download:

    def GET(self):
        inputs = web.webapi.input(relay_id='', token='',
                                  relay_addr='', relay_port = '',
                                  repo_id='', repo_name='',
                                  encrypted='', email='')

        relay_id   = inputs.relay_id
        token       = inputs.token
        relay_addr = inputs.relay_addr
        relay_port = inputs.relay_port
        repo_id     = inputs.repo_id
        repo_name   = inputs.repo_name
        email       = inputs.email

        if seafile_rpc.get_repo(inputs.repo_id):
            return render.repo_download(repo_already_exists=True,
                                        **default_options)
        
        tasks = seafile_rpc.get_clone_tasks()
        for task in tasks:
            if task.props.repo_id == inputs.repo_id:
                if task.props.state != 'done' and task.props.state != 'error' \
                   and task.props.state != 'canceled': 
                    raise web.seeother('/repos/download-tasks/')

        repo_name = urllib.unquote(inputs.repo_name)

        default_seafile_worktree = get_default_seafile_worktree ()
        worktree = seafile_rpc.gen_default_worktree(default_seafile_worktree, repo_name)

        return render.repo_download(error_msg=None,
                                    repo_already_exists=False,
                                    repo_id=inputs.repo_id,
                                    relay_id=inputs.relay_id,
                                    token=token,
                                    relay_addr=relay_addr,
                                    relay_port=relay_port,
                                    repo_name=repo_name,
                                    worktree=worktree,
                                    encrypted=inputs.encrypted,
                                    email=email,
                                    **default_options)

    def POST(self):
        inputs = web.webapi.input(relay_id='', token='',
                                  relay_addr='', relay_port = '',
                                  repo_id='', repo_name='', encrypted='',
                                  password='', worktree='', email='')

        repo_id = inputs.repo_id.strip()

        error_msg = None
        if not inputs.worktree:
            error_msg = _("You must choose a local directory")
        elif inputs.encrypted and not inputs.password:
            error_msg=_("Password can not be empty")
        elif len(repo_id) != 36:
            error_msg=_("Invalid Repo ID")

        if error_msg:
            return render.repo_download (error_msg=error_msg,
                                         repo_already_exists=False,
                                         repo_id=repo_id,
                                         relay_id=inputs.relay_id,
                                         relay_addr=inputs.relay_addr,
                                         relay_port=inputs.relay_port,
                                         token=inputs.token,
                                         repo_name=inputs.repo_name,
                                         encrypted=inputs.encrypted,
                                         password=inputs.password,
                                         worktree=inputs.worktree,
                                         email=inputs.email,
                                         **default_options)

        if not inputs.password:
            inputs.password = None
                                         
        try:
            seafile_rpc.clone (repo_id, inputs.relay_id,
                               inputs.repo_name.encode('utf-8'),
                               inputs.worktree.encode('utf-8'),
                               inputs.token,
                               inputs.password,
                               inputs.relay_addr, inputs.relay_port, inputs.email)
        except SearpcError as e:
            if e.msg == 'Invalid local directory':
                error_msg = _('Invalid local directory')
            elif e.msg == 'Already in sync':
                error_msg = _('The local directory you chose is in sync with another repo. Please choose another one.')
            elif e.msg == 'Invalid local directory name':
                error_msg = _('Invalid local directory name.')
            else:
                error_msg = _('Internal error.') + str(e)

        if error_msg:
            return render.repo_download (error_msg=error_msg,
                                         repo_already_exists=False,
                                         repo_id=repo_id,
                                         relay_id=inputs.relay_id,
                                         relay_addr=inputs.relay_addr,
                                         relay_port=inputs.relay_port,
                                         token=inputs.token,
                                         repo_name=inputs.repo_name,
                                         encrypted=inputs.encrypted,
                                         password=inputs.password,
                                         worktree=inputs.worktree,
                                         email=inputs.email,
                                         **default_options)

        raise web.seeother('/repos/download-tasks/')


class settings_page:

    def GET(self):
        current_prefs = get_current_prefs()
        return render.settings(prefs=current_prefs, **default_options)
        
    def POST(self):
        current_prefs = get_current_prefs()
        inputs = web.webapi.input(auto_start='on', notify_sync='on')

        applet_rpc.set_auto_start(inputs.auto_start)

        if inputs.notify_sync != current_prefs['notify_sync']:
            seafile_rpc.set_config('notify_sync', inputs.notify_sync)

        raise web.seeother('/settings/')


class procs:

    def GET(self):
        aprocs = ccnet_rpc.get_procs_alive(0, -1)
        dprocs = ccnet_rpc.get_procs_dead(0, -1)
        acnt = ccnet_rpc.count_procs_alive()
        dcnt = ccnet_rpc.count_procs_dead()
        return render.procs(aprocs=aprocs, dprocs=dprocs,
                            acnt=acnt, dcnt=dcnt, **default_options)

class i18n:
    def GET(self):
        global lang_in_use
        if lang_in_use == 'zh_CN':
            gettext.translation('messages', localedir,
                                languages=['en_US']).install(True)
            lang_in_use = 'en_US'
        else:
            gettext.translation('messages', localedir,
                                languages=['zh_CN']).install(True)
            lang_in_use = 'zh_CN'

        default_options['lang'] = lang_in_use
        inputs = web.webapi.input(prev='/home/')    

        raise web.seeother(inputs.prev)


# for seahub repo download
class seafile_access_check:
    """For seahub to check whether local seafile is started.

    A web page from seahub can use ajax to check whether local seafile is
    started. Because this is cross site ajax access, we have to return a
    string in jsonp, and the web page should use ajax with result data type
    `jsonp`. The returned value is the version of current repo download
    protocol, which can be used by seahub to judge the version of the
    client.

    """

    def GET(self):
        return 'xx(2)'
        
if __name__ == "__main__":
    app.run()
