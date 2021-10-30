//
//  epics-relay
//
//  Stuart B. Wilkins, Brookhaven National Laboratory
//
//
//  BSD 3-Clause License
//
//  Copyright (c) 2021, Brookhaven Science Associates
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//  3. Neither the name of the copyright holder nor the names of its
//     contributors may be used to endorse or promote products derived from
//     this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
//  THE POSSIBILITY OF SUCH DAMAGE.
//

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/ip.h>
#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <libnet.h>

int debug_flag = -1;

#include "ethernet.h"
#include "debug.h"
#include "proto.h"

#define PORT    5064
#define MAXLINE 4096
#define EPICSRELAY_CONFIG_MAX_STRING    2048

unsigned char mac_zeros[] = {0, 0, 0, 0, 0, 0};
unsigned char mac_bcast[] = {255, 255, 255, 255, 255, 255};

typedef struct {
  int fd;
  int fd_ns;
  int fd_beacon;
  struct ifdatav4 iface;
  struct ifdatav4 iface_listen;
} collector_params;

int setup_sockets(collector_params *params) {
  if (bind_socket(params->iface_listen.broadcast, 5064,
                  1, &params->fd_ns)) {
    return -1;
  }
  if (bind_socket(params->iface_listen.broadcast, 5065,
                  1, &params->fd_beacon)) {
    return -1;
  }
  return 0;
}

void listen_start(collector_params *params) {
  fd_set socks;
  struct timeval timeout;
  timeout.tv_sec = 1; timeout.tv_usec = 0;

  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(9999);
  inet_aton("10.69.0.38", &servaddr.sin_addr);

  FD_ZERO(&socks);

  int maxfd = intmax(params->fd_ns, params->fd_beacon) + 1;
  while (1) {
    FD_SET(params->fd_ns, &socks);
    FD_SET(params->fd_beacon, &socks);

    select(maxfd, &socks, NULL, NULL, &timeout);

    int len = 0;
    char data[10000];
    struct sockaddr_in si;
    unsigned slen = sizeof(struct sockaddr);

    if (FD_ISSET(params->fd_ns, &socks)) {
      len = recvfrom(params->fd_ns,
                     (data + sizeof(struct proto_udp_header)),
                     sizeof(data) - sizeof(struct proto_udp_header) - 1,
                     0, (struct sockaddr *)&si, &slen);

      DEBUG_PRINT("Recieve name: %s:%d\n",
                  inet_ntoa(si.sin_addr), ntohs(si.sin_port));
    }

    if (FD_ISSET(params->fd_beacon, &socks)) {
      len = recvfrom(params->fd_beacon,
                     (data + sizeof(struct proto_udp_header)),
                     sizeof(data) - sizeof(struct proto_udp_header) - 1,
                     0, (struct sockaddr *)&si, &slen);
      DEBUG_PRINT("Recieve beacon: %s:%d\n",
                  inet_ntoa(si.sin_addr), ntohs(si.sin_port));
    }

    // Check if packet is from local interface

    if (len != 0) {
      DEBUG_PRINT("len = %d\n", len);

      if (!is_native_packet(&(si.sin_addr), &(params->iface_listen))) {
        DEBUG_COMMENT("Non native packet ... skipping ...\n");
        continue;
      }

      struct proto_udp_header *header = (struct proto_udp_header*)data;
      memset(header, 0, sizeof(struct proto_udp_header));
      header->version = PROTO_VERSION;
      header->payload_len = len;
      header->src_ip = si.sin_addr.s_addr;
      header->src_port = si.sin_port;
      // TODO(swilkins) add dest addr from bind
      // header->dst_port = htons(5064);

      // Now transmit header
      int n = sendto(params->fd,
                     data, sizeof(struct proto_udp_header) + len,
                     0, (struct sockaddr *)(&servaddr),
                     sizeof(servaddr));

      if (n < 0) {
        ERROR_COMMENT("Unable to send....\n");
        continue;
      }

      char name[INET_ADDRSTRLEN];
      if (inet_ntop(AF_INET, &(servaddr.sin_addr),
                    name, sizeof(name))) {
        DEBUG_PRINT("Sent %d bytes to %s:%d\n", n,
                    name, ntohs(servaddr.sin_port));
      }
    }
  }
}

int main() {
  collector_params params;

  if (get_interface("ens192", &(params.iface))) {
    ERROR_COMMENT("Unable to get iface data\n");
    return -1;
  }
  if (get_interface("ens224", &(params.iface_listen))) {
    ERROR_COMMENT("Unable to get iface data\n");
    return -1;
  }

  if (bind_socket(params.iface.address, 9999, 0, &params.fd)) {
    ERROR_COMMENT("Unable to bind....\n");
    return -1;
  }

  setup_sockets(&params);
  listen_start(&params);

  close(params.fd);
  close(params.fd_beacon);
  close(params.fd_ns);
  return 0;
}
