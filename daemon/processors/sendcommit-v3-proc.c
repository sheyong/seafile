/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include "common.h"

#include <fcntl.h>

#include <ccnet.h>
#include "net.h"
#include "utils.h"

#include "seafile-session.h"
#include "sendcommit-v3-proc.h"
#include "processors/objecttx-common.h"
#include "vc-common.h"

/*
              seafile-recvcommit-v3
  INIT      --------------------->
                 200 OK
  INIT     <---------------------
                
                  Object
  SEND_OBJ  ----------------------->
                Ack or Bad Object
           <---------------------

                   ...

                    End
           ----------------------->
 */

enum {
    INIT,
    SEND_OBJECT
};

typedef struct  {
    char        end_commit_id[41];
    GList       *id_list;
} SeafileSendcommitProcPriv;

#define GET_PRIV(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAFILE_TYPE_SENDCOMMIT_V3_PROC, SeafileSendcommitProcPriv))

#define USE_PRIV \
    SeafileSendcommitProcPriv *priv = GET_PRIV(processor);

static int send_commit_start (CcnetProcessor *processor, int argc, char **argv);
static void handle_response (CcnetProcessor *processor,
                             char *code, char *code_msg,
                             char *content, int clen);


G_DEFINE_TYPE (SeafileSendcommitV3Proc, seafile_sendcommit_v3_proc, CCNET_TYPE_PROCESSOR)

static void
release_resource (CcnetProcessor *processor)
{
    USE_PRIV;

    if (priv->id_list != NULL)
        string_list_free (priv->id_list);
}

static void
seafile_sendcommit_v3_proc_class_init (SeafileSendcommitV3ProcClass *klass)
{
    CcnetProcessorClass *proc_class = CCNET_PROCESSOR_CLASS (klass);

    proc_class->name = "sendcommit-v3-proc";
    proc_class->start = send_commit_start;
    proc_class->handle_response = handle_response;
    proc_class->release_resource = release_resource;

    g_type_class_add_private (klass, sizeof (SeafileSendcommitProcPriv));
}

static void
seafile_sendcommit_v3_proc_init (SeafileSendcommitV3Proc *processor)
{
}

static int
send_commit_start (CcnetProcessor *processor, int argc, char **argv)
{
    USE_PRIV;
    GString *buf;
    TransferTask *task = ((SeafileSendcommitV3Proc *)processor)->tx_task;

    memcpy (priv->end_commit_id, task->remote_head, 41);

    /* fs_roots can be non-NULL if transfer is resumed from NET_DOWN. */
    if (task->fs_roots != NULL)
        object_list_free (task->fs_roots);
    task->fs_roots = object_list_new ();
    
    buf = g_string_new (NULL);
    g_string_printf (buf, "remote %s seafile-recvcommit-v3 %s %s",
                     processor->peer_id, task->to_branch, task->session_token);
    ccnet_processor_send_request (processor, buf->str);
    g_string_free (buf, TRUE);

    return 0;
}

static void
send_commit (CcnetProcessor *processor, const char *object_id)
{
    char *data;
    int len;
    ObjectPack *pack = NULL;
    int pack_size;

    if (seaf_obj_store_read_obj (seaf->commit_mgr->obj_store,
                                 object_id, (void**)&data, &len) < 0) {
        g_warning ("Failed to read commit %s.\n", object_id);
        goto fail;
    }

    pack_size = sizeof(ObjectPack) + len;
    pack = malloc (pack_size);
    memcpy (pack->id, object_id, 41);
    memcpy (pack->object, data, len);

    ccnet_processor_send_update (processor, SC_OBJECT, SS_OBJECT,
                                 (char *)pack, pack_size);

    g_free (data);
    free (pack);
    return;

fail:
    ccnet_processor_send_update (processor, SC_NOT_FOUND, SS_NOT_FOUND,
                                 object_id, 41);
    ccnet_processor_done (processor, FALSE);
}

static void
send_one_commit (CcnetProcessor *processor)
{
    USE_PRIV;
    char *commit_id;

    if (!priv->id_list) {
        ccnet_processor_send_update (processor, SC_END, SS_END, NULL, 0);
        ccnet_processor_done (processor, TRUE);
        return;
    }

    commit_id = priv->id_list->data;
    priv->id_list = g_list_delete_link (priv->id_list, priv->id_list);

    send_commit (processor, commit_id);

    g_free (commit_id);
}

static gboolean
traverse_commit (SeafCommit *commit, void *data, gboolean *stop)
{
    CcnetProcessor *processor = data;
    TransferTask *task = ((SeafileSendcommitV3Proc *)processor)->tx_task;
    USE_PRIV;

    if (priv->end_commit_id[0] != 0 &&
        strcmp (priv->end_commit_id, commit->commit_id) == 0) {
        *stop = TRUE;
        return TRUE;
    }

    priv->id_list = g_list_prepend (priv->id_list, g_strdup(commit->commit_id));

    if (strcmp (commit->root_id, EMPTY_SHA1) != 0)
        object_list_insert (task->fs_roots, commit->root_id);

    return TRUE;
}

static void
send_commits (CcnetProcessor *processor, const char *head)
{
    gboolean ret;

    ret = seaf_commit_manager_traverse_commit_tree (seaf->commit_mgr,
                                                    head,
                                                    traverse_commit,
                                                    processor);
    if (!ret) {
        ccnet_processor_send_update (processor, SC_NOT_FOUND, SS_NOT_FOUND,
                                     NULL, 0);
        ccnet_processor_done (processor, FALSE);
        return;
    }

    send_one_commit (processor);
}

static void handle_response (CcnetProcessor *processor,
                             char *code, char *code_msg,
                             char *content, int clen)
{
    SeafileSendcommitV3Proc *proc = (SeafileSendcommitV3Proc *)processor;
    TransferTask *task = proc->tx_task;
    if (task->state != TASK_STATE_NORMAL) {
        ccnet_processor_done (processor, TRUE);
        return;
    }

    switch (processor->state) {
    case INIT:
        if (memcmp (code, SC_OK, 3) == 0) {
            processor->state = SEND_OBJECT;
            send_commits (processor, task->head);
        } else {
            g_warning ("Bad response: %s %s.\n", code, code_msg);
            ccnet_processor_done (processor, FALSE);
        }
        break;
    case SEND_OBJECT:
        if (memcmp (code, SC_ACK, 3) == 0) {
            send_one_commit (processor);
        } else {
            g_warning ("[sendcommit] Bad response in state SEND_OBJECT: %s %s\n",
                       code, code_msg);
            ccnet_processor_done (processor, FALSE);
        }
        break;
    default:
        g_assert (0);
    }
}
