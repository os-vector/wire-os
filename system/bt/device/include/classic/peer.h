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

#pragma once

#include "btcore/include/bdaddr.h"

static const char CLASSIC_PEER_MODULE[] = "classic_peer_module";

typedef struct classic_peer_t classic_peer_t;

// Returns a classic_peer_t for the provided |address|. If the peer
// already exists, that instance is returned. Otherwise a classic_peer_t
// is constructed for that |address| and then returned. |address| may not
// be NULL.
classic_peer_t *classic_peer_by_address(bt_bdaddr_t *address);

// Returns the bluetooth address of the |peer|. |peer| may not be NULL.
const bt_bdaddr_t *classic_peer_get_address(classic_peer_t *peer);
