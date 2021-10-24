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

int debug_flag = -1;
uint8_t hw_bcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

int send_udp(char *packet, int packet_len) {
  int rtn = -1;
  libnet_t *lnet;
  struct libnet_ether_addr* hw_addr = NULL;
  char errbuf[LIBNET_ERRBUF_SIZE];

  // Get some info about the packet

  struct ether_header *eptr = (struct ether_header *) packet;
  struct ipbdy *iptr = (struct ipbdy *) (packet + ether_header_size(packet));
  struct udphdr *uptr = (struct udphdr *)
    (packet + ether_header_size(packet) + sizeof(struct ipbdy));

  int len = ether_header_size(packet);
  len += sizeof(struct ipbdy) + sizeof(struct udphdr);
  char *data = packet + len;
  len = packet_len - len;

  DEBUG_PRINT("Source MAC : %s\n", int_to_mac(eptr->ether_shost));
  DEBUG_PRINT("Source IP : %s\n", inet_ntoa(iptr->ip_sip));

  lnet = libnet_init(LIBNET_LINK, NULL, errbuf);
  if (lnet == NULL) {
    printf("Error with libnet_init(): %s", errbuf);
    return -1;
  }

  // Get hardware (MAC) address

  if ((hw_addr = libnet_get_hwaddr(lnet)) == NULL) {
    ERROR_COMMENT("Unable to read HW address.\n");
    return -1;
  }

  struct in_addr bcast;
  inet_aton("10.69.0.255", &bcast);

  libnet_ptag_t eth = 0;                 /* Ethernet protocol tag */
  /* build the ethernet header */
  eth = libnet_build_udp(htons(uptr->sport),
                         htons(uptr->dport), len, 0,
                         data, len, lnet, 0);

  eth = libnet_build_ipv4(LIBNET_IPV4_H + LIBNET_UDP_H + len,
                          0, 242,
                          0, 64, IPPROTO_UDP, 0,
                          iptr->ip_sip.s_addr,
                          bcast.s_addr, NULL, 0, lnet, 0);

  eth = libnet_build_ethernet(hw_bcast,
                              eptr->ether_shost,
                              ETHERTYPE_IP,
                              NULL,
                              0,
                              lnet,
                              0);

  /* write the packet */
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
  char server_message[2000], client_message[2000];
  int client_struct_length = sizeof(client_addr);

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
  server_addr.sin_addr.s_addr = inet_addr("10.69.0.37");

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

    send_udp(client_message, rc);
  }
}
