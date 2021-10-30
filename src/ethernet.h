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
#ifndef SRC_ETHERNET_H_
#define SRC_ETHERNET_H_

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <netinet/in.h>
#include <net/ethernet.h>

#define ETHERTYPE_8021Q       0x8100

struct ethernet_header {
  uint8_t ether_dhost[ETH_ALEN];
  uint8_t ether_shost[ETH_ALEN];
  uint16_t ether_type;
} __attribute__((__packed__));

struct ethernet_header_8021q {
  uint8_t ether_dhost[ETH_ALEN];
  uint8_t ether_shost[ETH_ALEN];
  uint16_t tpid;
  uint16_t tci;
  uint16_t ether_type;
} __attribute__((__packed__));

struct ipbdy {
  uint8_t  ver_ihl;          // Version (4 bits) + header (4 bits)
  uint8_t  tos;              // Type of service
  uint16_t tlen;             // Total length
  uint16_t identification;   // Identification
  uint16_t flags_fo;         // Flags (3 bits) + Fragment offset (13 bits)
  uint8_t  ttl;              // Time to live
  uint8_t  proto;            // Protocol
  uint16_t crc;              // Header checksum
  struct in_addr ip_sip;     // Source address
  struct in_addr ip_dip;     // Destination address
  // uint32_t op_pad;           // Option + Padding
} __attribute__((__packed__));

struct udphdr {
  uint16_t sport;
  uint16_t dport;
  uint16_t len;
  uint16_t checksum;
} __attribute__((__packed__));

struct ifdatav4 {
  struct in_addr address;
  struct in_addr netmask;
  struct in_addr broadcast;
};

int ether_header_size(const u_char *packet);
const char * int_to_mac(unsigned char *addr);
int intmax(int x, int y);
int bind_socket(struct in_addr ip, uint16_t port, int bcast, int* fd);
int get_interface(const char *device, struct ifdatav4 *interface);
int is_native_packet(struct in_addr *ip, struct ifdatav4 *iface);

#endif  // SRC_ETHERNET_H_
