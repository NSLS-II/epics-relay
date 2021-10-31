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
#include <getopt.h>
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

int debug_flag = 0;

#include "ethernet.h"
#include "debug.h"
#include "proto.h"
#include "epics.h"

#define MAX_FD        50

typedef struct {
  int fd;
  int fd_listen[MAX_FD];
  int listen_ports[MAX_FD];
  int fd_listen_max;
  struct ifdatav4 iface;
  struct ifdatav4 iface_listen;
  struct in_addr emitter;
} collector_params;

static struct option long_options[] = {
  {"debug", no_argument, &debug_flag, -1},
  {"iface", required_argument, 0, 'i'},
  {0, 0, 0, 0}
};

int setup_sockets(collector_params *params) {
  for (int i = 0; i < params->fd_listen_max; i++) {
    DEBUG_PRINT("Setting up port %d\n", params->listen_ports[i]);
    if (bind_socket(params->iface_listen.broadcast,
                    params->listen_ports[i],
                    1, &params->fd_listen[i])) {
      return -1;
    }
  }
  return 0;
}

void listen_start(collector_params *params) {
  fd_set socks;
  struct timeval timeout;
  timeout.tv_sec = 1; timeout.tv_usec = 0;

  // Emitter setup
  struct sockaddr_in emitter_addr;
  memset(&emitter_addr, 0, sizeof(emitter_addr));
  emitter_addr.sin_family = AF_INET;
  emitter_addr.sin_port = htons(PROTO_UDP_PORT);
  emitter_addr.sin_addr = params->emitter;

  FD_ZERO(&socks);

  // Allocate data buffer
  char data[2048];

  // Set header struct
  struct proto_udp_header *header = (struct proto_udp_header*)data;
  memset(header, 0, sizeof(struct proto_udp_header));
  header->version = PROTO_VERSION;

  // Find max fd
  int maxfd = intmax(params->fd_listen, params->fd_listen_max);

  // Loop forever!
  while (1) {
    for (int i = 0; i < params->fd_listen_max; i++) {
      FD_SET(params->fd_listen[i], &socks);
    }

    // Setup select for multiple descriptors
    select(maxfd, &socks, NULL, NULL, &timeout);

    struct sockaddr_in si;
    unsigned slen = sizeof(struct sockaddr);

    // Cycle through fd
    for (int i = 0; i < params->fd_listen_max; i++) {
      if (FD_ISSET(params->fd_listen[i], &socks)) {
        int len = recvfrom(params->fd_listen[i],
                           (data + sizeof(struct proto_udp_header)),
                           sizeof(data) - sizeof(struct proto_udp_header) - 1,
                           0, (struct sockaddr *)&si, &slen);

        DEBUG_PRINT("Recieve %d: %s:%d %d bytes\n", i,
                    inet_ntoa(si.sin_addr), ntohs(si.sin_port), len);

        if (len != 0) {
          if (!is_native_packet(&(si.sin_addr), &(params->iface_listen))) {
            DEBUG_COMMENT("Non native packet ... skipping ...\n");
            continue;
          }

          // Fill in the header with packet data
          header->payload_len = len;
          header->src_ip = si.sin_addr.s_addr;
          header->src_port = si.sin_port;
          header->dst_port = htons(params->listen_ports[i]);
          header->dst_ip = params->iface_listen.broadcast.s_addr;

          // Now read EPICS data

          epics_read_packet(data + sizeof(struct proto_udp_header), len);

          // Now transmit header
          int n = sendto(params->fd,
                        data, sizeof(struct proto_udp_header) + len,
                        0, (struct sockaddr *)(&emitter_addr),
                        sizeof(emitter_addr));

          if (n < 0) {
            ERROR_COMMENT("Unable to send....\n");
            continue;
          }

#ifdef DEBUG
          char name[INET_ADDRSTRLEN];
          if (inet_ntop(AF_INET, &(emitter_addr.sin_addr),
                        name, sizeof(name))) {
            DEBUG_PRINT("Sent %d bytes to %s:%d\n", n,
                        name, ntohs(emitter_addr.sin_port));
          }
#endif
          break;
        }
      }
    }
  }
}

int start_collector(collector_params *params) {
  // Setup ports to listen to
  params->listen_ports[0] = 5064;
  params->listen_ports[1] = 5065;
  params->listen_ports[2] = 5076;
  params->fd_listen_max = 3;

  if (bind_socket(params->iface.address, PROTO_UDP_PORT, 0, &params->fd)) {
    ERROR_COMMENT("Unable to bind....\n");
    return -1;
  }

  setup_sockets(params);
  listen_start(params);

  return 0;
}

int main(int argc, char *argv[]) {
  collector_params params;
  char *iface = NULL;
  char *iface_listen = NULL;
  char *emitter_ip = NULL;

  // Parse command line options
  while (1) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "di:",
                        long_options, &option_index);
    if (c == -1) {
      break;
    }

    switch (c) {
    case 0:
      /* If this option set a flag, do nothing else now. */
      if (long_options[option_index].flag != 0)
        break;
      printf("option %s", long_options[option_index].name);
      if (optarg) {
        printf(" with arg %s", optarg);
      }
      printf("\n");
      break;
    case 'i':
      iface = optarg;
      break;
    case 'd':
      debug_flag = -1;
      break;
    case '?':
    default:
      exit(-1);
      break;
    }
  }

  int extra = argc - optind;
  DEBUG_PRINT("Extra opts : %d\n", extra);

  if (extra != 2) {
    ERROR_COMMENT("Incorrect options on command line\n");
    exit(-1);
  }

  iface_listen = argv[optind++];
  emitter_ip = argv[optind++];

  if (get_interface(iface, &(params.iface))) {
    ERROR_COMMENT("Unable to get iface data\n");
    return -1;
  }
  if (get_interface(iface_listen, &(params.iface_listen))) {
    ERROR_COMMENT("Unable to get iface data\n");
    return -1;
  }

  if (inet_pton(AF_INET, emitter_ip, &params.emitter) != 1) {
    ERROR_COMMENT("Unable to convert emitter address\n");
    return -1;
  }

  start_collector(&params);

  // TODO(swilkins) close sockets
}
