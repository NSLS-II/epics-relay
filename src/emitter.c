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
#include <string.h>
#include <libnet.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "debug.h"
#include "ethernet.h"
#include "emitter.h"

int debug_flag = -1;
uint8_t hw_bcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

int send_udp_packet(unsigned char *packet, int packet_len) {
  int rtn = -1;
  libnet_t *lnet;
  struct libnet_ether_addr* hw_addr = NULL;
  char errbuf[LIBNET_ERRBUF_SIZE];

  // Get some info about the packet

  struct ether_header *eptr = (struct ether_header *) packet;
  struct ipbdy *iptr = (struct ipbdy *) (packet + ether_header_size(packet));
  struct udphdr *uptr = (struct udphdr *)
    (packet + ether_header_size(packet) + sizeof(struct ipbdy));

  int payload_len = ether_header_size(packet);
  payload_len += sizeof(struct ipbdy) + sizeof(struct udphdr);
  unsigned char *payload = packet + payload_len;
  payload_len = packet_len - payload_len;

  DEBUG_PRINT("Payload len = %d\n", payload_len);

  DEBUG_PRINT("Source MAC : %s\n", int_to_mac(eptr->ether_shost));
  DEBUG_PRINT("Source IP : %s\n", inet_ntoa(iptr->ip_sip));

  // Initialize libnet

  lnet = libnet_init(LIBNET_LINK, "ens32", errbuf);
  if (lnet == NULL) {
    ERROR_PRINT("Error with libnet_init(): %s", errbuf);
    goto _error;
  }

  // Get hardware (MAC) address

  if ((hw_addr = libnet_get_hwaddr(lnet)) == NULL) {
    ERROR_COMMENT("Unable to read HW address.\n");
    goto _error;
  }

  struct in_addr bcast;
  inet_aton("10.69.2.255", &bcast);
  struct in_addr src_test;
  inet_aton("10.69.3.38", &src_test);

  libnet_ptag_t udp_t = 0;  // UDP protocol tag
  libnet_ptag_t ipv4_t = 0;  // IP protocol tag
  libnet_ptag_t eth_t = 0;  // Ethernet protocol tag

  /* build the ethernet header */
  udp_t = libnet_build_udp(
    htons(uptr->sport),                          // src port
    htons(uptr->dport),                          // dst port
    LIBNET_UDP_H + payload_len,                  // Total packet length
    0,                                           // checksum (autofill)
    payload,                                     // payload
    payload_len,                                 // length of payload
    lnet, udp_t);

  if (udp_t == -1) {
    ERROR_COMMENT("Unable to create UDP packet\n");
    goto _error;
  }

  ipv4_t = libnet_build_ipv4(
    LIBNET_IPV4_H + LIBNET_UDP_H + payload_len,   // Total packet length
    iptr->tos,                                    // Type of service
    libnet_get_prand(LIBNET_PRu16),               // Packet id
    0x4000,                                       // Frag (don't frag)
    iptr->ttl,                                    // Time to live
    IPPROTO_UDP,                                  // Protocol
    0,                                            // Checksum (autofill)
    iptr->ip_sip.s_addr,                          // Source IP
    bcast.s_addr,                                 // Dest IP
    NULL, 0,                                      // Payload
    lnet, ipv4_t);

  if (ipv4_t == -1) {
    ERROR_COMMENT("Unable to create IPV4 packet\n");
    goto _error;
  }

  eth_t = libnet_build_ethernet(
    hw_bcast,                                     // Dest hw address
    (uint8_t*)hw_addr,                            // Interface HW Address
    ETHERTYPE_IP,                                 // Type
    NULL,                                         // Payload
    0,                                            // Payload size
    lnet, eth_t);

  if (eth_t == -1) {
    ERROR_COMMENT("Unable to create ETH packet\n");
    goto _error;
  }

  // Write the packet and send on the wire

  if ((libnet_write(lnet)) == -1) {
    ERROR_COMMENT("Unable to write packet.");
    goto _error;
  }

  rtn = 0;
_error:
  libnet_destroy(lnet);
  return rtn;
}

int main(void) {
  int socket_desc;
  struct sockaddr_in server_addr, client_addr;
  unsigned char client_message[2000];
  unsigned int client_struct_length = sizeof(client_addr);

  // Create UDP socket:
  socket_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (socket_desc < 0) {
      printf("Error while creating socket\n");
      return -1;
  }
  printf("Socket created successfully\n");

  // Set port and IP:
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(9999);
  server_addr.sin_addr.s_addr = inet_addr("10.69.0.38");

  // Bind to the set port and IP:
  if (bind(socket_desc, (struct sockaddr*)&server_addr,
           sizeof(server_addr)) < 0) {
      printf("Couldn't bind to the port\n");
      return -1;
  }
  printf("Done with binding\n");

  show_interface_broadaddr(socket_desc, "ens192");

  printf("Listening for incoming messages...\n\n");

  for (;;) {
    // Receive client's message:
    int rc;
    if ((rc = recvfrom(socket_desc, client_message, sizeof(client_message), 0,
        (struct sockaddr*)&client_addr, &client_struct_length)) < 0) {
        printf("Couldn't receive\n");
        return -1;
    }
    printf("Received message from IP: %s and port: %i\n",
          inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    send_udp_packet(client_message, rc);
  }
}
