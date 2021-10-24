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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/ip.h>
#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <pcap.h>
#include <libnet.h>

#include "debug.h"
#include "ethernet.h"

char hexchars[] = { '0', '1', '2', '3', '4', '5',
                    '6', '7', '8', '9', 'A', 'B',
                    'C', 'D', 'E', 'F' };

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

void show_interface_broadaddr(int fd, const char *device) {
  struct ifreq ifreq;
  char host[128];
  memset(&ifreq, 0, sizeof ifreq);
  strncpy(ifreq.ifr_name, device, IFNAMSIZ);

  if (ioctl(fd, SIOCGIFBRDADDR, &ifreq) != 0) {
    fprintf(stderr, "Could not find interface named %s", device);
    return;
  }

  getnameinfo(&ifreq.ifr_broadaddr, sizeof(ifreq.ifr_broadaddr),
              host, sizeof(host), 0, 0, NI_NUMERICHOST);
  DEBUG_PRINT("Broadcast Address for %s is %s\n", device, host);
}
