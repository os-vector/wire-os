/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/************************************************************************************
 *
 *  Filename:      btif_uid.c
 *
 *  Description:   Contains data structures and functions for keeping track of
 *                 socket usage per app UID.
 *
 ***********************************************************************************/
#define LOG_TAG "bt_uid_set"
#include "bt_common.h"
#include "btif_uid.h"

#include <log/log.h>
#include <pthread.h>

typedef struct uid_set_node_t {
    struct uid_set_node_t* next;
    bt_uid_traffic_t data;
} uid_set_node_t;

typedef struct uid_set_t {
    pthread_mutex_t lock;
    uid_set_node_t* head;
} uid_set_t;

uid_set_t* uid_set_create(void) {
    uid_set_t* set = osi_calloc(sizeof(uid_set_t));
    pthread_mutex_init(&set->lock, NULL);
    return set;
}

void uid_set_destroy(uid_set_t* set) {
    pthread_mutex_lock(&set->lock);
    uid_set_node_t* node = set->head;
    while (node) {
        uid_set_node_t* temp = node;
        node = node->next;
        osi_free(temp);
    }
    set->head = NULL;
    pthread_mutex_unlock(&set->lock);
    pthread_mutex_destroy(&set->lock);
    osi_free(set);
}

// Lock in uid_set_t must be held.
static uid_set_node_t* uid_set_find_or_create_node(uid_set_t* set, int32_t app_uid) {
    uid_set_node_t* node = set->head;
    while (node && node->data.app_uid != app_uid) {
        node = node->next;
    }

    if (!node) {
        node = osi_calloc(sizeof(uid_set_node_t));
        node->data.app_uid = app_uid;
        node->next = set->head;
        set->head = node;
    }
    return node;
}

void uid_set_add_tx(uid_set_t* set, int32_t app_uid, uint64_t bytes) {
    if (app_uid == -1 || bytes == 0) {
        return;
    }

    pthread_mutex_lock(&set->lock);
    uid_set_node_t* node = uid_set_find_or_create_node(set, app_uid);
    node->data.tx_bytes += bytes;
    pthread_mutex_unlock(&set->lock);
}

void uid_set_add_rx(uid_set_t* set, int32_t app_uid, uint64_t bytes) {
    if (app_uid == -1 || bytes == 0) {
        return;
    }

    pthread_mutex_lock(&set->lock);
    uid_set_node_t* node = uid_set_find_or_create_node(set, app_uid);
    node->data.rx_bytes += bytes;
    pthread_mutex_unlock(&set->lock);
}

bt_uid_traffic_t* uid_set_read_and_clear(uid_set_t* set) {
    pthread_mutex_lock(&set->lock);

    // Find the length
    size_t len = 0;
    uid_set_node_t* node = set->head;
    while (node) {
        len++;
        node = node->next;
    }

    // Allocate an array of elements + 1, to signify the end with app_uid set to -1.
    bt_uid_traffic_t* result = osi_calloc(sizeof(bt_uid_traffic_t) * (len + 1));

    bt_uid_traffic_t* data = result;
    node = set->head;
    while (node) {
        // Copy the data.
        *data = node->data;
        data++;

        // Clear the counters.
        node->data.rx_bytes = 0;
        node->data.tx_bytes = 0;
        node = node->next;
    }

    // Mark the last entry
    data->app_uid = -1;

    pthread_mutex_unlock(&set->lock);

    return result;
}
