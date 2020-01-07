## <center>Status Report Interface</center>

#### 1. Report status of device while first time connected.
Report sample:
```json
{
    "device_id": "LY0123456789",
    "message_type": "update",
    "device_params": {
        "battery": 80,
        "optical_sensor_status": 0,
        "lumen": 50,
        "curtain_position": 50,
        "server_ip": "192.168.1.1",
        "server_port": 4567,
        "optical_work_time": [7, 10, 00, 18, 10, 00],
        "curtain_work_time": [7, 10, 00, 18, 10, 00],
        "curtain_repeater": 4
    },
    "report_time": 12033300234
}
```

**device_id**: required, unique ID of device, string. Example:`LY0123456789`

**message_type**: required, message type of this message, string. valid
value see the table below:
|value|description|direction|
|-----|-----------|---------|
| update | update device status by first time connect on server| device ---> server|
| action | command from server| server ---> device |
| get | get device status | server ---> device |
| set | set device parameters | server ---> device |

**device_status**: required, all device current status, json object.
1. **battery**: remain quantity of electricity of battery, int;
2. **optical_sensor_status**: optical sensor status, 0: disable, 1: enable, boolean.
3. **lumen**: lumen percent from max brightness to dark.
4. **curtain_position**: position of curtain, percent, full open dedicate 100%.

**report_time**: timestamp of report update action, int.

#### Response of message
Example success:
```json
{
    "result": "OK",
    "code": 0
}
```

Example failure:
```json
{
    "result": "Server internal error",
    "code": 501
}
```
**result**: update action result, string.

**code**: result code, coding table define in future.

#### 2. Get current device params
Example 1: get all device params from device.
```json
{
    "device_id": "LY0123456789",
    "message_type": "get",
    "device_params": "all"
}
```
**device_id**: required, unique ID of device, string. Example:`LY0123456789`.

**message_type**: fixed value:`get`.

**device_params**: **`all`** **OR** contains device status fields[`battery`, `optical_sensor_status`, `lumen`, `curtain_position`].

Response:
```json
{
    "device_id": "LY0123456789",
    "message_type": "update",
    "device_status": {
        "battery": 80,
        "optical_sensor_status": 0,
        "lumen": 50,
        "curtain_position": 50
    },
    "report_time": 1123349289
}
```

Example 2: get battery and curtain_position from device.
```json
{
    "device_id": "LY0123456789",
    "message_type": "get",
    "device_params": "battery|curtain_position"
}
```

Response:
```json
{
    "device_id": "LY0123456789",
    "message_type": "update",
    "device_status": {
        "battery": 80,
        "curtain_position": 50
    },
    "report_time": 1123349289
}
```

#### 3. Set current device params
Example 1: Set optical_sensor_status the params
```json
{
    "device_id": "LY0123456789",
    "message_type": "set",
    "device_status": {
        "optical_sensor_status": 0,
        "curtain_position": 50
    }
}
```
- Response success:
```json
{
    "result": "Set OK",
    "code": 0
}
```

- Response failure:
```json
{
    "result": "Error, there are something wrong.",
    "code": 501
}
```

Example 2: Set to an read only params
```json
{
    "device_id": "LY0123456789",
    "message_type": "set",
    "device_status": {
        "battery": 80,
        "lumen": 50
    }
}
```
- Response success:
```json
{
    "result": "Error, read only field can not be set.",
    "code": -1
}
```

#### 4. Dispatch command to device
Example 2: Run a defined command
```json
{
    "device_id": "LY0123456789",
    "message_type": "action",
    "cmd": "say_hello"
}
```

- Response success:
```json
{
    "result": "Success",
    "code": 0
}
```

- Response failure:
```json
{
    "result": "fail",
    "code": -1
}
```