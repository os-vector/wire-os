## Power Management

### Overview

Power management (PM) is an event-driven state machine, tickled by various
`bta/sys` events via a callback. The actual state switching calls are handled
by the BTM HCI interfacing code, with results being posted back to the PM
code via the BTA workqueue thread.

Power states are managed per-device, per-profile, so every incoming event
includes a profile ID, app ID, and a `BD_ADDR`.

The events fired to drive the state machine at the time of this writing are:

  - `BTA_SYS_CONN_OPEN`
  - `BTA_SYS_CONN_CLOSE`
  - `BTA_SYS_CONN_IDLE`
  - `BTA_SYS_CONN_BUSY`
  - `BTA_SYS_APP_OPEN`
  - `BTA_SYS_APP_CLOSE`
  - `BTA_SYS_SCO_OPEN`
  - `BTA_SYS_SCO_CLOSE`

Each of these correspond to a function name in `bta/sys/bta_sys_conn.c`, which
are called by each profile definition in `bta/$PROFILE`.

The PM code makes calls into the BTM module to set various power
states. Responses are handled in an asynchronous fashion, primarily via the
callbacks `bta_dm_pm_cback` and `bta_dm_pm_timer_cback`. Responses are handled
through the BTA workqueue thread and the `bta_dm_pm_btm_status` function. Since
we might possibly get into a bad state where we never hear back from the
controller, timers are used to post messages to the BTA workqueue thread as
well, which filters down through the same status function.

Overall power states are managed *per device*, not per connection, but the power
policy is determined by the greatest allowable power action defined across all
currently known connections to a given device. Thus, if RFCOMM specifies that
it's willing to go to into SNIFF and specifies that as an action, and say, a PAN
connection is up which specifies it is willing to go into SNIFF, but its action
states it wants ACTIVE, the power management code will change to ACTIVE.

### Power management tables

The tables that determine which power levels are acceptable for which profiles
and what actions to take for the above events are defined in the
`bta/dm/bta_dm_cfg.c` file, as `bta_dm_pm_cfg`, `bta_dm_pm_spec`, and
`bta_dm_ssr_spec`.

During a lookup attempt, the code iterates over the `bta_dm_pm_cfg` array,
looking for a match between the profile and app IDs. When it finds one, it uses
the `spec_idx` field to index into `bta_dm_pm_spec` array to determine which
power modes are acceptable and what actions to take for each event.

The action constants are defined in `bta_api.h` and are defined as a series of
hex bitfields. The actual actions taken are determined by the
`bta_dm_pm_set_mode` function, but a few of the actions listed deserve some
additional description:

  - `BTA_DM_PM_NO_ACTION` is effectively a no-op and has a value of zero, so any
    other profile will override this.
  - `BTA_DM_PM_NO_PREF` overrides `BTA_DM_PM_NO_ACTION` and if selected as the
    action that `bta_dm_pm_set_mode` will take, the connection will be removed
    from `bta_dm_conn_srvcs` and no longer be considered for power management
    decisions.
  - `BTA_DM_PM_SNIFF` through `BTA_DM_PM_SNIFF4` are special, in that each
    level specifies a set of parameters for the SNIFF mode which relate to the
    min and max intervals, the number of attempts and the timeout. The overall
    action is still the same, however -- SNIFF mode is attempted. There are
    definitions available up to SNIFF7, but actual SSR values are only defined
    up to SNIFF4. Params are defined in `bta_dm_ssr_spec`.
  - `BTA_DM_PM_ACTIVE` is full-on power.
  - `BTA_DM_PM_RETRY` has the same effect as `BTA_DM_PM_NO_ACTION`, except a
    timeout is possible to be set, which effectively allows a power operation to
    be "retried".

### Initialization

`bta_dm_pm.c`'s `bta_dm_init_pm` function calls out to register
`bta_dm_pm_cback` with the bta sys module for incoming power management events,
and also registers `bta_dm_pm_btm_cback` with the btm module to handle responses
and timeouts of HCI requests (via `bta_dm_pm_btm_status`).

At this point, the power managment code is basically done until the first set of
events come in through `bta_dm_pm_cback`.

Throughout the `bta_dm_pm.c` file, connections whose power management states are
managed are tracked in a global array called `bta_dm_conn_srvcs`. Unfortunately,
while this variable is declared as an extern in the `bta_dm_int.h` file, it only
seems to be used in the `bta_dm_act.c` file, and only for reinitialization.

### Event flow

#### Events fired from SYS

  1. An event is fired from one of the methods mentioned above in
     `bta/sys/bta_sys_conn.c`
  2. The `bta_dm_pm_cback` function is called.
     - The power mode config is looked up in the `bta_dm_pm_cfg` table. If none
       are found for the given profile ID and app ID, the function simply
       returns with no action taken.
     - If any timers were set for the given `BD_ADDR`, they are stopped.
     - The SSR params for the CONN_OPEN event are looked up.
     - The power spec state table (`bta_dm_pm_spec`) is checked to see if
       there's no action to be performed (`BTA_DM_PM_NO_ACTION`), and if so,
       returns with no action taken.
     - `bta_dm_conn_srvcs` is consulted to ensure there's an entry for this
       connection if it's supposed to be managed according to the power spec
       state tables. If the spec specifies `BTA_DM_PM_NO_PREF`, then any
       existing entry in this list is removed, otherwise one is added/updated
       with the state given to the function.
  3. `bta_dm_pm_cback` checks to see if the `bta_dm_ssr_spec` specifies SSR
     adjustments are to be made, and if so, `bta_dm_pm_ssr` is called with the
     peer `BD_ADDR`.
     - `bta_dm_pm_ssr` iterates the managed services array to find all connected
       services for the given `BD_ADDR`, then looks up the ssr values from the
       `bta_dm_ssr_spec` tables, looking for the smallest max latency to use.
     - `bta_dm_pm_ssr` calls `BTM_SetSsrParams` to actually send along the SSR
       params to the bluetooth chip.
  4. `bta_dm_pm_cback` calls `bta_dm_pm_set_mode` with the peer address and the
     `timed_out` parameter set to `false`.
     - For each managed connection, `bta_dm_pm_set_mode` grabs
       both actions specified for the profile in the `bta_dm_pm_spec` tables. If
       the first power management action didn't timeout (or was never attempted,
       according to the `tBTA_DM_PEER_DEVICE` `pm_mode_failed` and
       `pm_mode_attempted` fields), its timeout and mode are used. Otherwise,
       the same check is done against the second action and it is used
       instead. If both actions have been attempted, then the action is set to
       `BTA_DM_PM_NO_ACTION`. Only the highest power mode action is chosen from
       all connected profiles.
     - If the chosen action is `BTA_DM_PM_PARK` or `BTA_DM_PM_SNIFF` but the
       profile doesn't allow it, this function takes no action.
     - If a timeout is specified in the power spec table, then an unused timer
       in `bta_dm_cb.pm_timer` is started.
     - If the action chosen is `BTA_DM_PM_PARK`, `bta_dm_pm_park` is called,
       which calls `BTM_ReadPowerMode` and `BTM_SetPowerMode` to make an HCI
       request to enable PARK for the given peer and connection.
     - If the action chosen is `BTA_DM_PM_SNIFF`, the peer device's link policy
       is checked to see if it's allowed. If so, then `bta_dm_pm_sniff` is
       called, which makes various calls to `BTM_ReadLocalFeatures`,
       `BTM_ReadRemoteFeatures` and `BTM_SetPowerMode` to ensure SNIFF mode is
       enabled.
     - If the action chosen is `BTA_DM_PM_ACTIVE`, a call to `bta_dm_pm_active`
       is made, which calls `BTM_SetPowerMode` to set the link into ACTIVE
       mode.

At this point, if one of the timers in `bta_dm_cb.pm_timer` times out, a call is
made through the BTA workqueue thread to `bta_dm_pm_btm_cback`, which then
triggers `bta_dm_pm_btm_status`, with the timeout field set to TRUE. HCI
responses are also fired as messages through the BTA workqueue thread, which are
handled again, through `bta_dm_pm_btm_status`.

#### Events fired through BTM

Essentially these messages eventually go through the same functions as events
fired from the SYS side of things, except from the initial path they take:

  1. An event is fired from a callback in BTM to `bta_dm_pm_btm_cback`.
  2. `bta_dm_pm_btm_cback` packages up the given parameters into a
     `tBTA_DM_PM_BTM_STATUS` struct and posts it to the BTA workqueue thread via
     `bta_sys_sendmsg`, with the event header set to
     `BTA_DM_PM_BTM_STATUS_EVT`.
  3. This is eventually routed to the `bta_dm_pm_btm_status` function.
     **Determine if this is running on the workqueue thread or not**
     - The message `status` passed in is actually the current status of the
       device.
     - If the status is `BTM_PM_STS_ACTIVE` (still in the ACTIVE power mode),
       checks the HCI status code:
       - If that's non-zero and a PARK or SNIFF mode change was attempted,
         `bta_dm_pm_btm_status` stops any timers started for the device in
         `bta_dm_pm_set_mode`, clears some status bits in the peer device
         structure, and then calls back into `bta_dm_pm_set_mode` with the peer
         device address and timeout set to FALSE.
       - If the status is zero, and if the peer device `tBTA_DM_PEER_DEVICE`
         `prev_low` field is set, calls `bta_dm_pm_ssr` to re-send SSR params,
         stops all timers for the device, and then re-calls `bta_dm_pm_set_mode`
         with timeout set to FALSE to re-attempt with a second action (if the
         previous PARK or SNIFF failed, otherwise it'll re-attempt the first
         action).
     - If the status is `BTM_PM_STS_PARK` or `BTM_PM_STS_HOLD`, saves the
       previous low power mode in the peer device's `prev_low` field.
     - If the status is `BTM_PM_STS_SSR`, simply clears or sets the device
       `info` field's `BTA_DM_DI_USE_SSR` bit, depending on the value of
       `tBTA_DM_MSG.value`, which determines if the device can handle SSR.
     - If the status is `BTM_PM_STS_SNIFF` and the info field has the
       `BTA_DM_DI_SET_SNIFF` bit set, then `BTA_DM_DI_INT_SNIFF` is set,
       otherwise `BTA_DM_DI_ACP_SNIFF` is set.
     - If `BTA_PM_STS_ERROR`, the `BTA_DM_DI_SET_SNIFF` bit is cleared in the
       device info field.

At this point, either the method simply returns, or has called back into
`bta_dm_pm_set_mode`, in which case the usual flow takes over.

#### Events fired from timers

Timers are used exclusively for handling HCI command timeouts, and filter
through to a call to `bta_dm_pm_set_mode`:

  1. A timer expires, and calls `bta_dm_pm_timer_cback`.
  2. `bta_dm_pm_timer_cback` clears the use flag on the timer that fired, and
     sends off an event to the BTA workqueue thread.
  3. The event eventually fires off a call to `bta_dm_pm_timer`, which just
     calls `bta_dm_pm_set_mode` with timeout set to `TRUE`.
