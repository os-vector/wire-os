/******************************************************************************
 *
 *  Copyright (C) 2014 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#define LOG_TAG "bt_classic_peer"

#include "device/include/classic/peer.h"

#include <assert.h>
#include <cutils/log.h>
#include <pthread.h>
#include <stdbool.h>

#include "btcore/include/module.h"
#include "osi/include/allocator.h"
#include "osi/include/future.h"
#include "osi/include/hash_map.h"
#include "osi/include/osi.h"

struct classic_peer_t {
  bt_bdaddr_t address;
};

static const size_t number_of_address_buckets = 42;

static bool initialized;
static pthread_mutex_t bag_of_peers_lock;
static hash_map_t *peers_by_address;

static bool bdaddr_equality_fn(const void *x, const void *y);

// Module lifecycle functions

static future_t *init(void) {
  peers_by_address = hash_map_new(
    number_of_address_buckets,
    hash_function_bdaddr,
    NULL,
    osi_free,
    bdaddr_equality_fn);

  pthread_mutex_init(&bag_of_peers_lock, NULL);

  initialized = true;
  return NULL;
}

static future_t *clean_up(void) {
  initialized = false;

  hash_map_free(peers_by_address);
  peers_by_address = NULL;

  pthread_mutex_destroy(&bag_of_peers_lock);
  return NULL;
}

//TODO: Fix this
#ifndef ANDROID
#define EXPORT_SYMBOL   __attribute__((visibility("default")))
#endif

EXPORT_SYMBOL const module_t classic_peer_module = {
  .name = CLASSIC_PEER_MODULE,
  .init = init,
  .start_up = NULL,
  .shut_down = NULL,
  .clean_up = clean_up,
  .dependencies = {
    NULL
  }
};

// Interface functions

classic_peer_t *classic_peer_by_address(bt_bdaddr_t *address) {
  assert(initialized);
  assert(address != NULL);

  classic_peer_t *peer = hash_map_get(peers_by_address, address);

  if (!peer) {
    pthread_mutex_lock(&bag_of_peers_lock);

    // Make sure it didn't get added in the meantime
    peer = hash_map_get(peers_by_address, address);
    if (peer)
      goto done;

    // Splice in a new peer struct on behalf of the caller.
    peer = osi_calloc(sizeof(classic_peer_t));
    peer->address = *address;
    hash_map_set(peers_by_address, &peer->address, peer);

    pthread_mutex_unlock(&bag_of_peers_lock);
  }

done:
  return peer;
}

const bt_bdaddr_t *classic_peer_get_address(classic_peer_t *peer) {
  assert(peer != NULL);
  return &peer->address;
}

// Internal functions

// Wrapper for bdaddr_equals used in the hash map of peers by address
static bool bdaddr_equality_fn(const void *x, const void *y) {
  return bdaddr_equals((bt_bdaddr_t *)x, (bt_bdaddr_t *)y);
}
