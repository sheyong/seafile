/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include "common.h"

#include <fcntl.h>

#include <ccnet.h>
#include "net.h"
#include "utils.h"

#include "seafile-session.h"
#include "putcommit-v2-proc.h"
#include "processors/objecttx-common.h"
#include "vc-common.h"

typedef struct  {
    char        head_commit_id[41];
    char        remote_commit_id[41];
    GList       *id_list;
    guint32     reader_id;
    gboolean    registered;
} SeafilePutcommitProcPriv;

#define GET_PRIV(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAFILE_TYPE_PUTCOMMIT_V2_PROC, SeafilePutcommitProcPriv))

#define USE_PRIV \
    SeafilePutcommitProcPriv *priv = GET_PRIV(processor);

static int put_commit_start (CcnetProcessor *processor, int argc, char **argv);

G_DEFINE_TYPE (SeafilePutcommitV2Proc, seafile_putcommit_v2_proc, CCNET_TYPE_PROCESSOR)

static void
release_resource (CcnetProcessor *processor)
{
    USE_PRIV;

    if (priv->id_list)
        string_list_free (priv->id_list);
    if (priv->registered)
        seaf_obj_store_unregister_async_read (seaf->commit_mgr->obj_store,
                                              priv->reader_id);
}

static void
seafile_putcommit_v2_proc_class_init (SeafilePutcommitV2ProcClass *klass)
{
    CcnetProcessorClass *proc_class = CCNET_PROCESSOR_CLASS (klass);

    proc_class->name = "putcommit-v2-proc";
    proc_class->start = put_commit_start;
    proc_class->release_resource = release_resource;

    g_type_class_add_private (klass, sizeof (SeafilePutcommitProcPriv));
}

static void
seafile_putcommit_v2_proc_init (SeafilePutcommitV2Proc *processor)
{
}

static void
send_commit (CcnetProcessor *processor,
             const char *commit_id,
             char *data, int len)
{
    ObjectPack *pack = NULL;
    int pack_size;

    pack_size = sizeof(ObjectPack) + len;
    pack = malloc (pack_size);
    memcpy (pack->id, commit_id, 41);
    memcpy (pack->object, data, len);

    ccnet_processor_send_response (processor, SC_OBJECT, SS_OBJECT,
                                   (char *)pack, pack_size);
    free (pack);
}

static int
read_and_send_commit (CcnetProcessor *processor)
{
    char *id;
    USE_PRIV;

    id = priv->id_list->data;
    priv->id_list = g_list_delete_link (priv->id_list, priv->id_list);

    if (seaf_obj_store_async_read (seaf->commit_mgr->obj_store,
                                   priv->reader_id,
                                   id) < 0) {
        g_warning ("[putcommit] Failed to start read of %s.\n", id);
        ccnet_processor_send_response (processor, SC_NOT_FOUND, SS_NOT_FOUND,
                                       NULL, 0);
        ccnet_processor_done (processor, FALSE);
        g_free (id);
        return -1;
    }

    g_free (id);
    return 0;
}

static void
read_done_cb (OSAsyncResult *res, void *cb_data)
{
    CcnetProcessor *processor = cb_data;
    USE_PRIV;

    if (!res->success) {
        g_warning ("[putcommit] Failed to read %s.\n", res->obj_id);
        goto bad;
    }

    send_commit (processor, res->obj_id, res->data, res->len);

    /* Send next commit. */
    if (priv->id_list != NULL)
        read_and_send_commit (processor);
    else {
        ccnet_processor_send_response (processor, SC_END, SS_END, NULL, 0);
        ccnet_processor_done (processor, TRUE);
    }

    return;

bad:
    ccnet_processor_send_response (processor, SC_NOT_FOUND, SS_NOT_FOUND,
                                   NULL, 0);
    ccnet_processor_done (processor, FALSE);
}

static gboolean
collect_id (SeafCommit *commit, void *data, gboolean *stop)
{
    CcnetProcessor *processor = data;
    USE_PRIV;

    if (g_strcmp0 (commit->commit_id, priv->remote_commit_id) == 0) {
        *stop = TRUE;
        return TRUE;
    }

    priv->id_list = g_list_prepend (priv->id_list, g_strdup(commit->commit_id));
    return TRUE;
}

static void *
collect_commit_id_thread (void *vprocessor)
{
    CcnetProcessor *processor = vprocessor;
    USE_PRIV;

    if (seaf_commit_manager_traverse_commit_tree (seaf->commit_mgr,
                                                  priv->head_commit_id,
                                                  collect_id,
                                                  processor) < 0) {
        g_warning ("[putcommit] Failed to collect commit id.\n");
        string_list_free (priv->id_list);
        priv->id_list = NULL;
        return vprocessor;
    }

    priv->id_list = g_list_reverse (priv->id_list);
    return vprocessor;
}

static void
collect_commit_id_done (void *vprocessor)
{
    CcnetProcessor *processor = vprocessor;
    USE_PRIV;

    if (processor->delay_shutdown) {
        ccnet_processor_done (processor, FALSE);
        return;
    }

    if (!priv->id_list) {
        ccnet_processor_send_response (processor, SC_NOT_FOUND, SS_NOT_FOUND,
                                       NULL, 0);
        ccnet_processor_done (processor, FALSE);
        return;
    }

    read_and_send_commit (processor);
}

static int
put_commit_start (CcnetProcessor *processor, int argc, char **argv)
{
    char *head_id, *remote_id = NULL;
    char *session_token;
    USE_PRIV;

    if (argc < 2) {
        ccnet_processor_send_response (processor, SC_BAD_ARGS, SS_BAD_ARGS, NULL, 0);
        ccnet_processor_done (processor, FALSE);
        return -1;
    }

    if (argc == 2) {
        head_id = argv[0];
        session_token = argv[1];
    } else if (argc >= 3) {
        head_id = argv[0];
        remote_id = argv[1];
        session_token = argv[2];
    }

    if (strlen(head_id) != 40 || (remote_id && strlen(remote_id) != 40)) {
        ccnet_processor_send_response (processor, SC_BAD_ARGS, SS_BAD_ARGS, NULL, 0);
        ccnet_processor_done (processor, FALSE);
        return -1;
    }

    if (seaf_token_manager_verify_token (seaf->token_mgr,
                                         processor->peer_id,
                                         session_token, NULL) < 0) {
        ccnet_processor_send_response (processor, 
                                       SC_ACCESS_DENIED, SS_ACCESS_DENIED,
                                       NULL, 0);
        ccnet_processor_done (processor, FALSE);
        return -1;
    }

    memcpy (priv->head_commit_id, head_id, 41);
    if (remote_id != NULL)
        memcpy (priv->remote_commit_id, remote_id, 41);
    ccnet_processor_send_response (processor, SC_OK, SS_OK, NULL, 0);

    priv->reader_id =
        seaf_obj_store_register_async_read (seaf->commit_mgr->obj_store,
                                            read_done_cb,
                                            processor);
    priv->registered = TRUE;

    ccnet_processor_thread_create (processor,
                                   collect_commit_id_thread,
                                   collect_commit_id_done,
                                   processor);

    return 0;
}
