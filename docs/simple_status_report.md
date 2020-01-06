## <center>Status Report Interface</center>

### <center>Device ===> Server</center>
#### 1. First time connect on report status of device
report sample:
```json
{
    "device_id": "device_id_string",
    "message_type": "update",
    "device_status": {
        "battery": 80, // percent, full 100%
        "optical_sensor_status": 0, // 0: disable, 1: enable
        "lumen": 50, // percent, max lumen is 100%
        "curtain_position": 50 // percent, full open dedicate 100%
    },
    "report_time": timestamp
}
```

device_id: required, unique ID of device, string.

message_type: required, message type of this message, string. valid
value set the table below:

|value|description|
|-----|-----------|
| update | update device status by first time connect on server|
| action | command from server|
| get | get device status |
| set | set device parameters |


#### Response of message
example success:
```json
{
    "result": "OK",
    "code": 0
}
```

example failure:
```json
{
    "result": "Server internal error",
    "code": 501
}
```
#### 2. 
### <center>Server ===> Device</center>
example 1: get all device params from device.
```json
{
    "device_id": "device_id_string",
    "message_type": "get",
    "device_params": "all"
}
```
Response sample:
```json
{
    "device_id": "device_id_string",
    "message_type": "update",
    "device_status": {
        "battery": 80, // percent, full 100%
        "optical_sensor_status": 0, // 0: disable, 1: enable
        "lumen": 50, // percent, max lumen is 100%
        "curtain_position": 50 // percent, full open dedicate 100%
    },
    "report_time": timestamp
}
```

example 2: get battery from device.
```json
{
    "device_id": "device_id_string",
    "message_type": "get",
    "device_params": "battery"
}
```