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
#include <regex.h>
#include <arpa/inet.h>

#include "debug.h"
#include "ethernet.h"
#include "epics.h"

int epics_process_version(const char *packet) {
  struct ca_proto_version *version =
    (struct ca_proto_version *)packet;

  DEBUG_PRINT("Priority  : %d\n", htons(version->priority));
  DEBUG_PRINT("Version   : %d\n", htons(version->version));
  return sizeof(struct ca_proto_version);
}

int epics_process_beacon(const char *packet) {
  struct ca_proto_rsrv_is_up *beacon =
    (struct ca_proto_rsrv_is_up *)packet;

  struct sockaddr_in sa;
  char addr[INET_ADDRSTRLEN];

  sa.sin_addr.s_addr = beacon->address;
  inet_ntop(AF_INET, &(sa.sin_addr), addr, INET_ADDRSTRLEN);

  DEBUG_PRINT("Beacon on port %d index %d on %s\n",
              htons(beacon->port), htonl(beacon->beaconid), addr);
  return sizeof(struct ca_proto_rsrv_is_up);
}

int epics_process_search(const char *packet) {
  int pos = 0;
  struct ca_proto_search *req =
    (struct ca_proto_search *)packet;
  pos += sizeof(struct ca_proto_search);

  DEBUG_PRINT("Reply       : %d\n", htons(req->reply));
  DEBUG_PRINT("Version     : %d\n", htons(req->version));
  DEBUG_PRINT("Search ID 1 : %d\n", htonl(req->cid1));
  DEBUG_PRINT("Search ID 2 : %d\n", htonl(req->cid2));
  DEBUG_PRINT("PV Name     : %s\n", packet + pos);

  pos += htons(req->payload_size);
  return pos;
}

int epics_read_packet(const char* packet, int packet_len) {
  int pos = 0;

  DEBUG_PRINT("Start. Packet len : %d\n", packet_len);

  while (pos < packet_len) {
    // Process messages
    struct ca_proto_msg *msg = (struct ca_proto_msg *)
                               (packet + pos);

    DEBUG_PRINT("Command : %d\n", htons(msg->command));
    DEBUG_PRINT("Payload_size : %d\n", htons(msg->payload_size));

    if (msg->command == CA_PROTO_VERSION) {
      DEBUG_COMMENT("Valid CA_PROTO_VERSION\n");
      pos += epics_process_version(packet + pos);
    } else if (htons(msg->command) == CA_PROTO_SEARCH) {
      DEBUG_COMMENT("Valid CA_SEARCH_REQUEST\n");
      pos += epics_process_search(packet + pos);
    } else if (htons(msg->command) == CA_PROTO_RSRV_IS_UP) {
      DEBUG_COMMENT("Valid CA_PROTO_RSRV_IS_UP\n");
      pos += epics_process_beacon(packet + pos);
    } else {
      DEBUG_PRINT("Unknown command %d\n", htons(msg->command));
      break;
    }
  }

  return 0;
}
