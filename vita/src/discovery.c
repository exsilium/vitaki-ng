#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <chiaki/discoveryservice.h>
#include <chiaki/log.h>

#include <netinet/in.h>
#include <sys/socket.h>

#include "discovery.h"
#include "context.h"
#include "host.h"
#include "util.h"

/// Save a newly discovered host into the context
void save_discovered_host(ChiakiDiscoveryHost* host) {
  CHIAKI_LOGI(&(context.log), "Saving discovered host...");
  // Check if the host is already known, and if not, locate a free spot for it
  uint8_t host_mac[6];
  parse_b64(host->host_id, host_mac, sizeof(host_mac));
  int target_idx = -1;
  VitaChiakiHost* h;
  // TODO fix this logic...
  for (int host_idx = 0; host_idx < MAX_NUM_HOSTS; host_idx++) {
    h = context.hosts[host_idx];
    if (h == NULL) {
      // Found a free spot
      if (target_idx < 0) {
        target_idx = host_idx;
        break;
      }
    } else if (h->type & DISCOVERED) {
      if (mac_addrs_match(&(h->server_mac), &host_mac)) {
        // Already known discovered hosts, we can skip saving
        return;
      }
    } else if (h->type & MANUALLY_ADDED) {
      if (mac_addrs_match(&(h->server_mac), &host_mac)) {
        // Manually added host matched this discovered host, update
        target_idx = host_idx;
      }
    }
  }

  // Maximum number of hosts reached, can't save host
  // TODO: Indicate to user?
  if (target_idx < 0) {
    CHIAKI_LOGE(&(context.log), "Max # of hosts reached; could not save newly discovered host.");
    return;
  }

  if (!h) {
    h = (VitaChiakiHost*)malloc(sizeof(VitaChiakiHost));
    h->registered_state = NULL;
    h->hostname = NULL;
  }

  CHIAKI_LOGI(&(context.log), "--");
  CHIAKI_LOGI(&(context.log), "Discovered Host:");
  CHIAKI_LOGI(&(context.log), "State:                             %s", chiaki_discovery_host_state_string(host->state));

  if(host->system_version)
    CHIAKI_LOGI(&(context.log), "System Version:                    %s", host->system_version);

  if(host->device_discovery_protocol_version)
    CHIAKI_LOGI(&(context.log), "Device Discovery Protocol Version: %s", host->device_discovery_protocol_version);

  if(host->host_request_port)
    CHIAKI_LOGI(&(context.log), "Request Port:                      %hu", (unsigned short)host->host_request_port);

  if(host->host_name)
    CHIAKI_LOGI(&(context.log), "Host Name:                         %s", host->host_name);

  if(host->host_type)
    CHIAKI_LOGI(&(context.log), "Host Type:                         %s", host->host_type);

  if(host->host_id)
    CHIAKI_LOGI(&(context.log), "Host ID:                           %s", host->host_id);

  if(host->running_app_titleid)
    CHIAKI_LOGI(&(context.log), "Running App Title ID:              %s", host->running_app_titleid);

  if(host->running_app_name)
    CHIAKI_LOGI(&(context.log), "Running App Name:                  %s%s", host->running_app_name, (strcmp(host->running_app_name, "Persona 5") == 0 ? " (best game ever)" : ""));

  h->type |= DISCOVERED;
  ChiakiTarget target = chiaki_discovery_host_system_version_target(host);
  CHIAKI_LOGI(&(context.log),   "Is PS5:                            %s", chiaki_target_is_ps5(target) ? "true" : "false");
  h->target = target;
  memcpy(&(h->server_mac), &host_mac, 6);
  h->discovery_state =
      (ChiakiDiscoveryHost*)malloc(sizeof(ChiakiDiscoveryHost));
  memcpy(h->discovery_state, host, sizeof(ChiakiDiscoveryHost));
  h->hostname = strdup(h->discovery_state->host_addr);

  CHIAKI_LOGI(&(context.log), "--");

  context.hosts[target_idx] = h;
  context.num_hosts++;

  // Check if the newly discovered host is a known registered one
  for (int rhost_idx = 0; rhost_idx < context.config.num_registered_hosts; rhost_idx++) {
    VitaChiakiHost* rhost =
        context.config.registered_hosts[rhost_idx];
    if (rhost == NULL) {
      continue;
    }

    // FIXME: Use MAC instead of name
    printf("FOUND HOST\n");
    printf("NAME1 %s\n", rhost->registered_state->server_nickname);
    printf("NAME2 %s\n", h->discovery_state->host_name);
    if (strcmp(rhost->registered_state->server_nickname, h->discovery_state->host_name) == 0) {
      printf("FOUND MATCH\n");
      h->registered_state = rhost->registered_state;
      h->type |= REGISTERED;
      break;
    }
  }
}

/// Called whenever new hosts are discovered
void discovery_cb(ChiakiDiscoveryHost* hosts, size_t hosts_count, void* user) {
  for (int dhost_idx = 0; dhost_idx < hosts_count; dhost_idx++) {
    save_discovered_host(&hosts[dhost_idx]);
  }

  // Call caller-defined callback
  VitaChiakiDiscoveryCallbackState* cb_state =
      (VitaChiakiDiscoveryCallbackState*)user;
  if (cb_state && cb_state->cb) {
    cb_state->cb(cb_state->cb_user);
  }
}

/// Initiate the Chiaki discovery thread
ChiakiErrorCode start_discovery(VitaChiakiDiscoveryCb cb, void* cb_user) {
  if (context.discovery_enabled) {
    return CHIAKI_ERR_SUCCESS;
  }
  if (cb != NULL) {
    context.discovery_cb_state =
        malloc(sizeof(VitaChiakiDiscoveryCallbackState));
    context.discovery_cb_state->cb = cb;
    context.discovery_cb_state->cb_user = cb_user;
  }
  ChiakiDiscoveryServiceOptions opts;
  opts.cb = discovery_cb;
  opts.cb_user = context.discovery_cb_state;
  opts.ping_ms = 500;
  opts.ping_initial_ms = opts.ping_ms;
  opts.hosts_max = MAX_NUM_HOSTS;
  opts.host_drop_pings = HOST_DROP_PINGS;

  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_BROADCAST;
  opts.send_addr = (struct sockaddr*) &addr;
  opts.send_addr_size = sizeof(addr);
  opts.send_host = NULL;

  ChiakiErrorCode err = chiaki_discovery_service_init(&(context.discovery),
                                                      &opts, &(context.log));
  context.discovery_enabled = true;
  return err;
}

/// Terminate the Chiaki discovery thread, clean up discovey state in context
void stop_discovery() {
  if (!context.discovery_enabled) {
    return;
  }
  chiaki_discovery_service_fini(&(context.discovery));
  context.discovery_enabled = false;
  for (int i = 0; i < MAX_NUM_HOSTS; i++) {
    VitaChiakiHost* h = context.hosts[i];
    if (h == NULL) {
      continue;
    }
    if (!(h->type & MANUALLY_ADDED)) {
      host_free(h);
      context.hosts[i] = NULL;
      context.num_hosts--;
    } else if (h->type & DISCOVERED) {
      // Discovered host that's also manually added, only free the discovery
      // state
      free(h->discovery_state);
    }
  }
  if (context.discovery_cb_state != NULL) {
    free(context.discovery_cb_state);
    context.discovery_cb_state = NULL;
  }
}