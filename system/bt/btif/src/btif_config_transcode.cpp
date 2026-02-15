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

#define LOG_TAG "bt_btif_config_transcode"

#include <tinyxml2.h>

extern "C" {
#include "osi/include/config.h"
#include "osi/include/log.h"
}

using namespace tinyxml2;

extern "C" config_t *btif_config_transcode(const char *xml_filename) {
  XMLDocument document;
  int error = document.LoadFile(xml_filename);
  if (error != XML_SUCCESS) {
    LOG_ERROR("%s unable to load XML file '%s': %d", __func__, xml_filename, error);
    return NULL;
  }

  XMLElement *rootElement = document.RootElement();
  if (!rootElement) {
    LOG_ERROR("%s unable to find root element; assuming corrupted config file.", __func__);
    return NULL;
  }

  config_t *config = config_new_empty();
  if (!config) {
    LOG_ERROR("%s unable to allocate config object.", __func__);
    return NULL;
  }

  for (XMLElement *i = rootElement->FirstChildElement(); i != NULL; i = i->NextSiblingElement())
    for (XMLElement *j = i->FirstChildElement(); j != NULL; j = j->NextSiblingElement()) {
      const char *section = j->Attribute("Tag");
      for (XMLElement *k = j->FirstChildElement(); k != NULL; k = k->NextSiblingElement()) {
        const char *key = k->Attribute("Tag");
        const char *value = k->GetText();
        if (section && key && value)
          config_set_string(config, section, key, value);
      }
    }

  return config;
}
