/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "qemu/osdep.h"

#include "net/net.h"
#include "clients.h"
#include "monitor/monitor.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "qemu/error-report.h"
#include "qemu/option.h"
#include "qemu/sockets.h"
#include "qemu/iov.h"
#include "qemu/main-loop.h"

typedef struct NetSocketState {
    NetClientState nc;
    int listen_fd;
    int fd;
    SocketReadState rs;
    unsigned int send_index;      /* number of bytes sent (only SOCK_STREAM) */
    struct sockaddr_in dgram_dst; /* contains inet host and port destination iff connectionless (SOCK_DGRAM) */
    IOHandler *send_fn;           /* differs between SOCK_STREAM/SOCK_DGRAM */
    bool read_poll;               /* waiting to receive data? */
    bool write_poll;              /* waiting to transmit data? */
} NetSocketState;

static void net_socket_accept(void *opaque);
static void net_socket_writable(void *opaque);

static void net_socket_update_fd_handler(NetSocketState *s)
{
    qemu_set_fd_handler(s->fd,
                        s->read_poll ? s->send_fn : NULL,
                        s->write_poll ? net_socket_writable : NULL,
                        s);
}

static void net_socket_read_poll(NetSocketState *s, bool enable)
{
    s->read_poll = enable;
    net_socket_update_fd_handler(s);
}

static void net_socket_write_poll(NetSocketState *s, bool enable)
{
    s->write_poll = enable;
    net_socket_update_fd_handler(s);
}

static void net_socket_writable(void *opaque)
{
    NetSocketState *s = opaque;

    net_socket_write_poll(s, false);

    qemu_flush_queued_packets(&s->nc);
}

static ssize_t net_socket_receive(NetClientState *nc, const uint8_t *buf, size_t size)
{
    NetSocketState *s = DO_UPCAST(NetSocketState, nc, nc);
    uint32_t len = htonl(size);
    struct iovec iov[] = {
        {
            .iov_base = &len,
            .iov_len  = sizeof(len),
        }, {
            .iov_base = (void *)buf,
            .iov_len  = size,
        },
    };
    size_t remaining;
    ssize_t ret;

    remaining = iov_size(iov, 2) - s->send_index;
    ret = iov_send(s->fd, iov, 2, s->send_index, remaining);

    if (ret == -1 && errno == EAGAIN) {
        ret = 0; /* handled further down */
    }
    if (ret == -1) {
        s->send_index = 0;
        return -errno;
    }
    if (ret < (ssize_t)remaining) {
        s->send_index += ret;
        net_socket_write_poll(s, true);
        return 0;
    }
    s->send_index = 0;
    return size;
}

static ssize_t net_socket_receive_dgram(NetClientState *nc, const uint8_t *buf, size_t size)
{
    NetSocketState *s = DO_UPCAST(NetSocketState, nc, nc);
    ssize_t ret;

    do {
        ret = qemu_sendto(s->fd, buf, size, 0,
                          (struct sockaddr *)&s->dgram_dst,
                          sizeof(s->dgram_dst));
    } while (ret == -1 && errno == EINTR);

    if (ret == -1 && errno == EAGAIN) {
        net_socket_write_poll(s, true);
        return 0;
    }
    return ret;
}

static void net_socket_send_completed(NetClientState *nc, ssize_t len)
{
    NetSocketState *s = DO_UPCAST(NetSocketState, nc, nc);

    if (!s->read_poll) {
        net_socket_read_poll(s, true);
    }
}

static void net_socket_rs_finalize(SocketReadState *rs)
{
    NetSocketState *s = container_of(rs, NetSocketState, rs);

    if (qemu_send_packet_async(&s->nc, rs->buf,
                               rs->packet_len,
                               net_socket_send_completed) == 0) {
        net_socket_read_poll(s, false);
    }
}

static void net_socket_send(void *opaque)
{
    NetSocketState *s = opaque;
    int size;
    int ret;
    uint8_t buf1[NET_BUFSIZE];
    const uint8_t *buf;

    size = qemu_recv(s->fd, buf1, sizeof(buf1), 0);
    if (size < 0) {
        if (errno != EWOULDBLOCK)
            goto eoc;
    } else if (size == 0) {
        /* end of connection */
    eoc:
        net_socket_read_poll(s, false);
        net_socket_write_poll(s, false);
        if (s->listen_fd != -1) {
            qemu_set_fd_handler(s->listen_fd, net_socket_accept, NULL, s);
        }
        closesocket(s->fd);

        s->fd = -1;
        net_socket_rs_init(&s->rs, net_socket_rs_finalize);
        s->nc.link_down = true;
        memset(s->nc.info_str, 0, sizeof(s->nc.info_str));

        return;
    }
    buf = buf1;

    ret = net_fill_rstate(&s->rs, buf, size);

    if (ret == -1) {
        goto eoc;
    }
}

static void net_socket_send_dgram(void *opaque)
{
    NetSocketState *s = opaque;
    int size;

    size = qemu_recv(s->fd, s->rs.buf, sizeof(s->rs.buf), 0);
    if (size < 0)
        return;
    if (size == 0) {
        /* end of connection */
        net_socket_read_poll(s, false);
        net_socket_write_poll(s, false);
        return;
    }
    if (qemu_send_packet_async(&s->nc, s->rs.buf, size,
                               net_socket_send_completed) == 0) {
        net_socket_read_poll(s, false);
    }
}

static int net_socket_mcast_create(struct sockaddr_in *mcastaddr, struct in_addr *localaddr)
{
    struct ip_mreq imr;
    int fd;
    int val, ret;
#ifdef __OpenBSD__
    unsigned char loop;
#else
    int loop;
#endif

    if (!IN_MULTICAST(ntohl(mcastaddr->sin_addr.s_addr))) {
        fprintf(stderr, "qemu: error: specified mcastaddr \"%s\" (0x%08x) "
                "does not contain a multicast address\n",
                inet_ntoa(mcastaddr->sin_addr),
                (int)ntohl(mcastaddr->sin_addr.s_addr));
        return -1;

    }
    fd = qemu_socket(PF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket(PF_INET, SOCK_DGRAM)");
        return -1;
    }

    /* Allow multiple sockets to bind the same multicast ip and port by setting
     * SO_REUSEADDR. This is the only situation where SO_REUSEADDR should be set
     * on windows. Use socket_set_fast_reuse otherwise as it sets SO_REUSEADDR
     * only on posix systems.
     */
    val = 1;
    ret = qemu_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if (ret < 0) {
        perror("setsockopt(SOL_SOCKET, SO_REUSEADDR)");
        goto fail;
    }

    ret = bind(fd, (struct sockaddr *)mcastaddr, sizeof(*mcastaddr));
    if (ret < 0) {
        perror("bind");
        goto fail;
    }

    /* Add host to multicast group */
    imr.imr_multiaddr = mcastaddr->sin_addr;
    if (localaddr) {
        imr.imr_interface = *localaddr;
    } else {
        imr.imr_interface.s_addr = htonl(INADDR_ANY);
    }

    ret = qemu_setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                          &imr, sizeof(struct ip_mreq));
    if (ret < 0) {
        perror("setsockopt(IP_ADD_MEMBERSHIP)");
        goto fail;
    }

    /* Force mcast msgs to loopback (eg. several QEMUs in same host */
    loop = 1;
    ret = qemu_setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP,
                          &loop, sizeof(loop));
    if (ret < 0) {
        perror("setsockopt(SOL_IP, IP_MULTICAST_LOOP)");
        goto fail;
    }

    /* If a bind address is given, only send packets from that address */
    if (localaddr != NULL) {
        ret = qemu_setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF,
                              localaddr, sizeof(*localaddr));
        if (ret < 0) {
            perror("setsockopt(IP_MULTICAST_IF)");
            goto fail;
        }
    }

    qemu_set_nonblock(fd);
    return fd;
fail:
    if (fd >= 0)
        closesocket(fd);
    return -1;
}

static void net_socket_cleanup(NetClientState *nc)
{
    NetSocketState *s = DO_UPCAST(NetSocketState, nc, nc);
    if (s->fd != -1) {
        net_socket_read_poll(s, false);
        net_socket_write_poll(s, false);
        close(s->fd);
        s->fd = -1;
    }
    if (s->listen_fd != -1) {
        qemu_set_fd_handler(s->listen_fd, NULL, NULL, NULL);
        closesocket(s->listen_fd);
        s->listen_fd = -1;
    }
}

static NetClientInfo net_dgram_socket_info = {
    .type = NET_CLIENT_DRIVER_SOCKET,
    .size = sizeof(NetSocketState),
    .receive = net_socket_receive_dgram,
    .cleanup = net_socket_cleanup,
};

static NetSocketState *net_socket_fd_init_dgram(NetClientState *peer,
                                                const char *model,
                                                const char *name,
                                                int fd, int is_connected)
{
    struct sockaddr_in saddr;
    int newfd;
    socklen_t saddr_len = sizeof(saddr);
    NetClientState *nc;
    NetSocketState *s;

    /* fd passed: multicast: "learn" dgram_dst address from bound address and save it
     * Because this may be "shared" socket from a "master" process, datagrams would be recv()
     * by ONLY ONE process: we must "clone" this dgram socket --jjo
     */

    if (is_connected) {
        if (getsockname(fd, (struct sockaddr *) &saddr, &saddr_len) == 0) {
            /* must be bound */
            if (saddr.sin_addr.s_addr == 0) {
                fprintf(stderr, "qemu: error: init_dgram: fd=%d unbound, "
                        "cannot setup multicast dst addr\n", fd);
                goto err;
            }
            /* clone dgram socket */
            newfd = net_socket_mcast_create(&saddr, NULL);
            if (newfd < 0) {
                /* error already reported by net_socket_mcast_create() */
                goto err;
            }
            /* clone newfd to fd, close newfd */
            dup2(newfd, fd);
            close(newfd);

        } else {
            fprintf(stderr,
                    "qemu: error: init_dgram: fd=%d failed getsockname(): %s\n",
                    fd, strerror(errno));
            goto err;
        }
    }

    nc = qemu_new_net_client(&net_dgram_socket_info, peer, model, name);

    s = DO_UPCAST(NetSocketState, nc, nc);

    s->fd = fd;
    s->listen_fd = -1;
    s->send_fn = net_socket_send_dgram;
    net_socket_rs_init(&s->rs, net_socket_rs_finalize);
    net_socket_read_poll(s, true);

    /* mcast: save bound address as dst */
    if (is_connected) {
        s->dgram_dst = saddr;
        snprintf(nc->info_str, sizeof(nc->info_str),
                 "socket: fd=%d (cloned mcast=%s:%d)",
                 fd, inet_ntoa(saddr.sin_addr), ntohs(saddr.sin_port));
    } else {
        snprintf(nc->info_str, sizeof(nc->info_str),
                 "socket: fd=%d", fd);
    }

    return s;

err:
    closesocket(fd);
    return NULL;
}

static void net_socket_connect(void *opaque)
{
    NetSocketState *s = opaque;
    s->send_fn = net_socket_send;
    net_socket_read_poll(s, true);
}

static NetClientInfo net_socket_info = {
    .type = NET_CLIENT_DRIVER_SOCKET,
    .size = sizeof(NetSocketState),
    .receive = net_socket_receive,
    .cleanup = net_socket_cleanup,
};

static NetSocketState *net_socket_fd_init_stream(NetClientState *peer,
                                                 const char *model,
                                                 const char *name,
                                                 int fd, int is_connected)
{
    NetClientState *nc;
    NetSocketState *s;

    nc = qemu_new_net_client(&net_socket_info, peer, model, name);

    snprintf(nc->info_str, sizeof(nc->info_str), "socket: fd=%d", fd);

    s = DO_UPCAST(NetSocketState, nc, nc);

    s->fd = fd;
    s->listen_fd = -1;
    net_socket_rs_init(&s->rs, net_socket_rs_finalize);

    /* Disable Nagle algorithm on TCP sockets to reduce latency */
    socket_set_nodelay(fd);

    if (is_connected) {
        net_socket_connect(s);
    } else {
        qemu_set_fd_handler(s->fd, NULL, net_socket_connect, s);
    }
    return s;
}

static NetSocketState *net_socket_fd_init(NetClientState *peer,
                                          const char *model, const char *name,
                                          int fd, int is_connected)
{
    int so_type = -1, optlen=sizeof(so_type);

    if(getsockopt(fd, SOL_SOCKET, SO_TYPE, (char *)&so_type,
        (socklen_t *)&optlen)< 0) {
        fprintf(stderr, "qemu: error: getsockopt(SO_TYPE) for fd=%d failed\n",
                fd);
        closesocket(fd);
        return NULL;
    }
    switch(so_type) {
    case SOCK_DGRAM:
        return net_socket_fd_init_dgram(peer, model, name, fd, is_connected);
    case SOCK_STREAM:
        return net_socket_fd_init_stream(peer, model, name, fd, is_connected);
    default:
        /* who knows ... this could be a eg. a pty, do warn and continue as stream */
        fprintf(stderr, "qemu: warning: socket type=%d for fd=%d is not SOCK_DGRAM or SOCK_STREAM\n", so_type, fd);
        return net_socket_fd_init_stream(peer, model, name, fd, is_connected);
    }
    return NULL;
}

static void net_socket_accept(void *opaque)
{
    NetSocketState *s = opaque;
    struct sockaddr_in saddr;
    socklen_t len;
    int fd;

    for(;;) {
        len = sizeof(saddr);
        fd = qemu_accept(s->listen_fd, (struct sockaddr *)&saddr, &len);
        if (fd < 0 && errno != EINTR) {
            return;
        } else if (fd >= 0) {
            qemu_set_fd_handler(s->listen_fd, NULL, NULL, NULL);
            break;
        }
    }

    s->fd = fd;
    s->nc.link_down = false;
    net_socket_connect(s);
    snprintf(s->nc.info_str, sizeof(s->nc.info_str),
             "socket: connection from %s:%d",
             inet_ntoa(saddr.sin_addr), ntohs(saddr.sin_port));
}

static int net_socket_listen_init(NetClientState *peer,
                                  const char *model,
                                  const char *name,
                                  const char *host_str)
{
    NetClientState *nc;
    NetSocketState *s;
    SocketAddress *saddr;
    int ret;
    Error *local_error = NULL;

    saddr = socket_parse(host_str, &local_error);
    if (saddr == NULL) {
        error_report_err(local_error);
        return -1;
    }

    ret = socket_listen(saddr, &local_error);
    if (ret < 0) {
        qapi_free_SocketAddress(saddr);
        error_report_err(local_error);
        return -1;
    }

    nc = qemu_new_net_client(&net_socket_info, peer, model, name);
    s = DO_UPCAST(NetSocketState, nc, nc);
    s->fd = -1;
    s->listen_fd = ret;
    s->nc.link_down = true;
    net_socket_rs_init(&s->rs, net_socket_rs_finalize);

    qemu_set_fd_handler(s->listen_fd, net_socket_accept, NULL, s);
    qapi_free_SocketAddress(saddr);
    return 0;
}

typedef struct {
    NetClientState *peer;
    SocketAddress *saddr;
    char *model;
    char *name;
} socket_connect_data;

static void socket_connect_data_free(socket_connect_data *c)
{
    qapi_free_SocketAddress(c->saddr);
    g_free(c->model);
    g_free(c->name);
    g_free(c);
}

static void net_socket_connected(int fd, Error *err, void *opaque)
{
    socket_connect_data *c = opaque;
    NetSocketState *s;
    char *addr_str = NULL;
    Error *local_error = NULL;

    addr_str = socket_address_to_string(c->saddr, &local_error);
    if (addr_str == NULL) {
        error_report_err(local_error);
        closesocket(fd);
        goto end;
    }

    s = net_socket_fd_init(c->peer, c->model, c->name, fd, true);
    if (!s) {
        closesocket(fd);
        goto end;
    }

    snprintf(s->nc.info_str, sizeof(s->nc.info_str),
             "socket: connect to %s", addr_str);

end:
    g_free(addr_str);
    socket_connect_data_free(c);
}

static int net_socket_connect_init(NetClientState *peer,
                                   const char *model,
                                   const char *name,
                                   const char *host_str)
{
    socket_connect_data *c = g_new0(socket_connect_data, 1);
    int fd = -1;
    Error *local_error = NULL;

    c->peer = peer;
    c->model = g_strdup(model);
    c->name = g_strdup(name);
    c->saddr = socket_parse(host_str, &local_error);
    if (c->saddr == NULL) {
        goto err;
    }

    fd = socket_connect(c->saddr, &local_error, net_socket_connected, c);
    if (fd < 0) {
        goto err;
    }

    return 0;

err:
    error_report_err(local_error);
    socket_connect_data_free(c);
    return -1;
}

static int net_socket_mcast_init(NetClientState *peer,
                                 const char *model,
                                 const char *name,
                                 const char *host_str,
                                 const char *localaddr_str)
{
    NetSocketState *s;
    int fd;
    struct sockaddr_in saddr;
    struct in_addr localaddr, *param_localaddr;

    if (parse_host_port(&saddr, host_str) < 0)
        return -1;

    if (localaddr_str != NULL) {
        if (inet_aton(localaddr_str, &localaddr) == 0)
            return -1;
        param_localaddr = &localaddr;
    } else {
        param_localaddr = NULL;
    }

    fd = net_socket_mcast_create(&saddr, param_localaddr);
    if (fd < 0)
        return -1;

    s = net_socket_fd_init(peer, model, name, fd, 0);
    if (!s)
        return -1;

    s->dgram_dst = saddr;

    snprintf(s->nc.info_str, sizeof(s->nc.info_str),
             "socket: mcast=%s:%d",
             inet_ntoa(saddr.sin_addr), ntohs(saddr.sin_port));
    return 0;

}

static int net_socket_udp_init(NetClientState *peer,
                                 const char *model,
                                 const char *name,
                                 const char *rhost,
                                 const char *lhost)
{
    NetSocketState *s;
    int fd, ret;
    struct sockaddr_in laddr, raddr;

    if (parse_host_port(&laddr, lhost) < 0) {
        return -1;
    }

    if (parse_host_port(&raddr, rhost) < 0) {
        return -1;
    }

    fd = qemu_socket(PF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket(PF_INET, SOCK_DGRAM)");
        return -1;
    }

    ret = socket_set_fast_reuse(fd);
    if (ret < 0) {
        closesocket(fd);
        return -1;
    }
    ret = bind(fd, (struct sockaddr *)&laddr, sizeof(laddr));
    if (ret < 0) {
        perror("bind");
        closesocket(fd);
        return -1;
    }
    qemu_set_nonblock(fd);

    s = net_socket_fd_init(peer, model, name, fd, 0);
    if (!s) {
        return -1;
    }

    s->dgram_dst = raddr;

    snprintf(s->nc.info_str, sizeof(s->nc.info_str),
             "socket: udp=%s:%d",
             inet_ntoa(raddr.sin_addr), ntohs(raddr.sin_port));
    return 0;
}

int net_init_socket(const Netdev *netdev, const char *name,
                    NetClientState *peer, Error **errp)
{
    /* FIXME error_setg(errp, ...) on failure */
    Error *err = NULL;
    const NetdevSocketOptions *sock;

    assert(netdev->type == NET_CLIENT_DRIVER_SOCKET);
    sock = &netdev->u.socket;

    if (sock->has_fd + sock->has_listen + sock->has_connect + sock->has_mcast +
        sock->has_udp != 1) {
        error_report("exactly one of fd=, listen=, connect=, mcast= or udp="
                     " is required");
        return -1;
    }

    if (sock->has_localaddr && !sock->has_mcast && !sock->has_udp) {
        error_report("localaddr= is only valid with mcast= or udp=");
        return -1;
    }

    if (sock->has_fd) {
        int fd;

        fd = monitor_fd_param(cur_mon, sock->fd, &err);
        if (fd == -1) {
            error_report_err(err);
            return -1;
        }
        qemu_set_nonblock(fd);
        if (!net_socket_fd_init(peer, "socket", name, fd, 1)) {
            return -1;
        }
        return 0;
    }

    if (sock->has_listen) {
        if (net_socket_listen_init(peer, "socket", name, sock->listen) == -1) {
            return -1;
        }
        return 0;
    }

    if (sock->has_connect) {
        if (net_socket_connect_init(peer, "socket", name, sock->connect) ==
            -1) {
            return -1;
        }
        return 0;
    }

    if (sock->has_mcast) {
        /* if sock->localaddr is missing, it has been initialized to "all bits
         * zero" */
        if (net_socket_mcast_init(peer, "socket", name, sock->mcast,
            sock->localaddr) == -1) {
            return -1;
        }
        return 0;
    }

    assert(sock->has_udp);
    if (!sock->has_localaddr) {
        error_report("localaddr= is mandatory with udp=");
        return -1;
    }
    if (net_socket_udp_init(peer, "socket", name, sock->udp, sock->localaddr) ==
        -1) {
        return -1;
    }
    return 0;
}
