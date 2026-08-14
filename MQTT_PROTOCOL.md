# WebCan — MQTT CAN Communication Protocol

This document describes the MQTT payloads that WebCan uses to exchange CAN
frames with a broker.

- **Transport**: MQTT over TLS (`WiFiClientSecure` + CA certificate loaded from
  SPIFFS at `/isrgrootx1.pem`).
- **Topics are configurable** in the web UI (Settings → MQTT Broker). They
  default to:
  - `webcan/tx` — device **subscribes** here (messages **into** WebCan, then
    forwarded onto the CAN bus)
  - `webcan/rx` — device **publishes** here (frames **received** from the CAN bus)
- **Message buffer**: 2048 bytes (`mqtt.setBufferSize(2048)`).

> Note: the two directions use different payload formats — **inbound** messages
> are plain text, **outbound** messages are JSON.

---

## 1. Inbound — MQTT → CAN (topic `webcan/tx`)

Messages published to the subscribe topic are parsed and queued for TX on
CAN1. The payload is a single-line, space-separated text string.

### Format

```text
ID: <hex-id> [Ext] DLC: <n> Data: <b0> <b1> ... <bn>
```

| Token      | Required | Description                                                        |
| ---------- | -------- | ------------------------------------------------------------------ |
| `ID:`      | yes      | Literal prefix.                                                    |
| `<hex-id>` | yes      | CAN identifier in hex, with or without `0x` prefix (1–8 hex chars). |
| `Ext`      | no       | Marks the frame as a 29-bit extended ID. Keyword is case-insensitive and must appear *after* the `ID:` token (not at position 0). |
| `DLC:`     | yes      | Literal prefix.                                                    |
| `<n>`      | yes      | Data length code, `0`–`8` (values > 8 are clamped to 8).           |
| `Data:`    | no       | Literal prefix. Only parsed when present.                          |
| `<bx>`     | no       | Byte value in 2-digit hex (`00`–`FF`), up to `<n>` bytes.          |

### Examples

Standard 11-bit ID (`0x7E8`), 8 data bytes:

```text
ID: 0x7E8 STD DLC: 8 Data: 03 22 01 05 00 00 00 00
```

Extended 29-bit ID (`0x18DAF110`), 3 data bytes:

```text
ID: 0x18DAF110 Ext DLC: 3 Data: 02 01 00
```

Remote frame request — **not supported**: inbound RTR is ignored, so every
inbound frame is transmitted as a data frame (`DAT`).

### Parsing notes (matches `mqttCallback()`)

- The **ID** is read as hex (`strtoul`, base 16); an `0x` prefix is optional.
- **Ext** is detected by searching for the substring `Ext`/`EXT` anywhere after
  the first character — keep it after `ID:` as shown above.
- **DLC** is read as a decimal number and clamped to `0..8`.
- **Data** bytes are parsed as pairs of hex characters, skipping spaces. Only
  the first `DLC` bytes are taken; missing bytes are left as `00`.
- Keep tokens space-separated. The canonical form always writes `DLC: n Data: …`.

---

## 2. Outbound — CAN → MQTT (topic `webcan/rx`)

Every frame received on CAN1 or CAN2 is published as a compact JSON object.

### Format

```json
{
  "MessageType": "internal.debug.can.frame.send.v1",
  "Payload": {
    "canId": "<hex-id>",
    "canFrame": "<b0> <b1> … <bn>"
  }
}
```

| Field           | Type   | Description                                                        |
| --------------- | ------ | ------------------------------------------------------------------ |
| `MessageType`   | string | Always `internal.debug.can.frame.send.v1`.                         |
| `Payload.canId` | string | Identifier in uppercase hex. Extended IDs are 8 digits (`%08lX`), standard IDs are at least 3 digits (`%03lX`). |
| `Payload.canFrame` | string | Data bytes as space-separated 2-digit uppercase hex (`%02X`), empty string when DLC is 0. |

### Examples

Standard ID `0x7E8`, 8 data bytes:

```json
{
  "MessageType": "internal.debug.can.frame.send.v1",
  "Payload": { "canId": "7E8", "canFrame": "03 22 01 05 00 00 00 00" }
}
```

Extended ID `0x18DAF110`, 3 data bytes:

```json
{
  "MessageType": "internal.debug.can.frame.send.v1",
  "Payload": { "canId": "18DAF110", "canFrame": "02 01 00" }
}
```

Frame with no data (DLC 0):

```json
{
  "MessageType": "internal.debug.can.frame.send.v1",
  "Payload": { "canId": "7E8", "canFrame": "" }
}
```

### Field notes (matches `mqttTask()`)

- The `EXT` flag is **not** sent explicitly — a consumer should treat IDs longer
  than 3 hex digits as extended (11-bit IDs only need up to `7FF`).
- The **bus number** (CAN1 vs CAN2) is not included in the JSON.
- **RTR** is not represented in the JSON payload.
- TX frames injected through the web UI are **not** re-published to MQTT; only
  frames physically received on CAN1/CAN2 are published.

---

## 3. Round-trip example

1. A client sends to `webcan/tx`:

   ```text
   ID: 0x7E8 STD DLC: 8 Data: 03 22 01 05 00 00 00 00
   ```

2. WebCan transmits it on CAN1.

3. A bus response `0x7E9` with data `05 47 7E 80 00 00 00 00` is received on
   CAN1 and published to `webcan/rx`:

   ```json
   {
     "MessageType": "internal.debug.can.frame.send.v1",
     "Payload": { "canId": "7E9", "canFrame": "05 47 7E 80 00 00 00 00" }
   }
   ```
