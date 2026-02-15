#include <gtest/gtest.h>

#include "AllocationTestHarness.h"

extern "C" {
#include "osi/include/config.h"
}

static const char CONFIG_FILE[] = "/data/local/tmp/config_test.conf";
static const char CONFIG_FILE_CONTENT[] =
"                                                                                    \n\
first_key=value                                                                      \n\
                                                                                     \n\
# Device ID (DID) configuration                                                      \n\
[DID]                                                                                \n\
                                                                                     \n\
# Record Number: 1, 2 or 3 - maximum of 3 records                                    \n\
recordNumber = 1                                                                     \n\
                                                                                     \n\
# Primary Record - true or false (default)                                           \n\
# There can be only one primary record                                               \n\
primaryRecord = true                                                                 \n\
                                                                                     \n\
# Vendor ID '0xFFFF' indicates no Device ID Service Record is present in the device  \n\
# 0x000F = Broadcom Corporation (default)                                            \n\
#vendorId = 0x000F                                                                   \n\
                                                                                     \n\
# Vendor ID Source                                                                   \n\
# 0x0001 = Bluetooth SIG assigned Device ID Vendor ID value (default)                \n\
# 0x0002 = USB Implementer's Forum assigned Device ID Vendor ID value                \n\
#vendorIdSource = 0x0001                                                             \n\
                                                                                     \n\
# Product ID & Product Version                                                       \n\
# Per spec DID v1.3 0xJJMN for version is interpreted as JJ.M.N                      \n\
# JJ: major version number, M: minor version number, N: sub-minor version number     \n\
# For example: 1200, v14.3.6                                                         \n\
productId = 0x1200                                                                   \n\
version = 0x1111                                                                     \n\
                                                                                     \n\
# Optional attributes                                                                \n\
#clientExecutableURL =                                                               \n\
#serviceDescription =                                                                \n\
#documentationURL =                                                                  \n\
                                                                                     \n\
# Additional optional DID records. Bluedroid supports up to 3 records.               \n\
[DID]                                                                                \n\
[DID]                                                                                \n\
version = 0x1436                                                                     \n\
";

class ConfigTest : public AllocationTestHarness {
  protected:
    virtual void SetUp() {
      AllocationTestHarness::SetUp();
      FILE *fp = fopen(CONFIG_FILE, "wt");
      fwrite(CONFIG_FILE_CONTENT, 1, sizeof(CONFIG_FILE_CONTENT), fp);
      fclose(fp);
    }
};

TEST_F(ConfigTest, config_new_empty) {
  config_t *config = config_new_empty();
  EXPECT_TRUE(config != NULL);
  config_free(config);
}

TEST_F(ConfigTest, config_new_no_file) {
  config_t *config = config_new("/meow");
  EXPECT_TRUE(config == NULL);
  config_free(config);
}

TEST_F(ConfigTest, config_new) {
  config_t *config = config_new(CONFIG_FILE);
  EXPECT_TRUE(config != NULL);
  config_free(config);
}

TEST_F(ConfigTest, config_free_null) {
  config_free(NULL);
}

TEST_F(ConfigTest, config_new_clone) {
  config_t *config = config_new(CONFIG_FILE);
  config_t *clone = config_new_clone(config);

  config_set_string(clone, CONFIG_DEFAULT_SECTION, "first_key", "not_value");

  EXPECT_STRNE(config_get_string(config, CONFIG_DEFAULT_SECTION, "first_key", "one"),
               config_get_string(clone, CONFIG_DEFAULT_SECTION, "first_key", "one"));

  config_free(config);
  config_free(clone);
}

TEST_F(ConfigTest, config_has_section) {
  config_t *config = config_new(CONFIG_FILE);
  EXPECT_TRUE(config_has_section(config, "DID"));
  config_free(config);
}

TEST_F(ConfigTest, config_has_key_in_default_section) {
  config_t *config = config_new(CONFIG_FILE);
  EXPECT_TRUE(config_has_key(config, CONFIG_DEFAULT_SECTION, "first_key"));
  EXPECT_STREQ(config_get_string(config, CONFIG_DEFAULT_SECTION, "first_key", "meow"), "value");
  config_free(config);
}

TEST_F(ConfigTest, config_has_keys) {
  config_t *config = config_new(CONFIG_FILE);
  EXPECT_TRUE(config_has_key(config, "DID", "recordNumber"));
  EXPECT_TRUE(config_has_key(config, "DID", "primaryRecord"));
  EXPECT_TRUE(config_has_key(config, "DID", "productId"));
  EXPECT_TRUE(config_has_key(config, "DID", "version"));
  config_free(config);
}

TEST_F(ConfigTest, config_no_bad_keys) {
  config_t *config = config_new(CONFIG_FILE);
  EXPECT_FALSE(config_has_key(config, "DID_BAD", "primaryRecord"));
  EXPECT_FALSE(config_has_key(config, "DID", "primaryRecord_BAD"));
  EXPECT_FALSE(config_has_key(config, CONFIG_DEFAULT_SECTION, "primaryRecord"));
  config_free(config);
}

TEST_F(ConfigTest, config_get_int_version) {
  config_t *config = config_new(CONFIG_FILE);
  EXPECT_EQ(config_get_int(config, "DID", "version", 0), 0x1436);
  config_free(config);
}

TEST_F(ConfigTest, config_get_int_default) {
  config_t *config = config_new(CONFIG_FILE);
  EXPECT_EQ(config_get_int(config, "DID", "primaryRecord", 123), 123);
  config_free(config);
}

TEST_F(ConfigTest, config_remove_section) {
  config_t *config = config_new(CONFIG_FILE);
  EXPECT_TRUE(config_remove_section(config, "DID"));
  EXPECT_FALSE(config_has_section(config, "DID"));
  EXPECT_FALSE(config_has_key(config, "DID", "productId"));
  config_free(config);
}

TEST_F(ConfigTest, config_remove_section_missing) {
  config_t *config = config_new(CONFIG_FILE);
  EXPECT_FALSE(config_remove_section(config, "not a section"));
  config_free(config);
}

TEST_F(ConfigTest, config_remove_key) {
  config_t *config = config_new(CONFIG_FILE);
  EXPECT_EQ(config_get_int(config, "DID", "productId", 999), 0x1200);
  EXPECT_TRUE(config_remove_key(config, "DID", "productId"));
  EXPECT_FALSE(config_has_key(config, "DID", "productId"));
  config_free(config);
}

TEST_F(ConfigTest, config_remove_key_missing) {
  config_t *config = config_new(CONFIG_FILE);
  EXPECT_EQ(config_get_int(config, "DID", "productId", 999), 0x1200);
  EXPECT_TRUE(config_remove_key(config, "DID", "productId"));
  EXPECT_EQ(config_get_int(config, "DID", "productId", 999), 999);
  config_free(config);
}

TEST_F(ConfigTest, config_section_begin) {
  config_t *config = config_new(CONFIG_FILE);
  const config_section_node_t *section = config_section_begin(config);
  EXPECT_TRUE(section != NULL);
  const char *section_name = config_section_name(section);
  EXPECT_TRUE(section != NULL);
  EXPECT_TRUE(!strcmp(section_name, CONFIG_DEFAULT_SECTION));
  config_free(config);
}

TEST_F(ConfigTest, config_section_next) {
  config_t *config = config_new(CONFIG_FILE);
  const config_section_node_t *section = config_section_begin(config);
  EXPECT_TRUE(section != NULL);
  section = config_section_next(section);
  EXPECT_TRUE(section != NULL);
  const char *section_name = config_section_name(section);
  EXPECT_TRUE(section != NULL);
  EXPECT_TRUE(!strcmp(section_name, "DID"));
  config_free(config);
}

TEST_F(ConfigTest, config_section_end) {
  config_t *config = config_new(CONFIG_FILE);
  const config_section_node_t * section = config_section_begin(config);
  section = config_section_next(section);
  section = config_section_next(section);
  EXPECT_EQ(section, config_section_end(config));
  config_free(config);
}

TEST_F(ConfigTest, config_save_basic) {
  config_t *config = config_new(CONFIG_FILE);
  EXPECT_TRUE(config_save(config, CONFIG_FILE));
  config_free(config);
}
