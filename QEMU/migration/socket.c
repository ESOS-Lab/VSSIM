/*
 * QEMU live migration via socket
 *
 * Copyright Red Hat, Inc. 2009-2016
 *
 * Authors:
 *  Chris Lalancette <clalance@redhat.com>
 *  Daniel P. Berrange <berrange@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 */

#include "qemu/osdep.h"

#include "qemu-common.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "migration/migration.h"
#include "migration/qemu-file.h"
#include "io/channel-socket.h"
#include "trace.h"


static SocketAddress *tcp_build_address(const char *host_port, Error **errp)
{
    InetSocketAddress *iaddr = inet_parse(host_port, errp);
    SocketAddress *saddr;

    if (!iaddr) {
        return NULL;
    }

    saddr = g_new0(SocketAddress, 1);
    saddr->type = SOCKET_ADDRESS_KIND_INET;
    saddr->u.inet.data = iaddr;

    return saddr;
}


static SocketAddress *unix_build_address(const char *path)
{
    SocketAddress *saddr;

    saddr = g_new0(SocketAddress, 1);
    saddr->type = SOCKET_ADDRESS_KIND_UNIX;
    saddr->u.q_unix.data = g_new0(UnixSocketAddress, 1);
    saddr->u.q_unix.data->path = g_strdup(path);

    return saddr;
}


struct SocketConnectData {
    MigrationState *s;
    char *hostname;
};

static void socket_connect_data_free(void *opaque)
{
    struct SocketConnectData *data = opaque;
    if (!data) {
        return;
    }
    g_free(data->hostname);
    g_free(data);
}

static void socket_outgoing_migration(QIOTask *task,
                                      gpointer opaque)
{
    struct SocketConnectData *data = opaque;
    QIOChannel *sioc = QIO_CHANNEL(qio_task_get_source(task));
    Error *err = NULL;

    if (qio_task_propagate_error(task, &err)) {
        trace_migration_socket_outgoing_error(error_get_pretty(err));
        data->s->to_dst_file = NULL;
        migrate_fd_error(data->s, err);
        error_free(err);
    } else {
        trace_migration_socket_outgoing_connected(data->hostname);
        migration_channel_connect(data->s, sioc, data->hostname);
    }
    object_unref(OBJECT(sioc));
}

static void socket_start_outgoing_migration(MigrationState *s,
                                            SocketAddress *saddr,
                                            Error **errp)
{
    QIOChannelSocket *sioc = qio_channel_socket_new();
    struct SocketConnectData *data = g_new0(struct SocketConnectData, 1);

    data->s = s;
    if (saddr->type == SOCKET_ADDRESS_KIND_INET) {
        data->hostname = g_strdup(saddr->u.inet.data->host);
    }

    qio_channel_set_name(QIO_CHANNEL(sioc), "migration-socket-outgoing");
    qio_channel_socket_connect_async(sioc,
                                     saddr,
                                     socket_outgoing_migration,
                                     data,
                                     socket_connect_data_free);
    qapi_free_SocketAddress(saddr);
}

void tcp_start_outgoing_migration(MigrationState *s,
                                  const char *host_port,
                                  Error **errp)
{
    Error *err = NULL;
    SocketAddress *saddr = tcp_build_address(host_port, &err);
    if (!err) {
        socket_start_outgoing_migration(s, saddr, &err);
    }
    error_propagate(errp, err);
}

void unix_start_outgoing_migration(MigrationState *s,
                                   const char *path,
                                   Error **errp)
{
    SocketAddress *saddr = unix_build_address(path);
    socket_start_outgoing_migration(s, saddr, errp);
}


static gboolean socket_accept_incoming_migration(QIOChannel *ioc,
                                                 GIOCondition condition,
                                                 gpointer opaque)
{
    QIOChannelSocket *sioc;
    Error *err = NULL;

    sioc = qio_channel_socket_accept(QIO_CHANNEL_SOCKET(ioc),
                                     &err);
    if (!sioc) {
        error_report("could not accept migration connection (%s)",
                     error_get_pretty(err));
        goto out;
    }

    trace_migration_socket_incoming_accepted();

    qio_channel_set_name(QIO_CHANNEL(sioc), "migration-socket-incoming");
    migration_channel_process_incoming(migrate_get_current(),
                                       QIO_CHANNEL(sioc));
    object_unref(OBJECT(sioc));

out:
    /* Close listening socket as its no longer needed */
    qio_channel_close(ioc, NULL);
    return FALSE; /* unregister */
}


static void socket_start_incoming_migration(SocketAddress *saddr,
                                            Error **errp)
{
    QIOChannelSocket *listen_ioc = qio_channel_socket_new();

    qio_channel_set_name(QIO_CHANNEL(listen_ioc),
                         "migration-socket-listener");

    if (qio_channel_socket_listen_sync(listen_ioc, saddr, errp) < 0) {
        object_unref(OBJECT(listen_ioc));
        qapi_free_SocketAddress(saddr);
        return;
    }

    qio_channel_add_watch(QIO_CHANNEL(listen_ioc),
                          G_IO_IN,
                          socket_accept_incoming_migration,
                          listen_ioc,
                          (GDestroyNotify)object_unref);
    qapi_free_SocketAddress(saddr);
}

void tcp_start_incoming_migration(const char *host_port, Error **errp)
{
    Error *err = NULL;
    SocketAddress *saddr = tcp_build_address(host_port, &err);
    if (!err) {
        socket_start_incoming_migration(saddr, &err);
    }
    error_propagate(errp, err);
}

void unix_start_incoming_migration(const char *path, Error **errp)
{
    SocketAddress *saddr = unix_build_address(path);
    socket_start_incoming_migration(saddr, errp);
}
