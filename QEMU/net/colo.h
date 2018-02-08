/*
 * COarse-grain LOck-stepping Virtual Machines for Non-stop Service (COLO)
 * (a.k.a. Fault Tolerance or Continuous Replication)
 *
 * Copyright (c) 2016 HUAWEI TECHNOLOGIES CO., LTD.
 * Copyright (c) 2016 FUJITSU LIMITED
 * Copyright (c) 2016 Intel Corporation
 *
 * Author: Zhang Chen <zhangchen.fnst@cn.fujitsu.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 */

#ifndef QEMU_COLO_PROXY_H
#define QEMU_COLO_PROXY_H

#include "slirp/slirp.h"
#include "qemu/jhash.h"
#include "qemu/timer.h"

#define HASHTABLE_MAX_SIZE 16384

#ifndef IPPROTO_DCCP
#define IPPROTO_DCCP 33
#endif

#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif

#ifndef IPPROTO_UDPLITE
#define IPPROTO_UDPLITE 136
#endif

typedef struct Packet {
    void *data;
    union {
        uint8_t *network_header;
        struct ip *ip;
    };
    uint8_t *transport_header;
    int size;
    /* Time of packet creation, in wall clock ms */
    int64_t creation_ms;
} Packet;

typedef struct ConnectionKey {
    /* (src, dst) must be grouped, in the same way than in IP header */
    struct in_addr src;
    struct in_addr dst;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t ip_proto;
} QEMU_PACKED ConnectionKey;

typedef struct Connection {
    /* connection primary send queue: element type: Packet */
    GQueue primary_list;
    /* connection secondary send queue: element type: Packet */
    GQueue secondary_list;
    /* flag to enqueue unprocessed_connections */
    bool processing;
    uint8_t ip_proto;
    /* offset = secondary_seq - primary_seq */
    tcp_seq  offset;
    /*
     * we use this flag update offset func
     * run once in independent tcp connection
     */
    int syn_flag;
} Connection;

uint32_t connection_key_hash(const void *opaque);
int connection_key_equal(const void *opaque1, const void *opaque2);
int parse_packet_early(Packet *pkt);
void fill_connection_key(Packet *pkt, ConnectionKey *key);
void reverse_connection_key(ConnectionKey *key);
Connection *connection_new(ConnectionKey *key);
void connection_destroy(void *opaque);
Connection *connection_get(GHashTable *connection_track_table,
                           ConnectionKey *key,
                           GQueue *conn_list);
void connection_hashtable_reset(GHashTable *connection_track_table);
Packet *packet_new(const void *data, int size);
void packet_destroy(void *opaque, void *user_data);

#endif /* QEMU_COLO_PROXY_H */
