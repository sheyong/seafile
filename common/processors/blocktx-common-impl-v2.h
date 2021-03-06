#ifndef BLOCKTX_COMMON_IMPL_V2_H
#define BLOCKTX_COMMON_IMPL_V2_H

#include "utils.h"
#include "log.h"

#define SC_SEND_PORT    "301"
#define SS_SEND_PORT    "PORT"
#define SC_GET_PORT     "302"
#define SS_GET_PORT     "GET PORT"
#define SC_GET_BLOCK    "303"
#define SS_GET_BLOCK    "GET BLOCK"
#define SC_BBITMAP      "304"
#define SS_BBITMAP      "BLOCK BITMAP"
#define SC_ACK          "305"
#define SS_ACK          "BLOCK OK"
#define SC_BLOCKLIST    "306"
#define SS_BLOCKLIST    "BLOCK LIST"

#define SC_BAD_BLK_REQ      "405"
#define SS_BAD_BLK_REQ      "BAD BLOCK REQUEST"
#define SC_BAD_BL           "408"
#define SS_BAD_BL           "BAD BLOCK LIST"

#define SC_ACCESS_DENIED "410"
#define SS_ACCESS_DENIED "Access denied"

#define MAX_BL_LEN 1024

typedef struct {
    int     block_idx;
    char    block_id[41];
} BlockRequest;

typedef struct {
    int      block_idx;
    int      tx_bytes;
    int      tx_time;
} BlockResponse;

typedef struct {
    uint32_t block_size;
    uint32_t block_idx;
    char     block_id[41];
} __attribute__((__packed__)) BlockPacket;

typedef struct ThreadData ThreadData;

/* function called when receiving event from transfer thread via pipe. */
typedef void (*ThreadEventHandler) (CEvent *event, void *vprocessor);
typedef int  (*TransferFunc) (ThreadData *tdata);

struct ThreadData {
    CcnetPeer           *peer;
    CcnetProcessor      *processor;
    uint32_t             cevent_id;
    ccnet_pipe_t         task_pipe[2];
    int                  port;
    evutil_socket_t      data_fd;

    gboolean             processor_done;
    char                *token;
    TransferFunc         transfer_func;
    int                  thread_ret;
};

typedef struct {
    ThreadData      *tdata;
    int              bm_offset;
    GHashTable      *block_hash;
} BlockProcPriv;

/*
 * Common code for processor start and release_resource functions.
 */

static void
prepare_thread_data (CcnetProcessor *processor,
                     TransferFunc tranfer_func,
                     ThreadEventHandler handler)
{
    USE_PRIV;

    priv->tdata = g_new0 (ThreadData, 1);
    priv->tdata->task_pipe[0] = -1;
    priv->tdata->task_pipe[1] = -1;
    priv->tdata->transfer_func = tranfer_func;
    priv->tdata->processor = processor;

    priv->tdata->cevent_id = cevent_manager_register (seaf->ev_mgr,
                                                      handler,
                                                      processor);
}

static void
release_thread (CcnetProcessor *processor)
{
    USE_PRIV;

    if (priv->tdata) {
        /* The read end will be closed by worker thread. */
        if (priv->tdata->task_pipe[1] >= 0)
            pipeclose (priv->tdata->task_pipe[1]);

        priv->tdata->processor_done = TRUE;
        cevent_manager_unregister (seaf->ev_mgr, priv->tdata->cevent_id);
    }
}

static void
thread_done (void *vtdata)
{
    ThreadData *tdata = vtdata;

    /* When the worker thread returns, the processor may have been
     * released. tdata->processor_done will be set to TRUE in
     * release_resource().
     *
     * Note: thread_done() and release_thread() are both called
     * in main thread, so there are only two cases:
     * 1) thread_done() is called before release_resource(), then release_thread()
     *    is called within thread_done()
     * 2) release_thread() is called before thread_done(), then tdata->processor_done
     *    is set.
     */
    if (!tdata->processor_done) {
        g_debug ("Processor is not released. Release it now.\n");
        if (tdata->thread_ret == 0)
            ccnet_processor_done (tdata->processor, TRUE);
        else
            ccnet_processor_done (tdata->processor, FALSE);
    }

    g_free (tdata->token);
    g_free (tdata);
}


/*
 * Common code for block transfer.
 */

inline static uint64_t
get_time_microseconds ()
{
    struct timeval tv;
    uint64_t ret;

    gettimeofday (&tv, NULL);

    ret = tv.tv_sec*1000000 + tv.tv_usec;

    return ret;
}

static void
send_block_rsp (int cevent_id, int block_idx, int tx_bytes, int tx_time)
{
    BlockResponse *blk_rsp = g_new0 (BlockResponse, 1);
    blk_rsp->block_idx = block_idx;
    blk_rsp->tx_bytes = tx_bytes;
    blk_rsp->tx_time = tx_time;
    cevent_manager_add_event (seaf->ev_mgr, 
                              cevent_id,
                              (void *)blk_rsp);
}

#if defined SENDBLOCK_PROC || defined PUTBLOCK_PROC

static int
send_block_packet (ThreadData *tdata,
                   int block_idx,
                   const char *block_id,
                   BlockHandle *handle, 
                   evutil_socket_t sockfd)
{
    SeafBlockManager *block_mgr = seaf->block_mgr;
    BlockMetadata *md;
    uint32_t size;
    BlockPacket pkt;
    char buf[1024];
    int n;
#if defined SENDBLOCK_PROC
    guint64 t_start, t_end;
    int delta = 0;
#endif

    md = seaf_block_manager_stat_block_by_handle (block_mgr, handle);
    if (!md) {
        g_warning ("Failed to stat block %s.\n", block_id);
        return -1;
    }
    size = md->size;
    g_free (md);

    pkt.block_size = htonl (size);
    pkt.block_idx = htonl ((uint32_t) block_idx);
    memcpy (pkt.block_id, block_id, 41);
    if (sendn (sockfd, &pkt, sizeof(pkt)) < 0) {
        g_warning ("Failed to write socket: %s.\n", 
                   evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
        return -1;
    }

#if defined SENDBLOCK_PROC
    t_start = get_time_microseconds ();
#endif
    while (1) {
        n = seaf_block_manager_read_block (block_mgr, handle, buf, 1024);
        if (n <= 0)
            break;
        if (sendn (sockfd, buf, n) < 0) {
            g_warning ("Failed to write block %s: %s.\n", block_id, 
                       evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
            return -1;
        }

#if defined SENDBLOCK_PROC
        delta += n;
        t_end = get_time_microseconds ();
        if (t_end - t_start >= 1000000) {
            send_block_rsp (tdata->cevent_id, -1, delta, (int)(t_end - t_start));
            t_start = t_end;
            delta = 0;
        }
#endif
    }
    if (n < 0) {
        g_warning ("Failed to write block %s: %s.\n", block_id, 
                   evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
        return -1;
    }

#if defined SENDBLOCK_PROC
    send_block_rsp (tdata->cevent_id, -1, delta, (int)(t_end - t_start));
#endif

    return size;
}

static int
send_blocks (ThreadData *tdata)
{
    SeafBlockManager *block_mgr = seaf->block_mgr;
    BlockRequest blk_req;
    BlockHandle *handle;
    int         n;
    int         n_sent;

    while (1) {
        n = pipereadn (tdata->task_pipe[0], &blk_req, sizeof(blk_req));
        if (n == 0) {
            g_debug ("Processor exited. I will exit now.\n");
            return -1;
        }
        if (n != sizeof(blk_req)) {
            g_warning ("read task pipe incorrect.\n");
            return -1;
        }

        handle = seaf_block_manager_open_block (block_mgr, 
                                                blk_req.block_id, BLOCK_READ);
        if (!handle) {
            g_warning ("[send block] failed to open block %s.\n", 
                       blk_req.block_id);
            return -1;
        }

        n_sent = send_block_packet (tdata, blk_req.block_idx, blk_req.block_id, 
                                    handle, tdata->data_fd);
        if (n_sent < 0)
            return -1;

        seaf_block_manager_close_block (block_mgr, handle);
        seaf_block_manager_block_handle_free (block_mgr, handle);
    }

    return 0;
}

#endif  /* defined SENDBLOCK_PROC || defined PUTBLOCK_PROC */

#if defined GETBLOCK_PROC || defined RECVBLOCK_PROC

enum {
    RECV_STATE_HEADER,
    RECV_STATE_BLOCK,
};

typedef struct {
    int state;
    BlockPacket hdr;
    int remain;
    int delta;
    BlockHandle *handle;
    uint64_t t_start;
    uint32_t cevent_id;
} RecvFSM;

static int
recv_tick (RecvFSM *fsm, evutil_socket_t sockfd)
{
    SeafBlockManager *block_mgr = seaf->block_mgr;
    char *block_id;
    BlockHandle *handle;
    int n, round;
    char buf[1024];
    uint64_t t_end;

    switch (fsm->state) {
    case RECV_STATE_HEADER:
        n = recv (sockfd, 
                  (char *)&fsm->hdr + sizeof(BlockPacket) - fsm->remain, 
                  fsm->remain, 0);
        if (n <= 0) {
            g_warning ("Failed to read block pkt: %s.\n",
                       evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
            return -1;
        }
        fsm->remain -= n;
        if (fsm->remain == 0) {
            fsm->remain = (int) ntohl (fsm->hdr.block_size);
            block_id = fsm->hdr.block_id;
            block_id[40] = 0;

            handle = seaf_block_manager_open_block (block_mgr, 
                                                    block_id, BLOCK_WRITE);
            if (!handle) {
                g_warning ("failed to open block %s.\n", block_id);
                return -1;
            }
            fsm->handle = handle; 
            fsm->t_start = get_time_microseconds ();
            fsm->state = RECV_STATE_BLOCK;
        }
        break;
    case RECV_STATE_BLOCK:
        handle = fsm->handle;
        block_id = fsm->hdr.block_id;

        round = MIN (fsm->remain, 1024);
        n = recv (sockfd, buf, round, 0);
        if (n < 0) {
            g_warning ("failed to read data: %s.\n",
                       evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
            return -1;
        } else if (n == 0) {
            g_debug ("data connection closed.\n");
            return -1;
        }

        fsm->delta += n;
        t_end = get_time_microseconds ();
        if (t_end - fsm->t_start >= 1000000) {
#if defined GETBLOCK_PROC
            send_block_rsp (fsm->cevent_id, -1, fsm->delta,
                            (int)(t_end - fsm->t_start));
#endif
            fsm->t_start = t_end;
            fsm->delta = 0;
        }

        if (seaf_block_manager_write_block (block_mgr, handle, buf, n) < 0) {
            g_warning ("Failed to write block %s.\n", fsm->hdr.block_id);
            return -1;
        }

        fsm->remain -= n;
        if (fsm->remain == 0) {
            if (seaf_block_manager_close_block (block_mgr, handle) < 0) {
                g_warning ("Failed to close block %s.\n", fsm->hdr.block_id);
                seaf_block_manager_block_handle_free (seaf->block_mgr, handle);
                return -1;
            }

            if (seaf_block_manager_commit_block (block_mgr, handle) < 0) {
                g_warning ("Failed to commit block %s.\n", fsm->hdr.block_id);
                seaf_block_manager_block_handle_free (seaf->block_mgr, handle);
                return -1;
            }

            seaf_block_manager_block_handle_free (block_mgr, handle);

            /* Notify finish receiving this block. */
            send_block_rsp (fsm->cevent_id,
                            (int)ntohl (fsm->hdr.block_idx),
                            fsm->delta, (int)(t_end - fsm->t_start));

            /* Prepare for the next packet. */
            fsm->state = RECV_STATE_HEADER;
            fsm->remain = sizeof(BlockPacket);
        }
        break;
    }

    return 0;
}

static int
recv_blocks (ThreadData *tdata)
{
    fd_set fds;
    int max_fd = MAX (tdata->task_pipe[0], tdata->data_fd);
    int rc;

    RecvFSM *fsm = g_new0 (RecvFSM, 1);
    fsm->remain = sizeof (BlockPacket);
    fsm->cevent_id = tdata->cevent_id;

    while (1) {
        FD_ZERO (&fds);
        FD_SET (tdata->task_pipe[0], &fds);
        FD_SET (tdata->data_fd, &fds);

        rc = select (max_fd + 1, &fds, NULL, NULL, NULL);
        if (rc < 0 && errno == EINTR) {
            continue;
        } else if (rc < 0) {
            g_warning ("select error: %s.\n", strerror(errno));
            g_free (fsm);
            return -1;
        }

        if (FD_ISSET (tdata->data_fd, &fds)) {
            if (recv_tick (fsm, tdata->data_fd) < 0) {
                g_free (fsm);
                return -1;
            }
        }

        if (FD_ISSET (tdata->task_pipe[0], &fds)) {
            /* task_pipe becomes readable only when the write end
             * is closed, in this case 0 is returned.
             * This means the processor was done.
             */
            char buf[1];
            int n = piperead (tdata->task_pipe[0], buf, sizeof(buf));
            g_assert (n == 0);
            g_debug ("task pipe closed. exit now.\n");
            g_free (fsm);
            return -1;
        }
    }

    g_free (fsm);
    return 0;
}

#endif  /* defined GETBLOCK_PROC || defined RECVBLOCK_PROC */

#if defined GETBLOCK_PROC || defined SENDBLOCK_PROC

static int
master_block_proc_start (CcnetProcessor *processor,
                         TransferTask *tx_task,
                         const char *remote_processor_name,
                         Bitfield *active,
                         Bitfield *block_bitmap)
{
    GString *buf;
    if (!tx_task || !tx_task->session_token) {
        g_warning ("transfer task not set.\n");
        return -1;
    }

    BitfieldConstruct (active,
                       tx_task->block_list->block_map.bitCount);
    BitfieldConstruct (block_bitmap,
                       tx_task->block_list->block_map.bitCount);

    buf = g_string_new (NULL);
    g_string_printf (buf, "remote %s %s %s", 
                     processor->peer_id,
                     remote_processor_name,
                     tx_task->session_token);
                         
    ccnet_processor_send_request (processor, buf->str);
    g_string_free (buf, TRUE);

    return 0;
}

static void
descruct_bitfield (Bitfield *block_bitmap,
                   Bitfield *active,
                   TransferTask *tx_task)
{
    BitfieldDestruct (block_bitmap);
    /*
     * Set all the queued blocks to inactive so that they can be rescheduled.
     */
    if (tx_task->state == TASK_STATE_NORMAL)
        BitfieldDifference (&tx_task->active, active);
    BitfieldDestruct (active);
}

static void* do_transfer(void *vtdata)
{
    ThreadData *tdata = vtdata;

    struct sockaddr_storage addr;
    struct sockaddr *sa  = (struct sockaddr*) &addr;
    socklen_t sa_len = sizeof (addr);
    evutil_socket_t data_fd;

    CcnetPeer *peer = tdata->peer;

    if (peer->addr_str == NULL) {
        g_warning ("peer address is NULL\n");
        tdata->thread_ret = -1;
        goto out;
    }

    if (sock_pton(peer->addr_str, tdata->port, &addr) < 0) {
        g_warning ("wrong address format %s\n", peer->addr_str);
        tdata->thread_ret = -1;
        goto out;
    }

    if ((data_fd = socket(sa->sa_family, SOCK_STREAM, 0)) < 0) {
        g_warning ("socket error: %s\n", strerror(errno));
        tdata->thread_ret = -1;
        goto out;
    }

#ifdef __APPLE__
    if (sa->sa_family == AF_INET)
        sa_len = sizeof(struct sockaddr_in);
    else if (sa->sa_family == PF_INET6)
        sa_len = sizeof(struct sockaddr_in6);
#endif

    if (connect(data_fd, sa, sa_len) < 0) {
        g_warning ("connect error: %s\n", strerror(errno));
        evutil_closesocket (data_fd);
        tdata->thread_ret = -1;
        goto out;
    }

    int token_len = strlen(tdata->token) + 1;
    if (sendn (data_fd, tdata->token, token_len) != token_len) {
        g_warning ("send connection token error: %s\n", strerror(errno));
        evutil_closesocket (data_fd);
        tdata->thread_ret = -1;
        goto out;
    }

    tdata->data_fd = data_fd;
    tdata->processor->state = ESTABLISHED;

    tdata->thread_ret = tdata->transfer_func(tdata);

    evutil_closesocket (tdata->data_fd);

out:
    pipeclose (tdata->task_pipe[0]);
    g_object_unref (peer);

    return vtdata;
}

static void
get_port (CcnetProcessor *processor, char *content, int clen)
{
    USE_PRIV;
    ThreadData *tdata = priv->tdata;
    char *p, *port_str, *token;

    if (content[clen-1] != '\0') {
        g_warning ("Bad port and token\n");
        ccnet_processor_done (processor, FALSE);
        return;
    }

    p = strchr (content, '\t');
    if (!p) {
        g_warning ("Bad port and token\n");
        ccnet_processor_done (processor, FALSE);
        return;
    }

    *p = '\0';
    port_str = content; token = p + 1;

    CcnetPeer *peer = ccnet_get_peer (seaf->ccnetrpc_client, processor->peer_id);
    if (!peer) {
        g_warning ("Invalid peer %s.\n", processor->peer_id);
        g_free (tdata);
        ccnet_processor_done (processor, FALSE);
        return;
    }
    /* Store peer address so that we don't need to call ccnet_get_peer()
     * in the worker thread later.
     */
    if (ccnet_pipe (tdata->task_pipe) < 0) {
        g_warning ("failed to create task pipe.\n");
        g_free (tdata);
        ccnet_processor_done (processor, FALSE);
        return;
    }
    
    tdata->port = atoi (port_str);
    tdata->token = g_strdup(token);
    tdata->peer = peer;

    ccnet_job_manager_schedule_job (seaf->job_mgr,
                                    do_transfer,
                                    thread_done,
                                    tdata);
}

static void
send_block_list (CcnetProcessor *processor)
{
#ifdef SENDBLOCK_PROC
    SeafileSendblockV2Proc *proc = (SeafileSendblockV2Proc *)processor;
#else
    SeafileGetblockV2Proc *proc = (SeafileGetblockV2Proc *)processor;
#endif
    BlockList *bl = proc->tx_task->block_list;
    int i, n = 0;
    char buf[MAX_BL_LEN * 41];
    int len = 0;

    for (i = 0; i < bl->n_blocks; ++i) {
        memcpy (&buf[len], g_ptr_array_index(bl->block_ids, i), 41);
        len += 41;

        if (++n == MAX_BL_LEN) {
            ccnet_processor_send_update (processor, SC_BLOCKLIST, SS_BLOCKLIST,
                                         (char *)buf, len);
            n = 0;
            len = 0;
        }
    }

    if (n != 0)
        ccnet_processor_send_update (processor, SC_BLOCKLIST, SS_BLOCKLIST,
                                     (char *)buf, len);
}

static int
process_block_bitmap (CcnetProcessor *processor, char *content, int clen)
{
#ifdef SENDBLOCK_PROC
    SeafileSendblockV2Proc *proc = (SeafileSendblockV2Proc *)processor;
#else
    SeafileGetblockV2Proc *proc = (SeafileGetblockV2Proc *)processor;
#endif
    USE_PRIV;

    if (proc->block_bitmap.byteCount < priv->bm_offset + clen) {
        g_warning ("Received block bitmap is too large.\n");
        ccnet_processor_done (processor, FALSE);
        return -1;
    }
    memcpy (proc->block_bitmap.bits + priv->bm_offset, content, clen);

    priv->bm_offset += clen;
    if (priv->bm_offset == proc->block_bitmap.byteCount) {
#ifdef SENDBLOCK_PROC
        /* Update global uploaded bitmap. */
        BitfieldOr (&proc->tx_task->uploaded, &proc->block_bitmap);
        proc->tx_task->n_uploaded = BitfieldCountTrueBits (&proc->tx_task->uploaded);
#endif
        ccnet_processor_send_update (processor, SC_GET_PORT, SS_GET_PORT,
                                     NULL, 0);
        processor->state = GET_PORT;
    }

    return 0;
}

#endif  /* defined SENDBLOCK_PROC || GETBLOCK_PROC */

#if defined RECVBLOCK_PROC || defined PUTBLOCK_PROC

static int
verify_session_token (CcnetProcessor *processor, int argc, char **argv)
{
    if (argc != 1) {
        return -1;
    }

    char *session_token = argv[0];
    if (seaf_token_manager_verify_token (seaf->token_mgr,
                                         processor->peer_id,
                                         session_token, NULL) < 0) {
        return -1;
    }

    return 0;
}


static void* do_passive_transfer(void *vtdata)
{
    ThreadData *tdata = vtdata;

    tdata->thread_ret = tdata->transfer_func (tdata);
    
    pipeclose (tdata->task_pipe[0]);
    evutil_closesocket (tdata->data_fd);
    
    return vtdata;
}

static void
accept_connection (evutil_socket_t connfd, void *vdata)
{
    ThreadData *tdata = vdata;
    CcnetProcessor *processor = tdata->processor;

    /* client error or timeout */
    if (connfd < 0)
        goto fail;

    tdata->data_fd = connfd;

    processor->state = ESTABLISHED;

    if (ccnet_pipe (tdata->task_pipe) < 0) {
        g_warning ("failed to create task pipe.\n");
        evutil_closesocket (tdata->data_fd);
        goto fail;
    }

    ccnet_job_manager_schedule_job (seaf->job_mgr,
                                    do_passive_transfer,
                                    thread_done,
                                    tdata);
    return;

fail:
    ccnet_processor_done (processor, FALSE);
    g_free (tdata);
}

static void
send_port (CcnetProcessor *processor)
{
    USE_PRIV;
    char buf[256];
    char *token = NULL;
    int len;

    token = seaf_listen_manager_generate_token (seaf->listen_mgr);
    if (seaf_listen_manager_register_token (seaf->listen_mgr, token,
                        (ConnAcceptedCB)accept_connection,
                        priv->tdata, 10) < 0) {
        seaf_warning ("failed to register token\n");
        g_free (token);
        ccnet_processor_done (processor, FALSE);
    }

    len = snprintf (buf, sizeof(buf), "%d\t%s", seaf->listen_mgr->port, token);
    ccnet_processor_send_response (processor,
                                   SC_SEND_PORT, SS_SEND_PORT,
                                   buf, len+1);

    g_free (token);
}


static void
process_block_list (CcnetProcessor *processor, char *content, int clen)
{
    char *block_id;
    int n_blocks;
    Bitfield bitmap;
    int i;

    if (clen % 41 != 0) {
        g_warning ("Bad block list.\n");
        ccnet_processor_send_response (processor, SC_BAD_BL, SS_BAD_BL, NULL, 0);
        ccnet_processor_done (processor, FALSE);
        return;
    }

    n_blocks = clen/41;
    BitfieldConstruct (&bitmap, n_blocks);

    block_id = content;
    for (i = 0; i < n_blocks; ++i) {
        block_id[40] = '\0';
        if (seaf_block_manager_block_exists(seaf->block_mgr, block_id))
            BitfieldAdd (&bitmap, i);
        block_id += 41;
    }

    ccnet_processor_send_response (processor, SC_BBITMAP, SS_BBITMAP,
                                   (char *)(bitmap.bits), bitmap.byteCount);
    BitfieldDestruct (&bitmap);
}

#endif  /* defined RECVBLOCK_PROC || defined PUTBLOCK_PROC */

#endif  /* BLOCKTX_COMMON_IMPL_V2_H */
