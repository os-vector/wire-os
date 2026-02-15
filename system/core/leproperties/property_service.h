/******************************************************************************
 *
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
 *
 *****************************************************************************/

#ifndef LE_PROP_SRVC_H
#define LE_PROP_SRVC_H

#include <stdbool.h>
#include <log/log.h>

#include "sys/system_properties.h"
#include "property_ops.h"

#define PROP_FILE_DEFAULT_PATH      "/build.prop"
#define PROP_FILE_PERSIST_PATH      "/etc/build.prop"
#define PROP_TRIGGER                "/etc/proptrigger.sh"
#define PROP_TRIGGER_CONF           "/etc/proptrigger.conf"

#undef  LOG_TAG
#define LOG_TAG "leprop-service"

#undef LOG_DEBUG
//#define LOG_DEBUG

#undef LOG
#ifdef LOG_DEBUG
  #define LOG(fmt, args...) \
        ALOGD("%s:%d " fmt "\n", __func__, __LINE__, ##args)
#else
  #define LOG(fmt, args...) do {} while(0)
#endif

#define UNUSED(x) (void)(x)

typedef struct property_unit{
    unsigned char property_name[PROP_NAME_MAX];     // name of property
    unsigned char property_value[PROP_VALUE_MAX];   // string val of property
    bool callback_to_be_invoked;                  // should the cb be invoked
} property_unit;

typedef struct property_db{
    property_unit unit;
    struct property_db *next;
} property_db;

typedef enum {
    CALLBACK_EXECUTE_PASS,
    CALLBACK_EXECUTE_FAILED,
} callback_state;

typedef enum {
    EXT_NAME, //name extraction index
    EXT_VAL   //val extraction index
} ext_value_index;

typedef callback_state (*user_callback)(const char *property_name,
                         const char* property_value);

/***********************************************************************
** Functions
***********************************************************************/

/**
 * Create the complete datastructure from the specifed file
 * @param Filename as path
 * @return True on success and false otherwise
 */
bool load_properties_from_file(const char *filename);

/**
 * Create datastructure from the default file at boot
 * @param Void since file name is known
 * @return True on success and false otherwise
 */
bool load_default_properties(void);

/**
 * Create datastructure from file with persist properties
 * normally present in userdata and has write permissions
 * @param Void as file name is known
 * @return True on success and false otherwise
 */
bool load_persist_properties(void);

/**
 * Save the properties on RAM with persist.* prefix into
 * persist file present in userdata
 * @param Void since the file and ds head are known
 * @return True on success and false otherwise
 */
bool save_persist_ds_to_file(void);

/**
 * Remove the list node with the matching property name
 * @param property name as char*, which has to be removed
 * @return True on success and false otherwise
 */
int remove_ds_node(const char*);

/**
 * Create the complete data list from the persist for first time
 * @param Filename as path
 * @return True on success and false otherwise
 */
void dump_current_ds(void);

/**
 * UNUSED for the moment - used to dump all the persist properties
 * @param Filename as path
 * @return True on success and false otherwise
 */
void dump_persist(void);

/**
 * Function to search from the file line by line
 * and add as new nodes in the data structure
 * @param file name as path (persist file with initial values)
 * @return true for success false otherwise
 */
bool search_and_add_property_val(const char* fpath);

/**
 * Function to Pull the values from property in one line
 * and line will be passed as input for parsing
 * @param file name as path (persist file with initial values)
 * @return true for success false otherwise
 */
property_db* pull_one_line_data(const char*);

/**
 * Update datastructure with property value for
 * a property name recived via socket
 * @param Char * for property name
 * @return 0 on success and 1 otherwise
 */
property_db* process_setprop_msg(char*);

/**
 * Get the property value for datastructure for
 * a given property name received via socket
 * @param Char * for property name
 * @return ds node on success and NULL otherwise
 */
property_db* process_getprop_msg(char*);

/**
 * Function to create_socket to establish communication
 * with libcutils library.
 * @param socket name,  socket type, permissions
 * @return socket discriptor on success -1 otherwise
 */
int create_socket(const char *, int, mode_t, uid_t, gid_t);

#endif /* LE_PROP_SRVC_H */
