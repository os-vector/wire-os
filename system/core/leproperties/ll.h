/******************************************************************************
 *
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef LE_PROP_LL_H
#define LE_PROP_LL_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include "property_service.h"

/**
 * Check if the list is empty
 * @param None
 * @return bool true on empty else false
 */
bool __list_is_empty();

/**
 * Run through the list and match the prop by name
 * @param char* string for search property name
 * @return Pointer to the node type property_db
 */
property_db* __list_matches_prop_name(const char* search_name);

/**
 * Create a list if nothing inside, else add to the existing list
 * @param char* string for search property name
 * @param char* string for inserting the property value
 * @return True for success and False otherwise
 */
bool __create_list_and_add( const char* search_name, const char* property_value);

/**
 * Update the value of the property with a new one
 * @param char* string for search property name
 * @param char* string for inserting the property value
 * @return True for success and False otherwise
 */
bool __update_prop_value(const char* search_name, const char* property_value);

/**
 * Retrive the value of the property with specifed name
 * @param char* string for search property name
 * @param char* string for fetching the property value
 * @return True for success and False otherwise
 */
bool __retrive_prop_value(const char* search_name, const char* property_value);

/**
 * Add the element into the list
 * @param Node pointer for the list node
 * @return True for success and False otherwise
 */
bool __list_add(property_db* list);

/**
 * Remove a node with matching property name
 * @param char* string for search property name
 * @return True for successful removal and False otherwise
 */
bool __remove_node_from_list(unsigned char* property_name);

/**
 * Free up the complete list to be called upon deinit of the service
 * @param Null
 * @return True for success and False otherwise
 */
bool __free_list();

/**
 * Return the head node of this list
 * @param None
 * @return Node pointer for the head of list
 */
property_db* __get_list_head();

/**
 * Dump the created nodes of the list (current snapshot)
 * @param None
 * @return Void - dump will be created on standard output
 */
void __dump_nodes();

#endif //#define LE_PROP_LL_H
