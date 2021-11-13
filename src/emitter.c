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

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <libnet.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "debug.h"
#include "ethernet.h"
#include "emitter.h"
#include "proto.h"
#include "config.h"
#include "defs.h"

int debug_flag = 0;

static struct option long_options[] = {
  {"debug", no_argument, &debug_flag, -1},
  {"config", required_argument, 0, 'c'},
  {0, 0, 0, 0}
};

uint8_t hw_bcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

int check_udp_packet(struct ifdatav4 *iface,
                     const unsigned char* buffer, ssize_t len) {
  // Check packet length
  if (len <= (ssize_t)sizeof(struct proto_udp_header)) {
    ERROR_PRINT("Invalid packet length %d\n", len);
    return -1;
  }

  struct proto_udp_header *header = (struct proto_udp_header*)buffer;

  // Check packet is NOT from subnet

  struct in_addr ip;
  ip.s_addr = header->src_ip;
  if (is_native_packet(&ip, iface)) {
    return -1;
  }

  return 0;
}

void close_libnet(struct libnet_params *params) {
  libnet_destroy(params->lnet);
}

int setup_libnet(struct libnet_params *params, const char *iface) {
#ifdef LIBNET_MODE_LINK
  params->lnet = libnet_init(LIBNET_LINK, iface, params->errbuf);
#else
  params->lnet = libnet_init(LIBNET_RAW4, iface, params->errbuf);
#endif
  if (params->lnet == NULL) {
    ERROR_PRINT("Error with libnet_init(): %s", params->errbuf);
    return -1;
  }

  if ((params->hw_addr = libnet_get_hwaddr(params->lnet)) == NULL) {
    ERROR_COMMENT("Unable to read HW address.\n");
    return -1;
  }

  DEBUG_PRINT("%s hardware address : %s\n", iface,
              int_to_mac(params->hw_addr->ether_addr_octet));

  return 0;
}

int send_udp_packet(struct libnet_params *params,
                    unsigned char *packet, ssize_t packet_len) {
  // Check packet length
  if (packet_len <= (ssize_t)sizeof(struct proto_udp_header)) {
    ERROR_PRINT("Invalid packet length %d\n", packet_len);
    return -1;
  }

  struct proto_udp_header *header = (struct proto_udp_header*)packet;

  DEBUG_PRINT("Payload len = %d\n", header->payload_len);
  struct in_addr ip;
  ip.s_addr = header->src_ip;
  DEBUG_PRINT("Source IP : %s\n", inet_ntoa(ip));

  /* build the ethernet header */
  params->udp_t = libnet_build_udp(
    htons(header->src_port),                     // src port
    htons(header->dst_port),                     // dst port
    LIBNET_UDP_H + header->payload_len,          // Total packet length
    0,                                           // checksum (autofill)
    packet + sizeof(struct proto_udp_header),    // payload
    header->payload_len,                         // length of payload
    params->lnet, params->udp_t);

  if (params->udp_t == -1) {
    ERROR_COMMENT("Unable to create UDP packet\n");
    return -1;
  }

  params->ipv4_t = libnet_build_ipv4(
    LIBNET_IPV4_H + LIBNET_UDP_H + header->payload_len,   // Total packet length
    0,                                            // Type of service
    libnet_get_prand(LIBNET_PRu16),               // Packet id
    0x4000,                                       // Frag (don't frag)
    64,                                           // Time to live
    IPPROTO_UDP,                                  // Protocol
    0,                                            // Checksum (autofill)
    header->src_ip,                               // Source IP
    params->bcast.s_addr,                         // Dest IP
    NULL, 0,                                      // Payload
    params->lnet, params->ipv4_t);

  if (params->ipv4_t == -1) {
    ERROR_COMMENT("Unable to create IPV4 packet\n");
    return -1;
  }

#ifdef LIBNET_MODE_LINK
  params->eth_t = libnet_build_ethernet(
    hw_bcast,                                     // Dest hw address
    (uint8_t*)params->hw_addr,                    // Interface HW Address
    ETHERTYPE_IP,                                 // Type
    NULL,                                         // Payload
    0,                                            // Payload size
    params->lnet, params->eth_t);

  if (params->eth_t == -1) {
    ERROR_COMMENT("Unable to create ETH packet\n");
    return -1;
  }
#endif

  // Write the packet and send on the wire

  if ((libnet_write(params->lnet)) == -1) {
    ERROR_COMMENT("Unable to write packet.");
    return -1;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  char *config_file = DEFAULT_CONFIG_FILE;
  emitter_params params;

  // Parse command line options
  while (1) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "dc:",
                        long_options, &option_index);
    if (c == -1) {
      break;
    }

    switch (c) {
    case 0:
      break;
    case 'c':
      config_file = optarg;
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

  if (config_read_emitter(config_file, &params)) {
    ERROR_COMMENT("Unable to read config file\n");
    exit(-1);
  }

  if (bind_socket(params.iface.address, PROTO_UDP_PORT, 0, &params.fd)) {
    ERROR_COMMENT("Unable to bind to socket\n");
    exit(-1);
  }

  // Setup libnet
  if (setup_libnet(&params.libnet, params.iface_epics_name)) {
    ERROR_COMMENT("Unable to setup packet emitter\n");
    exit(-1);
  }

  params.libnet.bcast = params.iface_epics.broadcast;

  struct sockaddr_in client_addr;
  unsigned int client_addr_len = sizeof(client_addr);
  unsigned char buffer[2000];

  for (;;) {
    // Receive client's message:
    ssize_t rc;
    if ((rc = recvfrom(params.fd, buffer, sizeof(buffer), 0,
        (struct sockaddr*)&client_addr, &client_addr_len)) < 0) {
        ERROR_COMMENT("Could not receive\n");
    } else {
      char name[INET_ADDRSTRLEN];
      if (inet_ntop(AF_INET, &(client_addr.sin_addr), name, sizeof(name))) {
        DEBUG_PRINT("Received message from IP: %s and port: %i\n", name,
                    ntohs(client_addr.sin_port));
      }
      if (check_udp_packet(&params.iface_epics, buffer, rc)) {
        ERROR_COMMENT("Packet check failed ... skipping ...\n");
        continue;
      }

      send_udp_packet(&params.libnet, buffer, rc);
    }
  }

  close_libnet(&params.libnet);
  close(params.fd);
}
