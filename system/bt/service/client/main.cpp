//
//  Copyright (C) 2015 Google, Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at:
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#include <iostream>
#include <iomanip>
#include <string>

#ifdef BT_LIBCHROME_NDEBUG
#define NDEBUG 1
#endif

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>

#include <bluetooth/adapter_state.h>
#include <bluetooth/binder/IBluetooth.h>
#include <bluetooth/binder/IBluetoothCallback.h>
#include <bluetooth/binder/IBluetoothGattClient.h>
#include <bluetooth/binder/IBluetoothGattClientCallback.h>
#include <bluetooth/binder/IBluetoothLowEnergy.h>
#include <bluetooth/binder/IBluetoothLowEnergyCallback.h>
#include <bluetooth/low_energy_constants.h>
#include <bluetooth/scan_filter.h>
#include <bluetooth/scan_settings.h>
#include <bluetooth/uuid.h>

using namespace std;

using android::sp;

using ipc::binder::IBluetooth;
using ipc::binder::IBluetoothGattClient;
using ipc::binder::IBluetoothLowEnergy;

namespace {

#define COLOR_OFF         "\x1B[0m"
#define COLOR_RED         "\x1B[0;91m"
#define COLOR_GREEN       "\x1B[0;92m"
#define COLOR_YELLOW      "\x1B[0;93m"
#define COLOR_BLUE        "\x1B[0;94m"
#define COLOR_MAGENTA     "\x1B[0;95m"
#define COLOR_BOLDGRAY    "\x1B[1;30m"
#define COLOR_BOLDWHITE   "\x1B[1;37m"
#define COLOR_BOLDYELLOW  "\x1B[1;93m"
#define CLEAR_LINE        "\x1B[2K"

#define CHECK_ARGS_COUNT(args, op, num, msg) \
  if (!(args.size() op num)) { \
    PrintError(msg); \
    return; \
  }
#define CHECK_NO_ARGS(args) \
  CHECK_ARGS_COUNT(args, ==, 0, "Expected no arguments")

// TODO(armansito): Clean up this code. Right now everything is in this
// monolithic file. We should organize this into different classes for command
// handling, console output/printing, callback handling, etc.
// (See http://b/23387611)

// Used to synchronize the printing of the command-line prompt and incoming
// Binder callbacks.
std::atomic_bool showing_prompt(false);

// The registered IBluetoothLowEnergy client handle. If |ble_registering| is
// true then an operation to register the client is in progress.
std::atomic_bool ble_registering(false);
std::atomic_int ble_client_id(0);

// The registered IBluetoothGattClient client handle. If |gatt_registering| is
// true then an operation to register the client is in progress.
std::atomic_bool gatt_registering(false);
std::atomic_int gatt_client_id(0);

// True if we should dump the scan record bytes for incoming scan results.
std::atomic_bool dump_scan_record(false);

// True if the remote process has died and we should exit.
std::atomic_bool should_exit(false);

void PrintPrompt() {
  cout << COLOR_BLUE "[FCLI] " COLOR_OFF << flush;
}

void PrintError(const string& message) {
  cout << COLOR_RED << message << COLOR_OFF << endl;
}

void PrintOpStatus(const std::string& op, bool status) {
  cout << COLOR_BOLDWHITE << op << " status: " COLOR_OFF
       << (status ? (COLOR_GREEN "success") : (COLOR_RED "failure"))
       << COLOR_OFF << endl;
}

inline void BeginAsyncOut() {
  if (showing_prompt.load())
    cout << CLEAR_LINE << "\r";
}

inline void EndAsyncOut() {
  std::flush(cout);
  if (showing_prompt.load())
      PrintPrompt();
  else
    cout << endl;
}

class CLIBluetoothCallback : public ipc::binder::BnBluetoothCallback {
 public:
  CLIBluetoothCallback() = default;
  ~CLIBluetoothCallback() override = default;

  // IBluetoothCallback overrides:
  void OnBluetoothStateChange(
      bluetooth::AdapterState prev_state,
      bluetooth::AdapterState new_state) override {

    BeginAsyncOut();
    cout << COLOR_BOLDWHITE "Adapter state changed: " COLOR_OFF
         << COLOR_MAGENTA << AdapterStateToString(prev_state) << COLOR_OFF
         << COLOR_BOLDWHITE " -> " COLOR_OFF
         << COLOR_BOLDYELLOW << AdapterStateToString(new_state) << COLOR_OFF << endl;
    EndAsyncOut();
   }

 private:
  DISALLOW_COPY_AND_ASSIGN(CLIBluetoothCallback);
};

class CLIBluetoothLowEnergyCallback
    : public ipc::binder::BnBluetoothLowEnergyCallback {
 public:
  CLIBluetoothLowEnergyCallback() = default;
  ~CLIBluetoothLowEnergyCallback() override = default;

  // IBluetoothLowEnergyCallback overrides:
  void OnClientRegistered(int status, int client_id) override {
    BeginAsyncOut();
    if (status != bluetooth::BLE_STATUS_SUCCESS) {
      PrintError("Failed to register BLE client");
    } else {
      ble_client_id = client_id;
      cout << COLOR_BOLDWHITE "Registered BLE client with ID: " COLOR_OFF
           << COLOR_GREEN << client_id << COLOR_OFF << endl;
    }
    EndAsyncOut();

    ble_registering = false;
  }

  void OnConnectionState(int status, int client_id, const char* address,
                         bool connected) override {
    BeginAsyncOut();
    cout << COLOR_BOLDWHITE "Connection state: "
         << COLOR_BOLDYELLOW "[" << address
         << " connected: " << (connected ? "true" : "false") << " ] "
         << COLOR_BOLDWHITE "- status: " << status
         << COLOR_BOLDWHITE " - client_id: " << client_id << COLOR_OFF << endl;
    EndAsyncOut();
  }

  void OnMtuChanged(int status, const char *address, int mtu) override {
    BeginAsyncOut();
    cout << COLOR_BOLDWHITE "MTU changed: "
         << COLOR_BOLDYELLOW "[" << address << " ] "
         << COLOR_BOLDWHITE " - status: " << status
         << COLOR_BOLDWHITE " - mtu: " << mtu << COLOR_OFF << endl;
    EndAsyncOut();
  }

  void OnServicesDiscovered(int status, const char *address) override {
    BeginAsyncOut();
    cout << COLOR_BOLDWHITE "Services Discovered: "
	 << COLOR_BOLDYELLOW "[" << address << " ] "
	 << COLOR_BOLDWHITE " - status: " << status << COLOR_OFF << endl;
    EndAsyncOut();
  }

  void uuid_to_string(bt_uuid_t *p_uuid, std::string& str) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 15 ; i >= 0; i--) {
      ss << std::setw(2) << static_cast<int>(p_uuid->uu[i]);
      if (i == 12 || i == 10 || i == 8 || i == 6) {
        ss << "-";
      }
    }
    str = ss.str();
  }

  void bt_gatt_db_attribute_type_t_to_string(bt_gatt_db_attribute_type_t t, std::string& type) {
    switch(t) {
      case BTGATT_DB_PRIMARY_SERVICE:
        type = "Primary Service";
        break;
      case BTGATT_DB_SECONDARY_SERVICE:
        type = "Secondary Service";
        break;
      case BTGATT_DB_INCLUDED_SERVICE:
        type = "Included Service";
        break;
      case BTGATT_DB_CHARACTERISTIC:
        type = "Characteristic";
        break;
      case BTGATT_DB_DESCRIPTOR:
        type = "Descriptor";
        break;
      default:
        type = "Unknown (" + std::to_string(t) + ")";;
        break;
    }
  }

  void gatt_properties_to_string(uint8_t btaCharProperties, std::string& properties) {
    const std::map<uint8_t, std::string> props {
      { 0x01, "broadcast"},
      { 0x02, "read"},
      { 0x04, "write_no_response"},
      { 0x08, "write"},
      { 0x10, "notify"},
      { 0x20, "indicate"},
      { 0x40, "auth"},
      { 0x80, "ext_prop"},
    };
    std::ostringstream oss;
    for (auto p : props) {
      if (btaCharProperties & p.first) {
        if (oss.tellp()) {
          oss << "|";
        }
        oss << p.second;
      }
    }
    properties = oss.str();
    
  }

  void OnGattDbUpdated(const char *address, btgatt_db_element_t* db, int size) override {
    BeginAsyncOut();
    cout << COLOR_BOLDWHITE "Gatt Db Updated: "
	 << COLOR_BOLDYELLOW "[" << address << " ] "
	 << COLOR_BOLDWHITE " - size: " << size << endl;
    for (int i = 0 ; i < size ; i++) {
      std::string uuid;
      uuid_to_string(&(db[i].uuid), uuid);
      std::string type;
      bt_gatt_db_attribute_type_t_to_string(db[i].type, type);
      cout << COLOR_BOLDYELLOW " - id: " << db[i].id
           << ", uuid: " << uuid
           << ", type: " << type;
      if (db[i].attribute_handle) {
        cout << ", attribute_handle: " << db[i].attribute_handle;
      }
      if (db[i].start_handle) {
        cout  << ", start_handle: " << db[i].start_handle;
      }
      if (db[i].end_handle) {
        cout << ", end_handle: " << db[i].end_handle;
      }
      if (db[i].properties) {
        std::string charProperties;
        gatt_properties_to_string(db[i].properties, charProperties);
        cout << ", properties: " << charProperties;
      }
      cout << endl;
    }
    cout << COLOR_OFF;
    EndAsyncOut();
    free(db);
  }

  void OnCharacteristicRead(const char* address, int status, btgatt_read_params_t* p_data)
  {
    BeginAsyncOut();
    cout << COLOR_BOLDWHITE "Characteristic Read Result: "
         << COLOR_BOLDYELLOW "[" << address << " ] "
         << COLOR_BOLDWHITE " - status: " << status << endl;
    if (status == BT_STATUS_SUCCESS) {
      cout << COLOR_BOLDWHITE "Value: "
           << COLOR_BOLDYELLOW << base::HexEncode(p_data->value.value, p_data->value.len)
           << endl;
    }
    cout << COLOR_OFF;
    EndAsyncOut();
    free(p_data);
  }

  void OnCharacteristicWrite(const char* address, int status, uint16_t handle)
  {
    BeginAsyncOut();
    cout << COLOR_BOLDWHITE "Characteristic Write Result: "
         << COLOR_BOLDYELLOW "[" << address << " ] "
         << COLOR_BOLDWHITE " - status: " << status
         << COLOR_BOLDWHITE " - handle: " << handle << COLOR_OFF << endl;
    EndAsyncOut();
  }

  void OnDescriptorWrite(const char* address, int status, uint16_t handle)
  {
    BeginAsyncOut();
    cout << COLOR_BOLDWHITE "Descriptor Write Result: "
         << COLOR_BOLDYELLOW "[" << address << " ] "
         << COLOR_BOLDWHITE " - status: " << status
         << COLOR_BOLDWHITE " - handle: " << handle << COLOR_OFF << endl;
    EndAsyncOut();
  }

  void OnCharacteristicNotificationRegistration(const char* address, int registered,
                                                int status, uint16_t handle)
  {
    BeginAsyncOut();
    cout << COLOR_BOLDWHITE "Characteristic Notification Registration Result: "
         << COLOR_BOLDYELLOW "[" << address << " ] "
         << COLOR_BOLDWHITE " - registered: " << registered
         << COLOR_BOLDWHITE " - status: " << status
         << COLOR_BOLDWHITE " - handle: " << handle << COLOR_OFF << endl;
    EndAsyncOut();
  }

  void OnCharacteristicChanged(const char* address,
                               btgatt_notify_params_t *p_data)
  {
    BeginAsyncOut();
    cout << COLOR_BOLDWHITE "Characteristic Notification: "
         << COLOR_BOLDYELLOW "[" << address << " ] "
         << " - handle: " << p_data->handle
         << " - value: " << base::HexEncode(p_data->value, p_data->len)
         << COLOR_OFF << endl;
    EndAsyncOut();
    free(p_data);
  }


  void OnScanResult(const bluetooth::ScanResult& scan_result) override {
    BeginAsyncOut();
    cout << COLOR_BOLDWHITE "Scan result: "
         << COLOR_BOLDYELLOW "[" << scan_result.device_address() << "] "
         << COLOR_BOLDWHITE "- RSSI: " << scan_result.rssi() << COLOR_OFF;

    if (dump_scan_record) {
      cout << " - Record: "
           << base::HexEncode(scan_result.scan_record().data(),
                              scan_result.scan_record().size());
    }
    EndAsyncOut();
  }

  void OnMultiAdvertiseCallback(
      int status, bool is_start,
      const bluetooth::AdvertiseSettings& /* settings */) {
    BeginAsyncOut();
    std::string op = is_start ? "start" : "stop";

    PrintOpStatus("Advertising " + op, status == bluetooth::BLE_STATUS_SUCCESS);
    EndAsyncOut();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CLIBluetoothLowEnergyCallback);
};

class CLIGattClientCallback
    : public ipc::binder::BnBluetoothGattClientCallback {
 public:
  CLIGattClientCallback() = default;
  ~CLIGattClientCallback() override = default;

  // IBluetoothGattClientCallback overrides:
  void OnClientRegistered(int status, int client_id) override {
    BeginAsyncOut();
    if (status != bluetooth::BLE_STATUS_SUCCESS) {
      PrintError("Failed to register GATT client");
    } else {
      gatt_client_id = client_id;
      cout << COLOR_BOLDWHITE "Registered GATT client with ID: " COLOR_OFF
           << COLOR_GREEN << client_id << COLOR_OFF << endl;
    }
    EndAsyncOut();

    gatt_registering = false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CLIGattClientCallback);
};

void PrintCommandStatus(bool status) {
  PrintOpStatus("Command", status);
}

void PrintFieldAndValue(const string& field, const string& value) {
  cout << COLOR_BOLDWHITE << field << ": " << COLOR_BOLDYELLOW << value
       << COLOR_OFF << endl;
}

void PrintFieldAndBoolValue(const string& field, bool value) {
  PrintFieldAndValue(field, (value ? "true" : "false"));
}

void HandleDisable(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);
  PrintCommandStatus(bt_iface->Disable());
}

void HandleEnable(IBluetooth* bt_iface, const vector<string>& args) {
  bool is_restricted_mode = false;

  for (auto iter : args) {
    const std::string& arg = iter;
    if (arg == "-h") {
      static const char kUsage[] =
          "Usage: start-adv [flags]\n"
          "\n"
          "Flags:\n"
          "\t--restricted|-r\tStart in restricted mode\n";
      cout << kUsage << endl;
      return;
    } else if (arg == "--restricted" || arg == "-r") {
      is_restricted_mode = true;
    }
  }

  PrintCommandStatus(bt_iface->Enable(is_restricted_mode));
}

void HandleGetState(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);
  bluetooth::AdapterState state = static_cast<bluetooth::AdapterState>(
      bt_iface->GetState());
  PrintFieldAndValue("Adapter state", bluetooth::AdapterStateToString(state));
}

void HandleIsEnabled(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);
  bool enabled = bt_iface->IsEnabled();
  PrintFieldAndBoolValue("Adapter enabled", enabled);
}

void HandleGetLocalAddress(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);
  string address = bt_iface->GetAddress();
  PrintFieldAndValue("Adapter address", address);
}

void HandleSetLocalName(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_ARGS_COUNT(args, >=, 1, "No name was given");

  std::string name;
  for (const auto& arg : args)
    name += arg + " ";

  base::TrimWhitespaceASCII(name, base::TRIM_TRAILING, &name);

  PrintCommandStatus(bt_iface->SetName(name));
}

void HandleGetLocalName(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);
  string name = bt_iface->GetName();
  PrintFieldAndValue("Adapter name", name);
}

void HandleAdapterInfo(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);

  cout << COLOR_BOLDWHITE "Adapter Properties: " COLOR_OFF << endl;

  PrintFieldAndValue("\tAddress", bt_iface->GetAddress());
  PrintFieldAndValue("\tState", bluetooth::AdapterStateToString(
      static_cast<bluetooth::AdapterState>(bt_iface->GetState())));
  PrintFieldAndValue("\tName", bt_iface->GetName());
  PrintFieldAndBoolValue("\tMulti-Adv. supported",
                         bt_iface->IsMultiAdvertisementSupported());
}

void HandleSupportsMultiAdv(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);

  bool status = bt_iface->IsMultiAdvertisementSupported();
  PrintFieldAndBoolValue("Multi-advertisement support", status);
}

void HandleRegisterBLE(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);

  if (ble_registering.load()) {
    PrintError("In progress");
    return;
  }

  if (ble_client_id.load()) {
    PrintError("Already registered");
    return;
  }

  sp<IBluetoothLowEnergy> ble_iface = bt_iface->GetLowEnergyInterface();
  if (!ble_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth Low Energy interface");
    return;
  }

  bool status = ble_iface->RegisterClient(new CLIBluetoothLowEnergyCallback());
  ble_registering = status;
  PrintCommandStatus(status);
}

void HandleUnregisterBLE(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);

  if (!ble_client_id.load()) {
    PrintError("Not registered");
    return;
  }

  sp<IBluetoothLowEnergy> ble_iface = bt_iface->GetLowEnergyInterface();
  if (!ble_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth Low Energy interface");
    return;
  }

  ble_iface->UnregisterClient(ble_client_id.load());
  ble_client_id = 0;
  PrintCommandStatus(true);
}

void HandleUnregisterAllBLE(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);

  sp<IBluetoothLowEnergy> ble_iface = bt_iface->GetLowEnergyInterface();
  if (!ble_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth Low Energy interface");
    return;
  }

  ble_iface->UnregisterAll();
  PrintCommandStatus(true);
}

void HandleRegisterGATT(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);

  if (gatt_registering.load()) {
    PrintError("In progress");
    return;
  }

  if (gatt_client_id.load()) {
    PrintError("Already registered");
    return;
  }

  sp<IBluetoothGattClient> gatt_iface = bt_iface->GetGattClientInterface();
  if (!gatt_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth GATT Client interface");
    return;
  }

  bool status = gatt_iface->RegisterClient(new CLIGattClientCallback());
  gatt_registering = status;
  PrintCommandStatus(status);
}

void HandleUnregisterGATT(IBluetooth* bt_iface, const vector<string>& args) {
  CHECK_NO_ARGS(args);

  if (!gatt_client_id.load()) {
    PrintError("Not registered");
    return;
  }

  sp<IBluetoothGattClient> gatt_iface = bt_iface->GetGattClientInterface();
  if (!gatt_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth GATT Client interface");
    return;
  }

  gatt_iface->UnregisterClient(gatt_client_id.load());
  gatt_client_id = 0;
  PrintCommandStatus(true);
}

void HandleRefreshDevice(IBluetooth* bt_iface, const vector<string>& args) {
  string address;

  if (args.size() != 1) {
    PrintError("Expected MAC address as only argument");
    return;
  }

  address = args[0];

  if (!gatt_client_id.load()) {
    PrintError("Not registered");
    return;
  }

  sp<IBluetoothGattClient> gatt_iface = bt_iface->GetGattClientInterface();
  if (!gatt_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth GATT Client interface");
    return;
  }

  gatt_iface->RefreshDevice(gatt_client_id.load(), address.c_str());
  PrintCommandStatus(true);
}

void HandleSetCharNotify(IBluetooth* bt_iface, const vector<string>& args) {
  string address;
  int handle;
  bool enable;

  if (args.size() != 3) {
    PrintError("Expected 3 arguments: MAC address, handle of characteristic, true|false");
    return;
  }

  address = args[0];
  handle = std::stoi(args[1]);
  enable = (args[2] == "true");

  if (!gatt_client_id.load()) {
    PrintError("Not registered");
    return;
  }

  sp<IBluetoothGattClient> gatt_iface = bt_iface->GetGattClientInterface();
  if (!gatt_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth GATT Client interface");
    return;
  }

  gatt_iface->SetCharacteristicNotification(gatt_client_id.load(), address.c_str(), handle, enable);
  PrintCommandStatus(true);
}

void HandleDiscoverServices(IBluetooth* bt_iface, const vector<string>& args) {
  string address;

  if (args.size() != 1) {
    PrintError("Expected MAC address as only argument");
    return;
  }

  address = args[0];

  if (!ble_client_id.load()) {
    PrintError("BLE not registered");
    return;
  }

  sp<IBluetoothLowEnergy> ble_iface = bt_iface->GetLowEnergyInterface();
  if (!ble_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth Low Energy interface");
    return;
  }

  bool status = ble_iface->DiscoverServices(ble_client_id.load(), address.c_str());
  PrintCommandStatus(status);
}

void HandleGetGattDb(IBluetooth* bt_iface, const vector<string>& args) {
  string address;

  if (args.size() != 1) {
    PrintError("Expected MAC address as only argument");
    return;
  }

  address = args[0];

  if (!ble_client_id.load()) {
    PrintError("BLE not registered");
    return;
  }

  sp<IBluetoothLowEnergy> ble_iface = bt_iface->GetLowEnergyInterface();
  if (!ble_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth Low Energy interface");
    return;
  }

  bool status = ble_iface->GetGattDb(ble_client_id.load(), address.c_str());
  PrintCommandStatus(status);
}

void HandleReadChar(IBluetooth* bt_iface, const vector<string>& args) {
  string address;
  int handle;

  if (args.size() != 2) {
    PrintError("Expected MAC address and handle as only arguments");
    return;
  }

  address = args[0];
  handle = std::stoi(args[1]);

  if (!ble_client_id.load()) {
    PrintError("BLE not registered");
    return;
  }

  sp<IBluetoothLowEnergy> ble_iface = bt_iface->GetLowEnergyInterface();
  if (!ble_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth Low Energy interface");
    return;
  }

  bool status = ble_iface->ReadCharacteristic(ble_client_id.load(), address.c_str(), handle);
  PrintCommandStatus(status);
}

void HandleWriteChar(IBluetooth* bt_iface, const vector<string>& args) {
  string address;
  int handle;
  int write_type;
  std::vector<uint8_t> value;

  if (args.size() != 4) {
    PrintError("Expected MAC address, handle, write type and hex-string value as only arguments");
    return;
  }

  address = args[0];
  handle = std::stoi(args[1]);
  write_type = std::stoi(args[2]);
  base::HexStringToBytes(args[3], &value);

  if (!ble_client_id.load()) {
    PrintError("BLE not registered");
    return;
  }

  sp<IBluetoothLowEnergy> ble_iface = bt_iface->GetLowEnergyInterface();
  if (!ble_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth Low Energy interface");
    return;
  }

  bool status = ble_iface->WriteCharacteristic(ble_client_id.load(),
                                               address.c_str(),
                                               handle,
                                               write_type,
                                               value);
  PrintCommandStatus(status);
}

void HandleWriteDesc(IBluetooth* bt_iface, const vector<string>& args) {
  string address;
  int handle;
  int write_type;
  std::vector<uint8_t> value;

  if (args.size() != 4) {
    PrintError("Expected MAC address, handle, write type and hex-string value as only arguments");
    return;
  }

  address = args[0];
  handle = std::stoi(args[1]);
  write_type = std::stoi(args[2]);
  base::HexStringToBytes(args[3], &value);

  if (!ble_client_id.load()) {
    PrintError("BLE not registered");
    return;
  }

  sp<IBluetoothLowEnergy> ble_iface = bt_iface->GetLowEnergyInterface();
  if (!ble_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth Low Energy interface");
    return;
  }

  bool status = ble_iface->WriteDescriptor(ble_client_id.load(),
                                           address.c_str(),
                                           handle,
                                           write_type,
                                           value);
  PrintCommandStatus(status);
}

void HandleStartAdv(IBluetooth* bt_iface, const vector<string>& args) {
  bool include_name = false;
  bool include_tx_power = false;
  bool connectable = false;
  bool set_manufacturer_data = false;
  bool set_uuid = false;
  bluetooth::UUID uuid;

  for (auto iter = args.begin(); iter != args.end(); ++iter) {
    const std::string& arg = *iter;
    if (arg == "-n")
      include_name = true;
    else if (arg == "-t")
      include_tx_power = true;
    else if (arg == "-c")
      connectable = true;
    else if (arg == "-m")
      set_manufacturer_data = true;
    else if (arg == "-u") {
      // This flag has a single argument.
      ++iter;
      if (iter == args.end()) {
        PrintError("Expected a UUID after -u");
        return;
      }

      std::string uuid_str = *iter;
      uuid = bluetooth::UUID(uuid_str);
      if (!uuid.is_valid()) {
        PrintError("Invalid UUID: " + uuid_str);
        return;
      }

      set_uuid = true;
    }
    else if (arg == "-h") {
      static const char kUsage[] =
          "Usage: start-adv [flags]\n"
          "\n"
          "Flags:\n"
          "\t-n\tInclude device name\n"
          "\t-t\tInclude TX power\n"
          "\t-c\tSend connectable adv. packets (default is non-connectable)\n"
          "\t-m\tInclude random manufacturer data\n"
          "\t-h\tShow this help message\n";
      cout << kUsage << endl;
      return;
    }
    else {
      PrintError("Unrecognized option: " + arg);
      return;
    }
  }

  if (!ble_client_id.load()) {
    PrintError("BLE not registered");
    return;
  }

  sp<IBluetoothLowEnergy> ble_iface = bt_iface->GetLowEnergyInterface();
  if (!ble_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth Low Energy interface");
    return;
  }

  std::vector<uint8_t> data;
  if (set_manufacturer_data) {
    data = {{
      0x07, bluetooth::kEIRTypeManufacturerSpecificData,
      0xe0, 0x00,
      'T', 'e', 's', 't'
    }};
  }

  if (set_uuid) {
    // Determine the type and length bytes.
    int uuid_size = uuid.GetShortestRepresentationSize();
    uint8_t type;
    if (uuid_size == bluetooth::UUID::kNumBytes128)
      type = bluetooth::kEIRTypeComplete128BitUUIDs;
    else if (uuid_size == bluetooth::UUID::kNumBytes32)
      type = bluetooth::kEIRTypeComplete32BitUUIDs;
    else if (uuid_size == bluetooth::UUID::kNumBytes16)
      type = bluetooth::kEIRTypeComplete16BitUUIDs;
    else
      NOTREACHED() << "Unexpected size: " << uuid_size;

    data.push_back(uuid_size + 1);
    data.push_back(type);

    auto uuid_bytes = uuid.GetFullLittleEndian();
    int index = (uuid_size == 16) ? 0 : 12;
    data.insert(data.end(), uuid_bytes.data() + index,
                uuid_bytes.data() + index + uuid_size);
  }

  base::TimeDelta timeout;

  bluetooth::AdvertiseSettings settings(
      bluetooth::AdvertiseSettings::MODE_LOW_POWER,
      timeout,
      bluetooth::AdvertiseSettings::TX_POWER_LEVEL_MEDIUM,
      connectable);

  bluetooth::AdvertiseData adv_data(data);
  adv_data.set_include_device_name(include_name);
  adv_data.set_include_tx_power_level(include_tx_power);

  bluetooth::AdvertiseData scan_rsp;

  bool status = ble_iface->StartMultiAdvertising(ble_client_id.load(),
                                                 adv_data, scan_rsp, settings);
  PrintCommandStatus(status);
}

void HandleStopAdv(IBluetooth* bt_iface, const vector<string>& args) {
  if (!ble_client_id.load()) {
    PrintError("BLE not registered");
    return;
  }

  sp<IBluetoothLowEnergy> ble_iface = bt_iface->GetLowEnergyInterface();
  if (!ble_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth Low Energy interface");
    return;
  }

  bool status = ble_iface->StopMultiAdvertising(ble_client_id.load());
  PrintCommandStatus(status);
}

void HandleConnect(IBluetooth* bt_iface, const vector<string>& args) {
  string address;

  if (args.size() != 1) {
    PrintError("Expected MAC address as only argument");
    return;
  }

  address = args[0];

  if (!ble_client_id.load()) {
    PrintError("BLE not registered");
    return;
  }

  sp<IBluetoothLowEnergy> ble_iface = bt_iface->GetLowEnergyInterface();
  if (!ble_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth Low Energy interface");
    return;
  }

  bool status = ble_iface->Connect(ble_client_id.load(), address.c_str(),
                                   false /*  is_direct */);

  PrintCommandStatus(status);
}

void HandleDisconnect(IBluetooth* bt_iface, const vector<string>& args) {
  string address;

  if (args.size() != 1) {
    PrintError("Expected MAC address as only argument");
    return;
  }

  address = args[0];

  if (!ble_client_id.load()) {
    PrintError("BLE not registered");
    return;
  }

  sp<IBluetoothLowEnergy> ble_iface = bt_iface->GetLowEnergyInterface();
  if (!ble_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth Low Energy interface");
    return;
  }

  bool status = ble_iface->Disconnect(ble_client_id.load(), address.c_str());

  PrintCommandStatus(status);
}

void HandleSetMtu(IBluetooth* bt_iface, const vector<string>& args) {
  string address;
  int mtu;

  if (args.size() != 2) {
    PrintError("Usage: set-mtu [address] [mtu]");
    return;
  }

  address = args[0];
  mtu = std::stoi(args[1]);

  if (mtu < 23) {
    PrintError("MTU must be 23 or larger");
    return;
  }

  if (!ble_client_id.load()) {
    PrintError("BLE not registered");
    return;
  }

  sp<IBluetoothLowEnergy> ble_iface = bt_iface->GetLowEnergyInterface();
  if (!ble_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth Low Energy interface");
    return;
  }

  bool status = ble_iface->SetMtu(ble_client_id.load(), address.c_str(), mtu);
  PrintCommandStatus(status);
}

void HandleStartLeScan(IBluetooth* bt_iface, const vector<string>& args) {
  if (!ble_client_id.load()) {
    PrintError("BLE not registered");
    return;
  }

  for (auto arg : args) {
    if (arg == "-d") {
      dump_scan_record = true;
    } else if (arg == "-h") {
      static const char kUsage[] =
          "Usage: start-le-scan [flags]\n"
          "\n"
          "Flags:\n"
          "\t-d\tDump scan record\n"
          "\t-h\tShow this help message\n";
      cout << kUsage << endl;
      return;
    }
  }

  sp<IBluetoothLowEnergy> ble_iface = bt_iface->GetLowEnergyInterface();
  if (!ble_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth Low Energy interface");
    return;
  }

  bluetooth::ScanSettings settings;
  std::vector<bluetooth::ScanFilter> filters;

  bool status = ble_iface->StartScan(ble_client_id.load(), settings, filters);
  PrintCommandStatus(status);
}

void HandleStopLeScan(IBluetooth* bt_iface, const vector<string>& args) {
  if (!ble_client_id.load()) {
    PrintError("BLE not registered");
    return;
  }

  sp<IBluetoothLowEnergy> ble_iface = bt_iface->GetLowEnergyInterface();
  if (!ble_iface.get()) {
    PrintError("Failed to obtain handle to Bluetooth Low Energy interface");
    return;
  }

  bluetooth::ScanSettings settings;
  std::vector<bluetooth::ScanFilter> filters;

  bool status = ble_iface->StopScan(ble_client_id.load());
  PrintCommandStatus(status);
}

void HandleHelp(IBluetooth* bt_iface, const vector<string>& args);

struct {
  string command;
  void (*func)(IBluetooth*, const vector<string>& args);
  string help;
} kCommandMap[] = {
  { "help", HandleHelp, "\t\t\tDisplay this message" },
  { "disable", HandleDisable, "\t\t\tDisable Bluetooth" },
  { "enable", HandleEnable, "\t\t\tEnable Bluetooth (-h for options)" },
  { "get-state", HandleGetState, "\t\tGet the current adapter state" },
  { "is-enabled", HandleIsEnabled, "\t\tReturn if Bluetooth is enabled" },
  { "get-local-address", HandleGetLocalAddress,
    "\tGet the local adapter address" },
  { "set-local-name", HandleSetLocalName, "\t\tSet the local adapter name" },
  { "get-local-name", HandleGetLocalName, "\t\tGet the local adapter name" },
  { "adapter-info", HandleAdapterInfo, "\t\tPrint adapter properties" },
  { "supports-multi-adv", HandleSupportsMultiAdv,
    "\tWhether multi-advertisement is currently supported" },
  { "register-ble", HandleRegisterBLE,
    "\t\tRegister with the Bluetooth Low Energy interface" },
  { "unregister-ble", HandleUnregisterBLE,
    "\t\tUnregister from the Bluetooth Low Energy interface" },
  { "unregister-all-ble", HandleUnregisterAllBLE,
    "\tUnregister all clients from the Bluetooth Low Energy interface" },
  { "register-gatt", HandleRegisterGATT,
    "\t\tRegister with the Bluetooth GATT Client interface" },
  { "unregister-gatt", HandleUnregisterGATT,
    "\t\tUnregister from the Bluetooth GATT Client interface" },
  { "refresh-device", HandleRefreshDevice,
    "\t\tRefresh Device GATT information" },
  { "discover-services", HandleDiscoverServices,
    "\tDiscover GATT Services offered by connected device" },
  { "get-gatt-db", HandleGetGattDb,
    "\t\tGet GATT DB offered by connected device" },
  { "read-char", HandleReadChar,
    "\t\tRead the value of a characteristic by handle" },
  { "write-char", HandleWriteChar,
    "\t\tWrite the value of a characteristic by handle" },
  { "write-desc", HandleWriteDesc,
    "\t\tWrite the value of a descriptor by handle" },
  { "set-char-notify", HandleSetCharNotify,
    "\t\tEnable or disable notifications/indications for a given characteristic" },
  { "connect-le", HandleConnect, "\t\tConnect to LE device (-h for options)"},
  { "disconnect-le", HandleDisconnect,
    "\t\tDisconnect LE device (-h for options)"},
  { "set-mtu", HandleSetMtu, "\t\t\tSet MTU (-h for options)"},
  { "start-adv", HandleStartAdv, "\t\tStart advertising (-h for options)" },
  { "stop-adv", HandleStopAdv, "\t\tStop advertising" },
  { "start-le-scan", HandleStartLeScan,
    "\t\tStart LE device scan (-h for options)" },
  { "stop-le-scan", HandleStopLeScan, "\t\tStop LE device scan" },
  {},
};

void HandleHelp(IBluetooth* /* bt_iface */, const vector<string>& /* args */) {
  cout << endl;
  for (int i = 0; kCommandMap[i].func; i++)
    cout << "\t" << kCommandMap[i].command << kCommandMap[i].help << endl;
  cout << endl;
}

const char kExecuteLong[] = "exec";
const char kExecuteShort[] = "e";

bool ExecuteCommand(sp<IBluetooth> bt_iface, std::string &command) {
  vector<string> args =
      base::SplitString(command, " ", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_ALL);

  if (args.empty())
    return true;

  // The first argument is the command while the remaining are what we pass to
  // the handler functions.
  command = args[0];
  args.erase(args.begin());

  for (int i = 0; kCommandMap[i].func; i++) {
    if (command == kCommandMap[i].command) {
      kCommandMap[i].func(bt_iface.get(), args);
      return true;
    }
  }

  cout << "Unrecognized command: " << command << endl;
  return false;
}

}  // namespace

class BluetoothDeathRecipient : public android::IBinder::DeathRecipient {
 public:
  BluetoothDeathRecipient() = default;
  ~BluetoothDeathRecipient() override = default;

  // android::IBinder::DeathRecipient override:
  void binderDied(const android::wp<android::IBinder>& /* who */) override {
    BeginAsyncOut();
    cout << COLOR_BOLDWHITE "The Bluetooth daemon has died" COLOR_OFF << endl;
    cout << "\nPress 'ENTER' to exit.";
    EndAsyncOut();

    android::IPCThreadState::self()->stopProcess();
    should_exit = true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothDeathRecipient);
};


int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  logging::LoggingSettings log_settings;

  if (!logging::InitLogging(log_settings)) {
    LOG(ERROR) << "Failed to set up logging";
    return EXIT_FAILURE;
  }

  sp<IBluetooth> bt_iface = IBluetooth::getClientInterface();
  if (!bt_iface.get()) {
    LOG(ERROR) << "Failed to obtain handle on IBluetooth";
    return EXIT_FAILURE;
  }

  sp<BluetoothDeathRecipient> dr(new BluetoothDeathRecipient());
  if (android::IInterface::asBinder(bt_iface.get())->linkToDeath(dr) !=
      android::NO_ERROR) {
    LOG(ERROR) << "Failed to register DeathRecipient for IBluetooth";
    return EXIT_FAILURE;
  }

  // Initialize the Binder process thread pool. We have to set this up,
  // otherwise, incoming callbacks from IBluetoothCallback will block the main
  // thread (in other words, we have to do this as we are a "Binder server").
  android::ProcessState::self()->startThreadPool();

  // Register Adapter state-change callback
  sp<CLIBluetoothCallback> callback = new CLIBluetoothCallback();
  bt_iface->RegisterCallback(callback);

  cout << COLOR_BOLDWHITE << "Fluoride Command-Line Interface\n" << COLOR_OFF
       << endl
       << "Type \"help\" to see possible commands.\n"
       << endl;

  string command;

  // Add commands from the command line, if they exist.
  auto command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kExecuteLong)) {
    command += command_line->GetSwitchValueASCII(kExecuteLong);
  }

  if (command_line->HasSwitch(kExecuteShort)) {
    if (!command.empty())
      command += " ; ";
    command += command_line->GetSwitchValueASCII(kExecuteShort);
  }

  while (true) {
    vector<string> commands = base::SplitString(command, ";",
                                                base::TRIM_WHITESPACE,
                                                base::SPLIT_WANT_ALL);
    for (string command : commands) {
      if (!ExecuteCommand(bt_iface, command))
        break;
    }

    commands.clear();

    PrintPrompt();

    showing_prompt = true;
    auto& istream = getline(cin, command);
    showing_prompt = false;

    if (istream.eof() || should_exit.load()) {
      cout << "\nExiting" << endl;
      return EXIT_SUCCESS;
    }

    if (!istream.good()) {
      LOG(ERROR) << "An error occured while reading input";
      return EXIT_FAILURE;
    }

  }

  return EXIT_SUCCESS;
}
