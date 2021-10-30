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

#define _GNU_SOURCE     /* To get defns of NI_MAXSERV and NI_MAXHOST */
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/ip.h>
#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <ifaddrs.h>
#include <netdb.h>

#include "debug.h"
#include "ethernet.h"

char hexchars[] = { '0', '1', '2', '3', '4', '5',
                    '6', '7', '8', '9', 'A', 'B',
                    'C', 'D', 'E', 'F' };

int intmax(int x, int y) {
  if (x > y)
    return x;
  else
    return y;
}

int bind_socket(struct in_addr ip, uint16_t port, int bcast, int* fd) {
  int broadcast = 1;

  struct sockaddr_in si;
  *fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (*fd == -1) {
    ERROR_COMMENT("Unable to get socket descriptor\n");
    return -1;
  }

  if (bcast) {
    setsockopt(*fd, SOL_SOCKET, SO_BROADCAST,
              &broadcast, sizeof(broadcast));
  }

  memset(&si, 0, sizeof(si));
  si.sin_family = AF_INET;
  si.sin_port = htons(port);
  si.sin_addr = ip;

  if (bind(*fd, (struct sockaddr *)&si,
           sizeof(struct sockaddr)) == -1) {
    ERROR_COMMENT("Bind failed\n");
    return -1;
  }

  return 0;
}

int ether_header_size(const u_char *packet) {
  struct ethernet_header *hdr = (struct ethernet_header *)packet;
  if (ntohs(hdr->ether_type) == ETHERTYPE_8021Q) {
    // Tagged packet
    return sizeof(struct ethernet_header_8021q);
  } else {
    return sizeof(struct ethernet_header);
  }
}

const char * int_to_mac(unsigned char *addr) {
  static char _mac[30];
  int j = 0;
  for (int i=0; i < 6; i++) {
    _mac[j++] = hexchars[(addr[i] >> 4) & 0x0F];
    _mac[j++] = hexchars[addr[i] & 0x0F];
    _mac[j++] = ':';
  }

  _mac[j - 1] = '\0';

  return _mac;
}

int get_interface(const char *device, struct ifdatav4 *interface) {
  struct ifaddrs *ifaddr;

  if (getifaddrs(&ifaddr) == -1) {
    ERROR_COMMENT("ERROR: getifaddrs");
    return -1;
  }

  for (struct ifaddrs *ifa = ifaddr; ifa != NULL;
       ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL) {
      continue;
    }
    if ((strcmp(ifa->ifa_name, device) == 0) &&
        (ifa->ifa_addr->sa_family == AF_INET)) {
      // We have our interface and its IPV4 addresses

      DEBUG_PRINT("Found device %s\n", device);
      interface->address =
        ((struct sockaddr_in *)(ifa->ifa_addr))->sin_addr;
      DEBUG_PRINT("%s address   : %s\n", device,
                  inet_ntoa(interface->address));

      interface->netmask =
        ((struct sockaddr_in *)(ifa->ifa_netmask))->sin_addr;
      DEBUG_PRINT("%s netmask   : %s\n", device,
                  inet_ntoa(interface->netmask));

      interface->broadcast =
        ((struct sockaddr_in *)(ifa->ifa_broadaddr))->sin_addr;
      DEBUG_PRINT("%s broadcast : %s\n", device,
                  inet_ntoa(interface->broadcast));
    }
  }

  freeifaddrs(ifaddr);
  return 0;
}
