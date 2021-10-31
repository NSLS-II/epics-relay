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
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <arpa/inet.h>

#include "debug.h"
#include "ethernet.h"
#include "epics.h"

int round_up(int num, int factor) {
    return num + factor - 1 - (num + factor - 1) % factor;
}

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

int epics_process_search(char *dst, const char *src, int *dst_len,
                         struct epics_pv_filter *filter) {
  struct ca_proto_search *req =
    (struct ca_proto_search *)src;
  int pos = sizeof(struct ca_proto_search);

  DEBUG_PRINT("Reply       : %d\n", htons(req->reply));
  DEBUG_PRINT("Version     : %d\n", htons(req->version));
  DEBUG_PRINT("Search ID 1 : %d\n", htonl(req->cid1));
  DEBUG_PRINT("Search ID 2 : %d\n", htonl(req->cid2));

  int len = htons(req->payload_size);

  char pv[128];
  if (len >= (int)(sizeof(pv))) {
    ERROR_PRINT("Payload size of %d is too large (max = %d)\n",
                len, sizeof(pv));

    // Exit without processing request
    *dst_len = 0;
    pos += len;
    return pos;
  }

  // Copy pv to string
  strncpy(pv, src + pos, len);
  DEBUG_PRINT("PV Name     : %s\n", pv);

  pos += len;

  int match = 0;

  for (struct epics_pv_filter *f = filter;
       f != NULL; f = f->next) {
    pcre2_match_data *match_data =
      pcre2_match_data_create_from_pattern(f->re, NULL);
    int rc = pcre2_match(
      f->re,                /* the compiled pattern */
      (PCRE2_SPTR)pv,       /* the subject string */
      strlen(pv),           /* the length of the subject */
      0,                    /* start at offset 0 in the subject */
      0,                    /* default options */
      match_data,           /* block for storing the result */
      NULL);                /* use default match context */

    if (rc < 0) {
      DEBUG_COMMENT("Match failed\n");
    } else {
      DEBUG_COMMENT("Match succeeded\n");
    }

    pcre2_match_data_free(match_data);
  }

  if (match) {
    DEBUG_COMMENT("Match exclude PV\n");
    *dst_len = 0;
  } else {
    *dst_len = pos;
    memcpy(dst, src, pos);
  }
  DEBUG_PRINT("pos = %d dst_len = %d\n", pos, *dst_len);
  return pos;
}

int epics_read_packet(char* dest, const char* src, int len,
                      struct epics_pv_filter *filter) {
  int pos = 0;
  int pos_dest = 0;

  DEBUG_PRINT("Start. Packet len : %d\n", len);

  while (pos < len) {
    // Process messages
    struct ca_proto_msg *msg = (struct ca_proto_msg *)
                               (src + pos);

    DEBUG_PRINT("Command : %d\n", htons(msg->command));
    DEBUG_PRINT("Payload_size : %d\n", htons(msg->payload_size));

    int _pos;
    if (msg->command == CA_PROTO_VERSION) {
      DEBUG_COMMENT("Valid CA_PROTO_VERSION\n");
      _pos = epics_process_version(src + pos);
      memcpy(dest + pos_dest, src + pos, _pos);
      pos += _pos;
      pos_dest += _pos;
    } else if (htons(msg->command) == CA_PROTO_SEARCH) {
      DEBUG_COMMENT("Valid CA_SEARCH_REQUEST\n");
      int _pos_dest = 0;
      _pos = epics_process_search(dest + pos_dest, src + pos, &_pos_dest,
                                  filter);
      pos += _pos;
      pos_dest += _pos_dest;
    } else if (htons(msg->command) == CA_PROTO_RSRV_IS_UP) {
      DEBUG_COMMENT("Valid CA_PROTO_RSRV_IS_UP\n");
      _pos = epics_process_beacon(src + pos);
      memcpy(dest + pos_dest, src + pos, _pos);
      pos += _pos;
      pos_dest += _pos;
    } else {
      DEBUG_PRINT("Unknown command %d\n", htons(msg->command));
      break;
    }
  }

  DEBUG_PRINT("pos_dest = %d\n", pos_dest);
  return pos_dest;
}
