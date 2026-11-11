# Ignition setup notes

Gateway-side configuration that is **not** captured by the tag and UDT exports in `ignition/`. Use this to rebuild the Gateway from scratch. No passwords are stored here.

Environment: Ignition Maker Edition 8.3.10, MariaDB 11.8.9, Mosquitto 2.x, Windows 11.

## 1. Database

| Item | Value |
|---|---|
| Connection name | `P1_MariaDB` |
| Driver | MariaDB (built into Ignition 8.3) |
| Connect URL | `jdbc:mariadb://localhost:3307/p1_scada` |
| Database / user | `p1_scada` / `ignition` (privileges on `p1_scada` only) |

MariaDB listens on **3307**, not the default 3306, because 3306 was already taken on this PC.

## 2. History and alarm storage

| Item | Value |
|---|---|
| Historian | `P1_SQL_Historian` (SQL Historian on `P1_MariaDB`), default settings: monthly partitions, pruning off |
| Alarm journal | `P1_AlarmJournal` on `P1_MariaDB`, **Store and Forward ticked**, minimum priority Diagnostic |

Tables are created on first use: `sqlt_data_1_<year>_<month>` and the `sqlth_*` metadata tables for history, `alarm_events` and `alarm_event_data` for the journal.

## 3. MQTT

**Broker (Mosquitto):** `listener 1883` and `allow_anonymous true` in `mosquitto.conf`. Anonymous access is a lab simplification. A Windows Firewall rule allows TCP 1883 on **Private** networks only.

**MQTT Engine** (Cirrus Link): the default server `Chariot SCADA` (`tcp://localhost:1883`, Default Set) connects to the local broker. Two custom namespaces handle the ESP32's plain (non-Sparkplug) messages:

| Namespace | Subscription | JSON payload | Numbers as Floats |
|---|---|---|---|
| `P1_Telemetry` | `p1/+/telemetry` | on | **on** |
| `P1_Status` | `p1/+/status` | off | off |

"Numbers as Floats" is needed because the ESP32 sometimes sends `27` instead of `27.0`. Without it the first value would create an Integer tag that clips later readings.

**ESP32 topics:** `p1/esp32-01/telemetry` (JSON: `temperature`, `humidity`, every 5 s) and `p1/esp32-01/status` (`online`, or `offline` from the MQTT Last Will).

Raw tags appear in the `[MQTT Engine]` provider and are read-only.

## 4. Tag structure (provider `default`)

```
P1/
  ESP32/
    Temperature   (P1_AnalogSignal)  SourcePath = [MQTT Engine]p1/esp32-01/telemetry/temperature
    Humidity      (P1_AnalogSignal)  SourcePath = [MQTT Engine]p1/esp32-01/telemetry/humidity
    Online        (Expression)       if({[MQTT Engine]p1/esp32-01/status} = "online", 1, 0)
  Siemens/        (Step 7)
  AllenBradley/   (Step 2)
```

**UDT `P1_AnalogSignal`** (parameter `SourcePath`, String):

| Member | Type | Notes |
|---|---|---|
| `Value` | Reference tag | Source Tag Path bound to `{SourcePath}`. History: `P1_SQL_Historian`, Periodic, 5000 ms |
| `HighLimit` | Memory | Default 100, Value Persistence = Database |
| `LowLimit` | Memory | Default 0, Value Persistence = Database |

Reference tags let the same template be reused for Siemens and Allen-Bradley signals later: only `SourcePath` changes.

## 5. Alarms

| Alarm | On | Mode | Setpoint | Priority | Deadband |
|---|---|---|---|---|---|
| `High` | `Value` (UDT) | Above Setpoint | `{[.]HighLimit}` | High | 0.5 |
| `Low` | `Value` (UDT) | Below Setpoint | `{[.]LowLimit}` | Medium | 0.5 |
| `ESP32 Offline` | `ESP32/Online` | Equal | 0 | Critical | none |

Setpoints are bound to sibling memory tags, so an operator changes alarm limits by writing to `HighLimit` / `LowLimit` with no edit to the alarm itself. Instance values: Temperature 32 / 20, Humidity 85 / 30.

## 6. Design notes and quirks

- **Device-offline detection:** when the ESP32 drops off, its MQTT tags keep their last value with **Good** quality. The reliable signal is the Last Will on the `status` topic, which feeds the `Online` tag and its alarm.
- **Alarm Metrics:** the bell icon on a tag only shows that alarms are configured. Use the `Alarm Metrics` folder (for example `HasActive`) or the alarm journal to see state.
- **Network:** the ESP32 only connects on 2.4 GHz Wi-Fi, and the PC's network profile must be **Private** or the firewall rule does not apply.
- **Designer comm mode:** writing tag values from the Designer needs Project > Comm Read/Write.

## 7. Verified (2026-11-11)

| Test | Result |
|---|---|
| History rows | New row every 5 s per signal in `sqlt_data_1_2026_11`, quality Good |
| Limit-driven alarms | Lowering `HighLimit` below the reading raised the High alarm. Moving `LowLimit` above it raised the Low alarm. Both cleared when the limits were restored |
| Journal | Active and cleared events written to `alarm_events` |
| Persistence | `HighLimit` changed to 33 survived a Gateway restart |
| Device offline | Unplugging the ESP32 set `Online` to 0 within ~25 s and raised `ESP32 Offline` (Critical). Replugging cleared it |

## 8. Not yet built

Perspective dashboard (Step 6), Allen-Bradley source (Step 2), Siemens source (Step 7), outage demo (Step 8).
