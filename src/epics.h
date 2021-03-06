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
#ifndef SRC_EPICS_H_
#define SRC_EPICS_H_

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#define EPICS_PV_MAX_LEN      128
#define CA_PROTO_VERSION      0
#define CA_PROTO_SEARCH       6
#define CA_PROTO_RSRV_IS_UP   13

#define EPICS_TYPE_NONE       0x00
#define EPICS_TYPE_SEARCH     0x01
#define EPICS_TYPE_BEACON     0x02

struct epics_pv_filter {
  int sense;        // Sense !=0 explicit include
  int logic;        // Logic !=0 and else or
  struct epics_pv_filter_elem *next;
};

struct epics_pv_filter_elem {
  pcre2_code *re;
  struct epics_pv_filter_elem *next;
};

struct ca_proto_msg {
  uint16_t command;
  uint16_t payload_size;
  uint16_t data;
  uint16_t count;
  uint32_t param1;
  uint32_t param2;
} __attribute__((__packed__));

struct ca_proto_version {
  uint16_t command;
  uint16_t payload_size;
  uint16_t priority;
  uint16_t version;
  uint32_t rsrvd1;
  uint32_t rsrvd2;
} __attribute__((__packed__));

struct ca_proto_search {
  uint16_t command;
  uint16_t payload_size;
  uint16_t reply;
  uint16_t version;
  uint32_t cid1;
  uint32_t cid2;
} __attribute__((__packed__));

struct ca_proto_rsrv_is_up {
  uint16_t command;
  uint16_t reserved;
  uint16_t version;
  uint16_t port;
  uint32_t beaconid;
  uint32_t address;
} __attribute__((__packed__));

struct epics_pv {
  char name[EPICS_PV_MAX_LEN];
  int len;
  int cid1;
  int cid2;
};

struct epics_pv_filter_elem* epics_filter_load(const char *filename);
struct epics_pv_filter_elem* epics_filter_add(const char *exp);
int epics_read_packet(char* dest, const char* src, int len,
                      struct epics_pv_filter *filter);

#endif  // SRC_EPICS_H_
