/*
 * net.h
 *
 *  Created on: 2017年10月25日
 *      Author: linzer
 */

#ifndef __QNODE_NET_H__
#define __QNODE_NET_H__

#include <stdint.h>

uint64_t hton64(uint64_t host64);
uint32_t hton32(uint32_t host32);
uint16_t hton16(uint16_t host16);
uint64_t ntoh64(uint64_t net64);
uint32_t ntoh32(uint32_t net32);
uint16_t ntoh16(uint16_t net16);
void noblocking(int fd);
void closeonexe(int fd);

#include <net_address.h>
#include <net_socket.h>
#include <net_timer.h>
#include <net_channel.h>
#include <net_connection.h>
#include <net_connector.h>
#include <net_eventloop.h>
#include <net_poller.h>
#include <net_client.h>
#include <net_acceptor.h>
#endif /* __QNODE_INCLUDE_NET_H__ */
