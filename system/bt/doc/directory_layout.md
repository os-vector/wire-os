## Directory Layout

### General directory layout
Each directory should be a self contained subsystem consisting
of one or more modules.

* src/ - All source files for the subsystem.
* include/ - All include files for the subsystem.
* test/ - All unit tests for the subsystem.

### Top level directory layout
* audio_a2dp_hw - A2DP audio HAL implementation.
* bta - *Bluetooth Application* - Strange collection of a-lot of things **Deprecate?**
* btcore - *Bluetooth Core* - Bluetooth data type definitions and operations on those data types.
* btif - *Bluetooth Interface* - JNI interface to Android.
* conf - *Configuration* - Various configuration text files.
* doc - *Documentation* - Stack documentation.
* embdrv - **Deprecated** - Bluetooth SBC Codec.
* hci - *Host Controller Interface* - Communication protocol with Bluetooth chip.
* include - **Deprecated** - System global include files.
* main - *Main stack entrypoint* - Provides HAL for stack access.
* osi - *Operating System Interface* - General resource support for stack.
* profiles - *Bluetooth Profiles* - **TBD** Collection of all profiles.
* stack - **Deprecated** - The Stack - Push to proper spot in *./profiles*
* test - *Test suite* - Stack level validiation and stress test suite.
* tools - *Tools* - Various engineering support tools.
* udrv -  **Deprecated**  *UIPC implementation*
* utils - **Deprecated** *Bluetooth utilities* - Eliminate.
* vnd - *Vendor* - Vendor specific APIs - *to be integrated into rest of stack ?*.
