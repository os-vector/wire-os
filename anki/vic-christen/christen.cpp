/**
 * File: main.cpp
 *
 * Author: seichert inspired by Paul Aluri's example code
 * Created: 3/7/2018
 *
 * Description: Christen the robot with a name if it doesn't have one
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <fstream>
#include <regex>
#include <sys/stat.h>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

const std::string kRobotNamePropertyKey = "anki.robot.name";
const std::string kProductNamePropertyKey = "ro.anki.product.name";
const std::string kDefaultProductName = "Vector";
const std::string kVectorNamePattern = "Vector ([A-Z][0-9][A-Z][0-9])";
const std::string kTestNameFile = "/factory/name";
const std::string kProcCmdlineFile = "/proc/cmdline";
const uint32_t kVectorNameSize = 11;
const uint32_t kRtsHeaderVersion = 2;

#define PROPERTY_VALUE_MAX 92

const std::string kSwitchboardBlockDevice = "/dev/block/bootdevice/by-name/switchboard";
const size_t kSwitchboardDataBlockLen = 262144; // 256 * 1024 bytes -- (256kb)

static uint8_t sBlockBuffer[kSwitchboardDataBlockLen] = {0};

// The robot shall be named "<ProductName> XYXY" where
// X is a random letter and Y is a random digit.
// In some fonts, '0' and 'O' look alike, so they are excluded.
// In addition, we exclude 'I' and 'L' for the same reason.

static const std::vector<char> kLetters = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'J',
                                           'K', 'M', 'N', 'P', 'R', 'S', 'T', 'U', 'V',
                                           'W', 'X', 'Y', 'Z'
                                          };
static const std::vector<char> kDigits =  {'1', '2', '3', '4', '5', '6', '7', '8', '9'};

/* Reference: vic-switchboard definitions
 *  Structs describing the internal format of block device data that we care about
 *  for accessing the robot name (defined in vic-switchboard)
 *
 * #define crypto_kx_PUBLICKEYBYTES 32
 * #define crypto_kx_SECRETKEYBYTES 32
 *
 * struct __attribute__ ((packed)) RtsIdData {
 *   bool hasName;
 *   char name[12];
 *   uint8_t publicKey[crypto_kx_PUBLICKEYBYTES];
 *   uint8_t privateKey[crypto_kx_SECRETKEYBYTES];
 * };
 *
 *
 * struct __attribute__ ((packed)) RtsKeysData {
 *   uint8_t magic[8];
 *   uint32_t version;
 *   RtsIdData id;
 *   uint8_t numKnownClients;
 * };
 *
 */

int property_get(const char* key, char* value, const char* default_value) {
  char cmd[256] = {0};
  snprintf(cmd, sizeof(cmd), "getprop %s", key);

  FILE* fp = popen(cmd, "r");
  if (!fp) {
    if (default_value) {
      strncpy(value, default_value, PROPERTY_VALUE_MAX - 1);
      value[PROPERTY_VALUE_MAX - 1] = 0;
      return strlen(value);
    }
    value[0] = 0;
    return 0;
  }

  if (!fgets(value, PROPERTY_VALUE_MAX, fp)) {
    pclose(fp);
    if (default_value) {
      strncpy(value, default_value, PROPERTY_VALUE_MAX - 1);
      value[PROPERTY_VALUE_MAX - 1] = 0;
      return strlen(value);
    }
    value[0] = 0;
    return 0;
  }

  pclose(fp);

  size_t len = strlen(value);
  while (len > 0 && (value[len - 1] == '\n' || value[len - 1] == '\r')) {
    value[--len] = 0;
  }

  if (len == 0 && default_value) {
    strncpy(value, default_value, PROPERTY_VALUE_MAX - 1);
    value[PROPERTY_VALUE_MAX - 1] = 0;
    return strlen(value);
  }

  return len;
}

int property_set(const char* key, const char* value) {
  if (!key || !value) return -1;

  char cmd[512] = {0};
  snprintf(cmd, sizeof(cmd), "setprop %s \"%s\"", key, value);

  int rc = system(cmd);
  if (rc != 0) return -1;

  return 0;
}

// Based on the reference structs, this header format can never change
// Note that due to a bug in the factory version of vic-switchboard, the value of the
// version field must always be 2.
struct __attribute__((packed)) RtsHeader {
  uint8_t magic[8];
  uint32_t version;
  bool hasName;
  char name[12];
};

/// Read "n" bytes from a descriptor.
/// adapted from Stevens, "Unix Network Programming"
ssize_t readn(int fd, void *vptr, size_t n)
{
  size_t  nleft;
  ssize_t nread;
  char   *ptr;

  ptr = (char*)vptr;
  nleft = n;
  while (nleft > 0) {
    nread = read(fd, ptr, nleft);
    if (nread  < 0) {
      if (errno == EINTR) {
        nread = 0;      /* and call read() again */
      } else {
        return (-1);
      }
    } else if (nread == 0) {
      break;              /* EOF */
    }

    nleft -= nread;
    ptr += nread;
  }
  return (n - nleft);         /* return >= 0 */
}

/// Write "n" bytes to a descriptor
/// adapted from Stevens, "Unix Network Programming"
ssize_t
writen(int fd, const void *vptr, size_t n)
{
  size_t nleft;
  ssize_t nwritten;
  const char *ptr;

  ptr = (char*)vptr;
  nleft = n;
  while (nleft > 0) {
    nwritten = write(fd, ptr, nleft);
    if (nwritten <= 0) {
      if (nwritten < 0 && errno == EINTR) {
        nwritten = 0;   /* and call write() again */
      } else {
        return (-1);    /* error */
      }
    }

    nleft -= nwritten;
    ptr += nwritten;
  }
  return (n);
}

bool AnkiUtil_FileExists(const std::string fileName)
{
  struct stat info;
  bool exists = false;
  if( stat(fileName.c_str(), &info)==0 ) {
    if( info.st_mode & S_IFREG ) {
      exists = true;
    }
  }
  return exists;
}

bool OSState_IsAnkiDevRobot()
{
  static bool _isDevRobot = false;
  if(_isDevRobot) {
    return true;
  }

  static bool read = false;
  if(!read)
  {
    read = true;
    std::ifstream infile(kProcCmdlineFile);
    std::string line;
    while(std::getline(infile, line))
    {
      static const char* kKey = "anki.dev";
      size_t index = line.find(kKey);
      if(index != std::string::npos)
      {
        _isDevRobot = true;
        break;
      }
    }
    infile.close();
  }

  return _isDevRobot;
}

static bool IsRobotNameValid(const std::string& robotName, const std::string& productName)
{
  if (robotName.length() < kVectorNameSize) {
    return false;
  }

  std::string productNamePlusSpace = productName + " ";
  if (robotName.find(productNamePlusSpace) != 0) {
    return false;
  }

  std::string::size_type offset = productNamePlusSpace.length();

  for (int i = 0 ; i < 2 ; i++) {
    if (std::end(kLetters) == std::find(std::begin(kLetters), std::end(kLetters), robotName[offset + (2 * i)])) {
      return false;
    }
    if (std::end(kDigits) == std::find(std::begin(kDigits), std::end(kDigits), robotName[offset + (2 * i) + 1])) {
      return false;
    }
  }

  return true;
}

/// Check sBlockBuffer for valid switchboard magic bytes.
/// @return true if data in sBlockBuffer is valid switchboard data, otherwise return false
static bool IsSwitchboardDataValid(const std::string& productName)
{
  // check magic
  struct RtsHeader *header = reinterpret_cast<struct RtsHeader*>(sBlockBuffer);
  uint8_t* m = header->magic;

  if (m[0] == 'A' && m[1] == 'N' && m[2] == 'K' && m[3] == 'I' &&
      m[4] == 'B' && m[5] == 'I' && m[6] == 'T' && m[7] == 'S' &&
      header->version == kRtsHeaderVersion) {

    if (header->hasName) {
      std::string robotName = std::string(header->name, 0, sizeof(header->name) - 1);
      return IsRobotNameValid(robotName, productName);
    }
    return true;
  } else {
    // invalid data (not written by an Anki process)
    return false;
  }
}

/// Read data from switchboard partition info sBlockBuffer.
/// @return 0 if data was read successfully or -1 on failure.
/// errno may be set on failure.
static int ReadSwitchboardData(const std::string& productName)
{
  int fd = open(kSwitchboardBlockDevice.c_str(), O_RDONLY);
  if (fd == -1) {
    return -1;
  }

  ssize_t bytesRead = readn(fd, sBlockBuffer, sizeof(sBlockBuffer));

  int r = close(fd);
  if ((bytesRead <= 0) || (r == -1)) {
    return -1;
  }

  // Switchboard stores formatted binary data in the block partition, but we
  // only care about the name. Just check that we've read enough data and
  // that the magic bytes are present.

  if (bytesRead < sizeof(struct RtsHeader)) {
    return -1;
  }

  return IsSwitchboardDataValid(productName) ? 0 : -1;
}

/// Write switchboard data to disk with the specified robit name
/// @return 0 on success, -1 on failure
static int WriteSwitchboardData(const std::string robotName, const std::string productName)
{
  int fd = open(kSwitchboardBlockDevice.c_str(), O_WRONLY);
  if (fd == -1) {
    return -1;
  }

  struct RtsHeader* header = reinterpret_cast<struct RtsHeader*>(sBlockBuffer);

  if (!IsSwitchboardDataValid(productName)) {
    // Fill out enough valid data to write name
    header->magic[0] = 'A';
    header->magic[1] = 'N';
    header->magic[2] = 'K';
    header->magic[3] = 'I';
    header->magic[4] = 'B';
    header->magic[5] = 'I';
    header->magic[6] = 'T';
    header->magic[7] = 'S';
    header->version = kRtsHeaderVersion; // Always set format version = 2 (factory compatability)
  }
  
  header->hasName = true;
  (void) memset(header->name, 0, sizeof(header->name));
  (void) robotName.copy(header->name, sizeof(header->name) - 1);

  ssize_t bytesWritten = writen(fd, sBlockBuffer, sizeof(sBlockBuffer));

  int r = close(fd);
  if ((bytesWritten <= 0) || (r == -1)) {
    return -1;
  }

  return 0;
}

/// GetRobotName from sBlockBuffer
static std::string GetRobotName()
{
  struct RtsHeader* header = reinterpret_cast<struct RtsHeader*>(sBlockBuffer);
  if (!header->hasName) {
    return "";
  } else {
    return std::string(header->name, 0, sizeof(header->name) - 1);
  }
}

/// Create a random robot name of the form "<ProductName> XYXY"
/// @return generated robot name
static std::string GenerateRobotName(const std::string& product_name)
{
  std::random_device rd;
  std::mt19937 gen(rd());

  // 198 = 22 * 9.  This should allow for a uniform distribution of values
  // when we select from either the kLetters or kDigits vectors.
  std::uniform_int_distribution<> dis(1, 198);

  std::string robot_id;
  for (int i = 0 ; i < 2 ; i++) {
    char letter = kLetters[dis(gen) % kLetters.size()];
    robot_id.push_back(letter);
    char digit = kDigits[dis(gen) % kDigits.size()];
    robot_id.push_back(digit);
  }

  std::string robot_name = product_name + " " + robot_id;
  return robot_name;
}

static std::string GetNameFromAutoTestFile() {
  char nameBuffer[kVectorNameSize] = {0};

  int fd = open(kTestNameFile.c_str(), O_RDONLY);
  if (fd == -1) {
    return "";
  }

  ssize_t bytesRead = readn(fd, nameBuffer, sizeof(nameBuffer));

  close(fd);

  if (bytesRead < sizeof(nameBuffer)) {
    return "";
  }

  std::string nameStr = std::string(nameBuffer, sizeof(nameBuffer));

  if(!std::regex_match(nameStr, std::regex(kVectorNamePattern))) {
    return "";
  }

  return nameStr;
}

static bool DoesTestNameFileExist()
{
  return AnkiUtil_FileExists(kTestNameFile);
}

static bool IsAutoTestBot()
{
  return DoesTestNameFileExist() && OSState_IsAnkiDevRobot();
}

int main(int argc, char *argv[])
{
  std::string productName;
  {
    char prop_value[PROPERTY_VALUE_MAX] = {0};
    (void) property_get(kProductNamePropertyKey.c_str(), prop_value, kDefaultProductName.c_str());
    if (prop_value[0] == '\0') {
      // Fatal Error.  No product name.
      std::cerr << "Fatal: No product name found" << std::endl;
      return 1;
    }
    productName = std::string(prop_value);
  }

  std::string robotName;
  if(IsAutoTestBot()) {
    std::string autoTestName = GetNameFromAutoTestFile();
    if (!autoTestName.empty()) {
      robotName = std::move(autoTestName);
    }
  }

  int rc;
  if(robotName.empty()) {
    rc = ReadSwitchboardData(productName);
    if (rc == 0) {
      std::string existingRobotName = GetRobotName();
      if (!existingRobotName.empty()) {
        robotName = std::move(existingRobotName);
      }
    }
  }

  if (robotName.empty()) {
    robotName = GenerateRobotName(productName);

    // update switchboard data
    rc = WriteSwitchboardData(robotName, productName);
    if (rc == -1) {
      std::cerr << "Fatal: Failed to save robot name" << std::endl;
      return 1;
    }
  }

  // set property
  rc = property_set(kRobotNamePropertyKey.c_str(), robotName.c_str());
  if (rc) {
    std::cerr << "Fatal: Failed to set robot's name in property" << std::endl;
    return 2;
  }

  std::cout << "Robot has been christened \"" << robotName << "\"" << std::endl;
  return 0;
}
