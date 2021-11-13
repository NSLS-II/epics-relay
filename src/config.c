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

#include <sys/socket.h>
#include <netdb.h>
#include <libconfig.h>

#include "config.h"
#include "collector.h"
#include "emitter.h"
#include "debug.h"
#include "proto.h"
#include "defs.h"

int config_open_file(const char* filename, config_t *cfg) {
  if (!config_read_file(cfg, filename)) {
    ERROR_PRINT("Config file error %s:%d - %s\n", config_error_file(cfg),
                config_error_line(cfg), config_error_text(cfg));
    return -1;
  }

  return 0;
}

int config_read_emitter(const char* filename, emitter_params *params) {
  config_t cfg;
  config_setting_t *root, *emitter;
  const char *str;

  DEBUG_PRINT("Config file : %s\n", filename);

  config_init(&cfg);

  if (config_open_file(filename, &cfg)) {
    goto _error;
  }

  DEBUG_PRINT("Opened config file : %s\n", filename);

  root = config_root_setting(&cfg);

  emitter = config_setting_get_member(root, "emitter");
  if (!emitter) {
    ERROR_PRINT("Missing \"emitter\" element in %s\n", filename);
    goto _error;
  }

  if (!config_setting_lookup_string(emitter, "interface", &str)) {
    get_interface(NULL, &(params->iface));
  } else {
    if (get_interface(str, &(params->iface))) {
      ERROR_PRINT("Unable to get iface data for %s\n", str);
      goto _error;
    }
  }


  if (!config_setting_lookup_string(emitter, "epics_interface", &str)) {
    ERROR_COMMENT("You must specify the epics interface\n");
    goto _error;
  } else {
    if (get_interface(str, &(params->iface_epics))) {
      ERROR_PRINT("Unable to get iface data for %s\n", str);
      goto _error;
    }
    strncpy(params->iface_epics_name, str, sizeof(params->iface_epics_name));
  }

  config_destroy(&cfg);
  return 0;

_error:
  config_destroy(&cfg);
  return -1;
}


int config_read_collector(const char* filename, collector_params *params) {
  config_t cfg;
  config_setting_t *root, *collector, *regex, *emitter;
  const char *str;

  config_init(&cfg);

  if (config_open_file(filename, &cfg)) {
    goto _error;
  }

  DEBUG_PRINT("Opened config file : %s\n", filename);

  root = config_root_setting(&cfg);

  collector = config_setting_get_member(root, "collector");
  if (!collector) {
    ERROR_PRINT("Missing \"collector\" element in %s\n", filename);
    goto _error;
  }

  if (!config_setting_lookup_string(collector, "interface", &str)) {
    get_interface(NULL, &(params->iface));
  } else {
    if (get_interface(str, &(params->iface))) {
      ERROR_PRINT("Unable to get iface data for %s\n", str);
      goto _error;
    }
  }


  if (!config_setting_lookup_string(collector, "epics_interface", &str)) {
    get_interface(NULL, &(params->iface_listen));
  } else {
    if (get_interface(str, &(params->iface_listen))) {
      ERROR_PRINT("Unable to get iface data for %s\n", str);
      goto _error;
    }
  }

  // Emitter hostname
  if (!(emitter = config_setting_get_member(collector, "emitter"))) {
    ERROR_COMMENT("Unable to find emitter list\n");
    goto _error;
  }

  if (!config_setting_is_list(emitter)) {
    ERROR_COMMENT("Emitter must be a list\n");
    goto _error;
  }

  // Allocate memory
  params->num_fd = config_setting_length(emitter);
  params->fd = (int *)malloc(sizeof(int) * params->num_fd);
  if (!params->fd) {
    ERROR_COMMENT("Unable to allocate memory\n");
    goto _error;
  }

  params->emitter_addr = malloc(sizeof(struct sockaddr_in) * params->num_fd);
  if (!params->emitter_addr) {
    ERROR_COMMENT("Unable to allocate memory\n");
    goto _error;
  }

  for (int i = 0; i < params->num_fd; i++) {
    const char* str = config_setting_get_string_elem(emitter, i);

    // Convert hostname to IP
    struct hostent *he = gethostbyname(str);
    if (he == NULL) {
      ERROR_PRINT("Invalid hostname : %s\n", str);
      goto _error;
    }

    // Emitter setup
    memset(&(params->emitter_addr[i]), 0, sizeof(struct sockaddr_in));
    params->emitter_addr[i].sin_family = AF_INET;
    params->emitter_addr[i].sin_port = htons(PROTO_UDP_PORT);
    params->emitter_addr[i].sin_addr = *((struct in_addr*)he->h_addr);
  }

  // Get regex list
  if (!(regex = config_setting_get_member(collector, "regex"))) {
    // No regex list
    params->filter.next = NULL;
    params->filter.sense = 0;
    params->filter.logic = 0;
  } else {
    // Get rules
    config_setting_t *rules;
    if (!(rules = config_setting_get_member(regex, "rules"))) {
      ERROR_COMMENT("Rules missing from config file\n");
      goto _error;
    }

    if (!config_setting_is_list(rules)) {
      ERROR_COMMENT("Rules must be a list\n");
      goto _error;
    }

    struct epics_pv_filter_elem **current = &(params->filter.next);
    for (int i = 0; i < config_setting_length(rules); i++) {
      const char* rule = config_setting_get_string_elem(rules, i);
      *current = epics_filter_add(rule);
      if (current == NULL) {
        ERROR_COMMENT("Error setting regex rule\n");
        goto _error;
      }
      current = &((*current)->next);
    }

    // Process regex list
    if (!config_setting_lookup_bool(regex, "sense",
                                    &(params->filter.sense))) {
      params->filter.sense = 0;
    }
    if (!config_setting_lookup_bool(regex, "logic",
                                    &(params->filter.logic))) {
      params->filter.logic = 0;
    }
  }

  config_destroy(&cfg);
  return 0;

_error:
  config_destroy(&cfg);
  return -1;
}
