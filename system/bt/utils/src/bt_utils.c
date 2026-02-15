/******************************************************************************
 *
 *  Copyright (C) 2012 Broadcom Corporation
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

/************************************************************************************
 *
 *  Filename:      bt_utils.c
 *
 *  Description:   Miscellaneous helper functions
 *
 *
 ***********************************************************************************/

#define LOG_TAG "bt_utils"

#include "bt_utils.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>
#ifdef ANDROID
#include <utils/ThreadDefs.h>
#include <cutils/sched_policy.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/un.h>
#include <sys/time.h>
#include <fcntl.h>
#define SOCKETNAME  "/data/misc/bluetooth/btprop"
#endif
#include "bt_types.h"
#include "btcore/include/module.h"
#include "osi/include/compat.h"
#include "osi/include/allocator.h"
#include "osi/include/log.h"
#include "osi/include/properties.h"
#include "osi/include/list.h"
#include <string.h>

/*******************************************************************************
**  Local type definitions
*******************************************************************************/
typedef struct {
    char header_name[MAX_NAME_LEN];             // name of header in iot_devlist_conf file
    list_t *devlist;                  // list of BD addresses
    tBLACKLIST_METHOD method_type;
} iot_header_node_t;

typedef struct {
    char dev_bd[3];                  // BD address of blacklisted device
} iot_devlist_bd_node_t;

typedef struct {
    char dev_name[MAX_NAME_LEN];              // Name of blacklisted device
} iot_devlist_name_node_t;

typedef struct {
    char *header;                   // header name
    unsigned char *dev_details;              // details of blacklisted device
    bool device_found;
} iot_input_param;

typedef struct {
    bt_soc_type soc_type;
    char* soc_name;
} soc_type_node;

static soc_type_node soc_type_entries[] = {
                           { BT_SOC_SMD , "smd" },
                           { BT_SOC_AR3K , "ath3k" },
                           { BT_SOC_ROME , "rome" },
                           { BT_SOC_CHEROKEE , "cherokee" },
                           { BT_SOC_RESERVED , "" }
                                       };

static list_t *iot_header_queue = NULL;
#define MAX_LINE 2048
#define MAX_ADDR_STR_LEN 9
static pthread_mutex_t         iot_mutex_lock;
/*******************************************************************************
**  Type definitions for callback functions
********************************************************************************/
static pthread_once_t g_DoSchedulingGroupOnce[TASK_HIGH_MAX];
static BOOLEAN g_DoSchedulingGroup[TASK_HIGH_MAX];
static pthread_mutex_t         gIdxLock;
static int g_TaskIdx;
static int g_TaskIDs[TASK_HIGH_MAX];
static bt_soc_type soc_type;
#define INVALID_TASK_ID  (-1)

static void init_soc_type();

#ifndef ANDROID
static int bt_prop_socket;      /* This end of connection*/
#endif

static future_t *init(void) {
  int i;
  pthread_mutexattr_t lock_attr;

#ifndef ANDROID
  int len;    /* length of sockaddr */
  struct sockaddr_un name;
  if( (bt_prop_socket = socket(AF_UNIX, SOCK_STREAM, 0) ) < 0) {
    perror("socket");
    exit(1);
  }
  /*Create the address of the server.*/
  memset(&name, 0, sizeof(struct sockaddr_un));
  name.sun_family = AF_UNIX;
  strlcpy(name.sun_path, SOCKETNAME, sizeof(name.sun_path));
  len = sizeof(name.sun_family) + strlen(name.sun_path);
  /*Connect to the server.*/
  if (connect(bt_prop_socket, (struct sockaddr *) &name, len) < 0){
      perror("connect");
      exit(1);
  }
#endif

  for(i = 0; i < TASK_HIGH_MAX; i++) {
    g_DoSchedulingGroupOnce[i] = PTHREAD_ONCE_INIT;
    g_DoSchedulingGroup[i] = TRUE;
    g_TaskIDs[i] = INVALID_TASK_ID;
  }

  pthread_mutexattr_init(&lock_attr);
  pthread_mutex_init(&gIdxLock, &lock_attr);
  pthread_mutex_init(&iot_mutex_lock, NULL);
  init_soc_type();
  return NULL;
}

void bt_utils_cleanup( void)
{
  pthread_mutex_destroy(&gIdxLock);
  pthread_mutex_destroy(&iot_mutex_lock);
#ifndef ANDROID
  shutdown(bt_prop_socket, SHUT_RDWR);
  close(bt_prop_socket);
#endif
}

static future_t *clean_up(void) {
  int i;
  pthread_mutex_destroy(&gIdxLock);
  pthread_mutex_destroy(&iot_mutex_lock);
#ifndef ANDROID
  shutdown(bt_prop_socket, SHUT_RDWR);
  close(bt_prop_socket);
#endif
  return NULL;
}

//TODO: Fix this
#ifndef ANDROID
#define EXPORT_SYMBOL   __attribute__((visibility("default")))
#endif

EXPORT_SYMBOL const module_t bt_utils_module = {
  .name = BT_UTILS_MODULE,
  .init = init,
  .start_up = NULL,
  .shut_down = NULL,
  .clean_up = clean_up,
  .dependencies = {
    NULL
  }
};

/*****************************************************************************
**
** Function        check_do_scheduling_group
**
** Description     check if it is ok to change schedule group
**
** Returns         void
**
*******************************************************************************/
static void check_do_scheduling_group(void) {
    char buf[PROPERTY_VALUE_MAX];
    int len = osi_property_get("debug.sys.noschedgroups", buf, "");
    if (len > 0) {
        int temp;
        if (sscanf(buf, "%d", &temp) == 1) {
            g_DoSchedulingGroup[g_TaskIdx] = temp == 0;
        }
    }
}

#ifndef ANDROID
int property_get_bt(const char *key, char *value, const char *default_value)
{
    char prop_string[200] = {'\0'};
    int ret, bytes_read = 0, i = 0;

    snprintf(prop_string, sizeof(prop_string), "get_property %s,", key);
    ret = send(bt_prop_socket, prop_string, strlen(prop_string), 0);
    memset(value, 0, sizeof(value));
    do
    {
        bytes_read = recv(bt_prop_socket, &value[i], 1, 0);
        if (bytes_read == 1)
        {
            if (value[i] == ',')
            {
                value[i] = '\0';
                break;
            }
            i++;
        }
    } while(1);
    ALOGD("property_get_bt: key(%s) has value: %s", key, value);
    if (!i && default_value)
    {
        ALOGD("property_get_bt: Copied default =%s", default_value);
        strlcpy(value, default_value, strlen(default_value)+1);
        return 1;
    }
    return 0;
}

/* property_set_bt: returns 0 on success, < 0 on failure
*/
int property_set_bt(const char *key, const char *value)
{
    char prop_string[200] = {'\0'};
    int ret;
    snprintf(prop_string, sizeof(prop_string), "set_property %s %s,", key, value);
    ALOGD("property_set_bt: setting key(%s) to value: %s\n", key, value);
    ret = send(bt_prop_socket, prop_string, strlen(prop_string), 0);
    return 0;
}
#endif

/*****************************************************************************
**
** Function        raise_priority_a2dp
**
** Description     Raise task priority for A2DP streaming
**
** Returns         void
**
*******************************************************************************/
void raise_priority_a2dp(tHIGH_PRIORITY_TASK high_task) {
#ifdef ANDROID
    int rc = 0;
    int tid = gettid();
    int priority = ANDROID_PRIORITY_AUDIO;

    pthread_mutex_lock(&gIdxLock);
    g_TaskIdx = high_task;

    // TODO(armansito): Remove this conditional check once we find a solution
    // for system/core on non-Android platforms.
#if defined(OS_GENERIC)
    rc = -1;
#else  // !defined(OS_GENERIC)
    pthread_once(&g_DoSchedulingGroupOnce[g_TaskIdx], check_do_scheduling_group);
    if (g_TaskIdx < TASK_HIGH_MAX && g_DoSchedulingGroup[g_TaskIdx]) {
        // set_sched_policy does not support tid == 0
        rc = set_sched_policy(tid, SP_AUDIO_SYS);
    }
#endif  // defined(OS_GENERIC)

    g_TaskIDs[high_task] = tid;
    pthread_mutex_unlock(&gIdxLock);

    if (rc) {
        LOG_WARN(LOG_TAG, "failed to change sched policy, tid %d, err: %d", tid, errno);
    }

    // always use urgent priority for HCI worker thread until we can adjust
    // its prio individually. All other threads can be dynamically adjusted voa
    // adjust_priority_a2dp()

    priority = ANDROID_PRIORITY_URGENT_AUDIO;

    if (setpriority(PRIO_PROCESS, tid, priority) < 0) {
        LOG_WARN(LOG_TAG, "failed to change priority tid: %d to %d", tid, priority);
    }
#endif
}

/*****************************************************************************
**
** Function        adjust_priority_a2dp
**
** Description     increase the a2dp consumer task priority temporarily when start
**                 audio playing, to avoid overflow the audio packet queue, restore
**                 the a2dp consumer task priority when stop audio playing.
**
** Returns         void
**
*******************************************************************************/
void adjust_priority_a2dp(int start) {
#ifdef ANDROID
    int priority = start ? ANDROID_PRIORITY_URGENT_AUDIO : ANDROID_PRIORITY_AUDIO;
    int tid;
    int i;

    for (i = 0; i < TASK_HIGH_MAX; i++)
    {
        tid = g_TaskIDs[i];
        if (tid != INVALID_TASK_ID)
        {
            if (setpriority(PRIO_PROCESS, tid, priority) < 0)
            {
                LOG_WARN(LOG_TAG, "failed to change priority tid: %d to %d", tid, priority);
            }
        }
    }
#endif
}

/*****************************************************************************
**
** Function        check_bd_cb
**
** Description     Compares the BD address.
**
** Returns         returns true if the BD address matches otherwise false
**
*******************************************************************************/
static bool check_bd_cb(void* node, void* cb_data)
{
    iot_devlist_bd_node_t *bd_node = (iot_devlist_bd_node_t*)node;
    iot_input_param *input_param = (iot_input_param*)cb_data;

    if (input_param->device_found == true)
        return true;

    if ((bd_node->dev_bd[0] == input_param->dev_details[0]) &&
        (bd_node->dev_bd[1] == input_param->dev_details[1]) &&
        (bd_node->dev_bd[2] == input_param->dev_details[2])) {
        input_param->device_found = true;
        return true;
    }
    return false;
}

/*****************************************************************************
**
** Function        check_name_cb
**
** Description     Compares the Device name.
**
** Returns         returns true if the name matches otherwise false
**
*******************************************************************************/
static bool check_name_cb(void* node, void* cb_data)
{
    iot_devlist_name_node_t *name_node = (iot_devlist_name_node_t*)node;
    iot_input_param *input_param = (iot_input_param*)cb_data;

    if (input_param->device_found == true)
        return true;

    if (!strncmp(name_node->dev_name, (const char*)input_param->dev_details,
                                                strlen((char *)input_param->dev_details))) {
        input_param->device_found = true;
        return true;
    }
    return false;
}

/*****************************************************************************
**
** Function        check_header_cb
**
** Description     Iterates through the each entry in the header list and
**                 calls the callback associated to each entry.
**
** Returns         boolean
**
*******************************************************************************/
static bool check_header_cb(void* node, void* cb_data)
{
    iot_header_node_t *header_node = (iot_header_node_t*)node;
    iot_input_param *input_param = (iot_input_param*)cb_data;
    if (!strcmp(header_node->header_name, input_param->header)) {
        if(header_node->devlist) {
            if (header_node->method_type == METHOD_BD)
                list_foreach_ext(header_node->devlist, check_bd_cb, cb_data);
            else if (header_node->method_type == METHOD_NAME)
                list_foreach_ext(header_node->devlist, check_name_cb, cb_data);
        }
    }
    return true;
}

/*****************************************************************************
**
** Function        is_device_present
**
** Description     Checks if the device is already present in the blacklisted
**                 device list or not.The input can be address based or name
**                 based.
**
** Returns         true incase device is present false otherwise.
**
*******************************************************************************/
bool is_device_present(char* header, unsigned char* device_details)
{
    iot_input_param input_param;
    input_param.dev_details = device_details;
    input_param.header = header;
    input_param.device_found = false;

    pthread_mutex_lock(&iot_mutex_lock);
    if (!iot_header_queue) {
        pthread_mutex_unlock(&iot_mutex_lock);
        return false;
    }
    list_foreach_ext(iot_header_queue, check_header_cb, &input_param);
    pthread_mutex_unlock(&iot_mutex_lock);

    if (input_param.device_found)
        return true;
    else
        return false;
}

/*****************************************************************************
**
** Function        parse_bd
**
** Description     It will read 3 bytes and copy them into node. It also
**                 increments header pointer.
**
** Returns         void.
**
*******************************************************************************/
static void parse_bd(char **start_ptr, iot_devlist_bd_node_t *bd)
{
    char *p_end;
    bd->dev_bd[0] = (unsigned char)strtol(*start_ptr, &p_end, 16);
    (*start_ptr) = p_end + 1;
    bd->dev_bd[1] = (unsigned char)strtol(*start_ptr, &p_end, 16);
    (*start_ptr) = p_end + 1;
    bd->dev_bd[2] = (unsigned char)strtol(*start_ptr, &p_end, 16);
    (*start_ptr) = p_end;
}

/*****************************************************************************
**
** Function        parse_name
**
** Description     It will read name and copy them into node. It also
**                 increments header pointer.
**
** Returns         void.
**
*******************************************************************************/
static void parse_name(char **start_ptr, iot_devlist_name_node_t *name)
{
    char *split = strchr(*start_ptr, ','); // split point to first occurrence of ,
    int len = 0;
    if (split == NULL) {
        // check once for end of line, for the last name in list
        split = strchr(*start_ptr, '\n');
        if (split == NULL)
            return;
    }
    len = (((split - (*start_ptr)) >= MAX_NAME_LEN) ? MAX_NAME_LEN - 1 :
                                            (split - (*start_ptr)));
    memcpy(name->dev_name, *start_ptr, len);
    name->dev_name[len] = '\0';
    *start_ptr = split;
}

/*****************************************************************************
**
** Function        is_device_node_exist
**
** Description     Checks if the device node is already present in the queue
**                 or not.The input can be address based or name  based.
**
** Returns         true if the entry found else false.
**
*******************************************************************************/
static bool is_device_node_exist(iot_header_node_t *header_entry, char* device_details,
                    tBLACKLIST_METHOD method_type)
{
    if(!header_entry || !header_entry->devlist)
        return false;

    for (const list_node_t *device_node = list_begin(header_entry->devlist);
            device_node != list_end(header_entry->devlist);
            device_node = list_next(device_node)) {
        if(method_type == METHOD_BD) {
            iot_devlist_bd_node_t *bd_addr_entry = list_node(device_node);
            if(!memcmp(device_details, bd_addr_entry->dev_bd, 3)) {
                return true;
            }
        }
        else if(method_type == METHOD_NAME) {
            iot_devlist_name_node_t *bd_name_entry = list_node(device_node);
            if(!strcmp((char *)device_details, bd_name_entry->dev_name)) {
                return true;
            }
        }
    }
    return false;
}

/*****************************************************************************
**
** Function        populate_list
**
** Description     It goes through the input buffer and add device node to the
**                 header list if the valid entry is found.It ignores the
**                 duplicated entries.
**
** Returns         void.
**
*******************************************************************************/
static void populate_list(char *header_end, iot_header_node_t *node)
{
    if(node->devlist == NULL)
        node->devlist = list_new(osi_free);
    while(header_end && (*header_end != '\n')&&(*header_end != '\0')) // till end of line reached
    {
        // read from line buffer and copy to list
        if (node->method_type == METHOD_BD) {
            iot_devlist_bd_node_t *bd = (iot_devlist_bd_node_t *)osi_malloc(sizeof(iot_devlist_bd_node_t));
            if(bd == NULL) {
                ALOGE(" Unable to allocate memory for addr entry");
                return;
            }
            header_end++;
            parse_bd(&header_end, bd);
            if(is_device_node_exist(node, (char *) bd, node->method_type)) {
                osi_free(bd);
            }
            else {
                list_append(node->devlist, bd);
            }
        }
        else if (node->method_type == METHOD_NAME) {
            iot_devlist_name_node_t *name = (iot_devlist_name_node_t *)osi_malloc(sizeof(iot_devlist_name_node_t));
            if(name == NULL) {
                ALOGE(" Unable to allocate memory for name entry");
                return;
            }
            header_end++;
            parse_name(&header_end, name);
            if(is_device_node_exist(node, (char *)name, node->method_type)) {
                osi_free(name);
            }
            else {
                list_append(node->devlist, name);
            }
        }
    }
}

/*****************************************************************************
**
** Function        create_header_node
**
** Description     This function is used to create the header node.
**
** Returns         valid pointer incase the node is created otherwise NULL.
**
*******************************************************************************/
static iot_header_node_t *create_header_node(char* name, unsigned int len,
                        tBLACKLIST_METHOD method_type)
{
    iot_header_node_t *node = NULL;
    if(len >= MAX_NAME_LEN) {
        return NULL;
    }
    node =(iot_header_node_t *) osi_malloc(sizeof(iot_header_node_t));
    if (node == NULL) {
       ALOGE(" Not enough memory to create the header node");
       return NULL;
    }
    memcpy(node->header_name, name, len);
    node->header_name[len] = '\0';  // header copied
    node->method_type = method_type;
    node->devlist = NULL;
    return node;
}

/*****************************************************************************
**
** Function        get_existing_header_node
**
** Description     This function is used to get exisiting header node if present.
**
** Returns         valid pointer incase the node is already prsent otherwise NULL.
**
*******************************************************************************/
static iot_header_node_t *get_existing_header_node(char* name, unsigned int len)
{
    for (const list_node_t *node = list_begin(iot_header_queue);
           node != list_end(iot_header_queue); node = list_next(node)) {
        iot_header_node_t *entry = list_node(node);
        if (!strncmp(entry->header_name, name, len)) {
            return entry;
        }
    }
    return NULL;
}

/*****************************************************************************
**
** Function        populate_header
**
** Description     It goes through the input buffer and add header node to the
**                 main queue if the valid entry is found.It ignores the
**                 duplicated entries.
**
** Returns         void.
**
*******************************************************************************/
static void populate_header(char* line_start, char *header_end)
{
    tBLACKLIST_METHOD method_type;
    iot_header_node_t *node = NULL;

    if (*(header_end + 3) == ':')
        method_type = METHOD_BD;
    else
        method_type = METHOD_NAME;

    if (!iot_header_queue) {
        iot_header_queue = list_new(osi_free);
        if (iot_header_queue == NULL) {
            ALOGE(" Not enough memory  to create the queue");
            return;
        }
    }

    if( (node = get_existing_header_node(line_start,  header_end - line_start)) == NULL) {
        node = create_header_node(line_start, header_end - line_start, method_type);
        if(node)
            list_append(iot_header_queue, node);
    }
    if(node)
        populate_list(header_end, node);
}

/*****************************************************************************
**
** Function        free_header_list
**
** Description     This function is used to free all entries under blacklist
**                 queue.
**
** Returns         boolean
**
*******************************************************************************/
static bool free_header_list(void* node, void *context)
{
    iot_header_node_t *header_node = (iot_header_node_t*)node;
    list_free(header_node->devlist);
    return true;
}

/*****************************************************************************
**
** Function        unload_iot_devlist
**
** Description     This function is used to free the IOT blacklist queue.
**
** Returns         void
**
*******************************************************************************/
void unload_iot_devlist()
{
    pthread_mutex_lock(&iot_mutex_lock);
    if (!iot_header_queue) {
        ALOGV(" Blacklist queue is not initialized ");
        pthread_mutex_unlock(&iot_mutex_lock);
        return;
    }
    list_foreach(iot_header_queue, free_header_list, NULL);
    list_free(iot_header_queue);
    iot_header_queue = NULL;
    pthread_mutex_unlock(&iot_mutex_lock);
}

/*****************************************************************************
**
** Function        copy_file
**
** Description     This function is used to copy one file to other.
**
** Returns         true incase copy is successful otherwise false.
**
*******************************************************************************/
static bool copy_file(const char *src, const char *dst)
{
    FILE *src_fp = NULL, *dst_fp = NULL;
    int ch;

    if( !src || !dst)  {
        return false;
    }
    src_fp = fopen(src, "rt");
    if(src_fp)
        dst_fp = fopen(dst, "wt");
    if(src_fp && dst_fp) {
        while( ( ch = fgetc(src_fp) ) != EOF ) {
            fputc(ch, dst_fp);
        }
        fclose(dst_fp);
        fclose(src_fp);
        return true;
    }
    else {
        if(src_fp)
            fclose(src_fp);
        if(dst_fp)
            fclose(dst_fp);
        return false;
    }
}

/*****************************************************************************
**
** Function        dump_all_iot_devices
**
** Description     This function is used to print all blacklisted devices
**                 which are loaded from iot_devlist.conf file..
**
** Returns         void.
**
*******************************************************************************/
static void dump_all_iot_devices(void)
{
    tBLACKLIST_METHOD method_type;

    if(!iot_header_queue)
        return;

    for (const list_node_t *header_node = list_begin(iot_header_queue);
           header_node != list_end(iot_header_queue);
           header_node = list_next(header_node)) {
        iot_header_node_t *header_entry = list_node(header_node);
        method_type = header_entry->method_type;

        if(!header_entry->devlist)
            continue;

        ALOGW(" ########### Blacklisted Device summary ##############");
        for (const list_node_t *device_node = list_begin(header_entry->devlist);
                device_node != list_end(header_entry->devlist);
                device_node = list_next(device_node)) {
            if(method_type == METHOD_BD) {
                iot_devlist_bd_node_t *bd_addr_entry = list_node(device_node);
                ALOGW(" Device  %02X:%02X:%02X Blacklisted under %s",
                    bd_addr_entry->dev_bd[0], bd_addr_entry->dev_bd[1],
                    bd_addr_entry->dev_bd[2], header_entry->header_name);
            }
            else if(method_type == METHOD_NAME) {
                iot_devlist_name_node_t *bd_name_entry = list_node(device_node);
                ALOGW(" Device %s Blacklisted under %s", bd_name_entry->dev_name,
                    header_entry->header_name);
            }
        }
    }
}

/*****************************************************************************
**
** Function        load_iot_devlist_from_file
**
** Description     This function is used to initialize the queue and load the
**                 load the devices from file.
**
** Returns         void.
**
*******************************************************************************/
void load_iot_devlist_from_file(const char *filename)
{
    if (!filename) {
        ALOGE(" Invalid IOT blacklist filename");
        return;
    }
    char line_start[MAX_LINE];
    int line_number = 0;
    char *header_end = NULL;
    FILE *iot_devlist_fp = fopen(filename, "rt");
    if (iot_devlist_fp == NULL) {
        if(!strcmp(filename, IOT_DEV_CONF_FILE))  {
            //load it from system partition
            if(copy_file(IOT_DEV_BASE_CONF_FILE, IOT_DEV_CONF_FILE) == false) {
                ALOGE(" Can't copy it from Base file %s", IOT_DEV_BASE_CONF_FILE);
                return;
            }
            else {
                if((iot_devlist_fp = fopen(filename, "rt")) == NULL)
                    return;
            }
        }
        else {
            ALOGE(" File %s does not exist ",filename);
            return;
        }
    }
    while(fgets(line_start, MAX_LINE, iot_devlist_fp))  {
        line_number++;
        if((*line_start == '\n') ||(*line_start == '#')) {
            ALOGV("line %d is empty",line_number);
            continue;
        }
        header_end = strchr(line_start, '=');
        if (header_end == NULL) {
            ALOGV(" NOT A valid line %d", line_number);
            continue;
        }
        populate_header(line_start, header_end);
    }
    dump_all_iot_devices();
    fclose(iot_devlist_fp);
}

/*****************************************************************************
**
** Function        load_iot_devlist
**
** Description     This function is used to initialize the queue.
**
** Returns         void.
**
*******************************************************************************/
void load_iot_devlist(const char *filename)
{
    pthread_mutex_lock(&iot_mutex_lock);
    load_iot_devlist_from_file(filename);
    pthread_mutex_unlock(&iot_mutex_lock);
}

/*****************************************************************************
**
** Function        add_iot_device
**
** Description     This function is used to add the device to the blacklist file
**                 as well as queue.
**
** Returns         true incase the device is blacklisted otherwise fasle.
**
*******************************************************************************/
bool add_iot_device(const char *filename, char* header,
    unsigned char* device_details, tBLACKLIST_METHOD method_type)
{
    char line_start[MAX_LINE];
    FILE *iot_devlist_fp;
    char *header_end = NULL;
    int index = 0, i, len = 0;

    if((header == NULL) || (device_details == NULL)) {
        ALOGE("Error adding device to the list: Invalid input data");
        return false;
    }
    if (is_device_present (header , device_details)) {
        ALOGW("Device already present in the blacklist");
        return true;
    }

    pthread_mutex_lock(&iot_mutex_lock);
    iot_devlist_fp = fopen(filename, "a");

    if (iot_devlist_fp == NULL) {
        ALOGE(" File %s does not exist ", filename);
        pthread_mutex_unlock(&iot_mutex_lock);
        return false;
    }
    /* first copy the header */
    len = strlcpy(&line_start[index], header, strlen(header)+ 1);
    index += len;

    line_start[index++] = '=';
    /* then copy the device addr/device name */
    if(method_type == METHOD_BD) {
        /* for addr take first 3 bytes */
        for(i = 0; i < 3; i++) {
            if(i < 2) {
                len = snprintf(&line_start[index], MAX_LINE - index, "%02X:",
                                                    *(device_details + i));
            }
            else {
                len = snprintf(&line_start[index], MAX_LINE - index, "%02X",
                                            *(device_details + i));
            }
            index += len;
        }
    }
    else if(method_type == METHOD_NAME) {
        len = strlcpy(&line_start[index], (const char*) device_details,
                        strlen((const char*)device_details) + 1);
        index += len;
    }
    /* append the new line characer at the end */
    line_start[index++] = '\n';
    line_start[index++] = '\0';

    header_end = strchr(line_start,'=');
    if(header_end) {
        populate_header(line_start, header_end);
    }
    if(fputs(line_start, iot_devlist_fp)) {
        fclose(iot_devlist_fp);
        pthread_mutex_unlock(&iot_mutex_lock);
        return true;
    }
    else {
        fclose(iot_devlist_fp);
        pthread_mutex_unlock(&iot_mutex_lock);
        return false;
    }
}

/*****************************************************************************
**
** Function        form_bd_addr
**
** Description     Adds the colon after 2 bytes to form valid BD address to
**                 compare the entry prsent in file.
**
** Returns         void
**
*******************************************************************************/
static void form_bd_addr(char *addr, char *new_addr, int max_len)
{
    int i = 0, index = 0, len =0;
    /* for addr take first 3 bytes */
    for(i = 0; i < 3; i++) {
        if(i < 2) {
            len = snprintf(&new_addr[index], max_len - index, "%02X:",
                    *(addr + i));
        }
        else {
            len = snprintf(&new_addr[index], max_len - index, "%02X",
                    *(addr + i));
        }
        index += len;
    }
    new_addr[max_len - 1]= '\0';
}

/*****************************************************************************
**
** Function        remove_iot_device_from_queue
**
** Description     This function is used remove the entry from internal queue.
**
** Returns         true if the entry removed from queue else false.
**
*******************************************************************************/
bool remove_iot_device_from_queue(unsigned char* device_details, char* header,
        tBLACKLIST_METHOD method_type)
{
    if(!iot_header_queue)
        return false;
    for (const list_node_t *header_node = list_begin(iot_header_queue);
           header_node != list_end(iot_header_queue);
           header_node = list_next(header_node)) {
        iot_header_node_t *header_entry = list_node(header_node);

        if(!header_entry->devlist)
            continue;

        if((!strcmp(header, header_entry->header_name)) &&
             method_type == header_entry->method_type) {

            for (const list_node_t *device_node = list_begin(header_entry->devlist);
                    device_node != list_end(header_entry->devlist);
                    device_node = list_next(device_node)) {
                if(method_type == METHOD_BD) {
                    iot_devlist_bd_node_t *bd_addr_entry = list_node(device_node);
                    if(!memcmp(device_details, bd_addr_entry->dev_bd, 3)) {
                        list_remove(header_entry->devlist, bd_addr_entry);
                        return true;
                    }
                }
                else if(method_type == METHOD_NAME) {
                    iot_devlist_name_node_t *bd_name_entry = list_node(device_node);
                    if(!strcmp((char *)device_details, bd_name_entry->dev_name)) {
                        list_remove(header_entry->devlist, bd_name_entry);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/*****************************************************************************
**
** Function        edit_line
**
** Description     This function is used to remove the device entry from the
**                 inputted line buffer if the entry present.
**
** Returns         true if the entry removed from line else false.
**
*******************************************************************************/
static void edit_line(char *line_start, char *dev_info, int line_len)
{
    char *dev_ptr = strstr(line_start, dev_info);
    char *comma_ptr = NULL;
    int len_to_copy = 0;
    if(dev_ptr) {
        comma_ptr = strchr(dev_ptr, ',');
        if(comma_ptr) {
            len_to_copy = line_len - (comma_ptr - line_start + 1);
        }
        else {
            *(dev_ptr - 1) = '\n';
            *(dev_ptr) = '\0';
        }
    }
    if(len_to_copy) {
        memmove(dev_ptr, comma_ptr + 1, len_to_copy);
    }
}

/*****************************************************************************
**
** Function        is_single_entry_line
**
** Description     This function is used to check the line consists of single
**                 input line if the entry present.
**
** Returns         true if the single entry present else false.
**
*******************************************************************************/
static bool is_single_entry_line(char *line_start)
{
    char *comma_ptr = strchr(line_start, ',');
    // check the char next to ,
    if( !comma_ptr || (*(comma_ptr + 1) == '\n')) {
        return true;
    }
    else {
        return false;
    }
}

/*****************************************************************************
**
** Function        get_header_from_line
**
** Description     This function is used to get the header from line buffer.
**
** Returns         true if the header found else false.
**
*******************************************************************************/
bool get_header_from_line(char *line_start, char* header)
{
    int i = 0;
    if(!line_start || !header || !strchr(line_start, '=')) {
        return false;
    }
    while (line_start[i] != '=') {
        header[i] = line_start[i];
        i++;
    }
    header[i] = '\0';
    return true;
}

/*****************************************************************************
**
** Function        remove_iot_device
**
** Description     This function is used to remove the device from internal
**                 blacklisted queue as well as black list file.
**
** Returns         true if the device is removed else false.
**
*******************************************************************************/
bool remove_iot_device(const char *filename, char* header,
    unsigned char* device_details, tBLACKLIST_METHOD method_type)
{
    char line_start[MAX_LINE];
    FILE *iot_devlist_fp, *iot_devlist_new_fp;
    char bd_addr[MAX_ADDR_STR_LEN];
    char header_name[MAX_NAME_LEN] = { 0 };
    char *dev = NULL;
    int len = 0;

    if((header == NULL) || (device_details == NULL)) {
        ALOGE("Invalid input data to add the device");
        return false;
    }
    if (!is_device_present (header , device_details)) {
        ALOGW("Device doesn't exist in the list");
        return false;
    }
    pthread_mutex_lock(&iot_mutex_lock);
    iot_devlist_fp = fopen(filename, "rt");

    if (iot_devlist_fp == NULL) {
        ALOGE(" File %s does not exist ", filename);
        pthread_mutex_unlock(&iot_mutex_lock);
        return false;
    }
    iot_devlist_new_fp = fopen(IOT_DEV_CONF_BKP_FILE, "wt");

    if (iot_devlist_new_fp == NULL) {
        ALOGE(" Unable to create backup file %s", IOT_DEV_CONF_BKP_FILE);
        fclose(iot_devlist_fp);
        pthread_mutex_unlock(&iot_mutex_lock);
        return false;
    }

    /* then copy the device addr/device name */
    while (fgets(line_start, sizeof line_start, iot_devlist_fp)) {
        len = strlen(line_start);

        if (len) {
            get_header_from_line(line_start, header_name);
            if(method_type == METHOD_BD) {
                form_bd_addr((char*)device_details, bd_addr, MAX_ADDR_STR_LEN);
                dev = bd_addr;
            }
            else if(method_type == METHOD_NAME) {
                dev = (char *) device_details;
            }
            // copy as it is if the line consists comments
            if( (line_start[0] == '#') || (line_start[0] == '/') ||
                    (line_start[0] == ' ') || (line_start[0] == '\n')) {
                fputs(line_start, iot_devlist_new_fp);
            }
            else if((!strcmp(header_name, header)) && (strstr(line_start, dev))) {
                if(is_single_entry_line(line_start)) {
                    if(!remove_iot_device_from_queue(device_details, header, method_type)) {
                        // if unable to remove from queue put the same line as it is
                        fputs(line_start, iot_devlist_new_fp);
                    }
                    else
                        ALOGE(" Removed %s device from blacklist file %s", dev, IOT_DEV_CONF_FILE);
                }
                else {
                    // multi line entry
                    if(remove_iot_device_from_queue(device_details, header, method_type)) {
                        edit_line(line_start, dev, len + 1);
                        fputs(line_start, iot_devlist_new_fp);
                        ALOGE(" Removed %s device from blacklist file %s", dev, IOT_DEV_CONF_FILE);
                    }
                    else {
                        fputs(line_start, iot_devlist_new_fp);
                    }
                }
            }
            else {
                fputs(line_start, iot_devlist_new_fp);
            }
        }
    }

    fclose(iot_devlist_fp);
    fclose(iot_devlist_new_fp);
    remove(filename);
    rename(IOT_DEV_CONF_BKP_FILE, filename);
    pthread_mutex_unlock(&iot_mutex_lock);
    return true;
}

/*****************************************************************************
**
** Function        init_soc_type
**
** Description     Get Bluetooth SoC type from system setting and stores it
**                 in soc_type.
**
** Returns         void.
**
*******************************************************************************/
static void init_soc_type()
{
    int ret = 0;
    char bt_soc_type[PROPERTY_VALUE_MAX];

    ALOGI("init_soc_type");

    soc_type = BT_SOC_DEFAULT;
#if defined(ANDROID)
    ret = property_get("qcom.bluetooth.soc", bt_soc_type, NULL);
    if (ret != 0) {
        int i;
        ALOGI("qcom.bluetooth.soc set to %s\n", bt_soc_type);
        for ( i = BT_SOC_AR3K ; i < BT_SOC_RESERVED ; i++ )
        {
            char* soc_name = soc_type_entries[i].soc_name;
            if (!strcmp(bt_soc_type, soc_name)) {
                soc_type = soc_type_entries[i].soc_type;
                break;
            }
        }
    }
#elif defined(BT_SOC_TYPE_ROME)
    soc_type = BT_SOC_ROME;
#elif defined(BT_SOC_TYPE_CHEROKEE)
    soc_type = BT_SOC_CHEROKEE;
#endif
}

/*****************************************************************************
**
** Function        get_soc_type
**
** Description     This function is used to get the Bluetooth SoC type.
**
** Returns         bt_soc_type.
**
*******************************************************************************/
bt_soc_type get_soc_type()
{
    return soc_type;
}
