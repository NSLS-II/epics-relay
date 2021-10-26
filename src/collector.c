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

#include <pcap.h>
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

#define PORT    5064
#define MAXLINE 4096
#define EPICSRELAY_CONFIG_MAX_STRING    2048
#define PCAP_PROGRAM "(ether broadcast) && (udp port 5064 || udp port 5065)"
#define IP_PROTO_UDP                0x11
#ifndef ETHERTYPE_8021Q
#define ETHERTYPE_8021Q       0x8100
#endif

unsigned char mac_zeros[] = {0, 0, 0, 0, 0, 0};
unsigned char mac_bcast[] = {255, 255, 255, 255, 255, 255};
pcap_t *pcap_description = NULL;


typedef struct {
  int sockfd;
  int fd_ns;
  int fd_beacon;
  struct sockaddr_in servaddr;
  int pcap_timeout;
  char program[EPICSRELAY_CONFIG_MAX_STRING];
  char device[EPICSRELAY_CONFIG_MAX_STRING];
  unsigned char hwaddress[ETH_ALEN];
} epicsrelay_params;

int max(int x, int y) {
  if (x > y)
    return x;
  else
    return y;
}

int bind_socket(const char* ip, uint16_t port, int* fd) {
  int broadcast = 1;

  struct sockaddr_in si;
  *fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (*fd == -1) {
    return -1;
  }

  setsockopt(*fd, SOL_SOCKET, SO_BROADCAST,
             &broadcast, sizeof(broadcast));

  memset(&si, 0, sizeof(si));
  si.sin_family = AF_INET;
  si.sin_port = htons(port);
  si.sin_addr.s_addr = inet_addr(ip);

  if (bind(*fd, (struct sockaddr *)&si,
           sizeof(struct sockaddr)) == -1) {
    return -1;
  }

  return 0;
}

int setup_sockets(epicsrelay_params *params) {
  if (bind_socket("10.69.3.255", 5064, &params->fd_ns)) {
    return -1;
  }
  if (bind_socket("10.69.3.255", 5065, &params->fd_beacon)) {
    return -1;
  }
  return 0;
}

void listen_start(epicsrelay_params *params) {
  fd_set socks;
  struct timeval timeout;
  timeout.tv_sec = 1; timeout.tv_usec = 0;

  FD_ZERO(&socks);

  int maxfd = max(params->fd_ns, params->fd_beacon) + 1;
  while (1) {
    FD_SET(params->fd_ns, &socks);
    FD_SET(params->fd_beacon, &socks);

    select(maxfd, &socks, NULL, NULL, &timeout);

    if (FD_ISSET(params->fd_ns, &socks)) {
      char buf[10000];
      struct sockaddr_in si;
      unsigned slen = sizeof(struct sockaddr);
      recvfrom(params->fd_ns, buf, sizeof(buf)-1, 0,
              (struct sockaddr *)&si, &slen);

      DEBUG_PRINT("Recieve name: %s:%d\n",
                  inet_ntoa(si.sin_addr), ntohs(si.sin_port));
    }

    if (FD_ISSET(params->fd_beacon, &socks)) {
      char buf[10000];
      struct sockaddr_in si;
      unsigned slen = sizeof(struct sockaddr);
      recvfrom(params->fd_beacon, buf, sizeof(buf)-1, 0,
              (struct sockaddr *)&si, &slen);

      DEBUG_PRINT("Recieve beacon: %s:%d\n",
                  inet_ntoa(si.sin_addr), ntohs(si.sin_port));
    }
  }
}

int capture_ip_packet(epicsrelay_params *params,
                      const struct pcap_pkthdr* pkthdr,
                      const u_char* packet) {
  struct ether_header *eptr = (struct ether_header *) packet;
  struct ipbdy *iptr = (struct ipbdy *) (packet + ether_header_size(packet));

  // Process any IP Packets that are broadcast

  DEBUG_PRINT("Iface : %s Packet time : %ld Broadcast Source:  %-20s %-16s\n",
              params->device,
              pkthdr->ts.tv_sec,
              ether_ntoa((const struct ether_addr *)&eptr->ether_shost),
              inet_ntoa(iptr->ip_sip));

  if (iptr->proto == IP_PROTO_UDP) {
    // We have a UDP Packet
    struct udphdr *uptr = (struct udphdr *)(packet
                          + ether_header_size(packet)
                          + sizeof(struct ipbdy));
    DEBUG_PRINT("UDP Port : %d\n", htons(uptr->dport));
    sendto(params->sockfd, packet, pkthdr->len, 0,
           (struct sockaddr *)(&params->servaddr), sizeof(params->servaddr));
  }
  return 0;
}

void capture_callback(u_char *args, const struct pcap_pkthdr* pkthdr,
                      const u_char* packet) {
  epicsrelay_params *params = (epicsrelay_params*)args;
  DEBUG_COMMENT("Enter\n");

  struct ethernet_header *eptr = (struct ethernet_header *) packet;
  uint16_t type = ntohs(eptr->ether_type);
  if (type == ETHERTYPE_IP) {
    DEBUG_COMMENT("IP Packet\n");
    capture_ip_packet(params, pkthdr, packet);
  } else {
    ERROR_COMMENT("Invalid Packet\n");
  }
}

int capture_start(epicsrelay_params *params) {
  char errbuf[PCAP_ERRBUF_SIZE];
  struct bpf_program fp;
  bpf_u_int32 maskp;
  bpf_u_int32 netp;

  pcap_if_t *interfaces = NULL, *temp;

  int rtn = -1;

  if (pcap_findalldevs(&interfaces, errbuf)) {
    ERROR_COMMENT("pcap_findalldevs() : ERROR\n");
    goto _error;
  }

  if (!interfaces) {
    goto _error;
  }

  int found = 0;
  for (temp=interfaces; temp; temp=temp->next) {
    DEBUG_PRINT("Found interface : %s\n", temp->name);
    if (!strcmp(temp->name, params->device)) {
      found = 1;
      break;
    }
  }

  if (!found) {
    ERROR_PRINT("Interface %s is not valid.\n", params->device);
    goto _error;
  }

  // Get the mac address of the interface
  struct ifreq ifr;
  int s = socket(AF_INET, SOCK_DGRAM, 0);
  strncpy(ifr.ifr_name, params->device, IFNAMSIZ);
  ioctl(s, SIOCGIFHWADDR, &ifr);
  memcpy(params->hwaddress, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
  // DEBUG_PRINT("MAC Address of %s : %s\n",
  //             params->device,
  //             int_to_mac(params->hwaddress));
  close(s);

  // Get the IP address and netmask of the interface
  pcap_lookupnet(params->device, &netp, &maskp, errbuf);

  pcap_description = pcap_open_live(params->device, BUFSIZ, 1,
                         params->pcap_timeout, errbuf);
  if (pcap_description == NULL) {
    ERROR_PRINT("pcap_open_live(): ERROR : %s\n", errbuf);
    goto _error;
  }

  DEBUG_PRINT("Opened interface : %s\n", params->device);

  // Compile the pcap program
  if (pcap_compile(pcap_description, &fp, params->program, 0, netp) == -1) {
    ERROR_COMMENT("pcap_compile() : ERROR\n");
    goto _error;
  }

  // Filter based on compiled program
  if (pcap_setfilter(pcap_description, &fp) == -1) {
    ERROR_COMMENT("pcap_setfilter() : ERROR\n");
    goto _error;
  }

  NOTICE_PRINT("Starting capture on : %s\n", params->device);
  pcap_loop(pcap_description, -1, capture_callback,
            (u_char*)(params));

  rtn = 0;

_error:
  if (interfaces) pcap_freealldevs(interfaces);

  return rtn;
}

int capture_stop(void) {
  if (pcap_description) {
    pcap_breakloop(pcap_description);
  }

  return 0;
}

// Driver code
int main() {
  epicsrelay_params params;

  snprintf(params.device, EPICSRELAY_CONFIG_MAX_STRING, "ens32");
  params.pcap_timeout = 1;
  strncpy(params.program,
          PCAP_PROGRAM,
          EPICSRELAY_CONFIG_MAX_STRING);

  // Creating socket file descriptor
  if ( (params.sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
      perror("socket creation failed");
      exit(EXIT_FAILURE);
  }

  memset(&params.servaddr, 0, sizeof(params.servaddr));
  params.servaddr.sin_family = AF_INET;
  params.servaddr.sin_port = htons(9999);
  params.servaddr.sin_addr.s_addr = inet_addr("10.69.0.38");

  setup_sockets(&params);
  listen_start(&params);

  return 0;
}
