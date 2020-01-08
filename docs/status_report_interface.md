## <center>Intelligent Curtain Interface</center>

#### 1. Report status of device while first time connected.
Report sample:
```json
{
	"device_id":	"LY0123456789",
	"message_type":	"update",
	"device_params":	{
		"battery":	0,
		"optical_sensor_status":	0,
		"lumen":	11,
		"curtain_position":	10,
		"server_ip":	"192.168.2.114",
		"server_port":	4567,
		"optical_work_time":	[0, 10, 11, 0, 10, 13],
		"curtain_work_time":	[0, 30, 8, 0, 30, 18],
		"curtain_repeater":	65
	},
	"report_time":	11255080
}
```

**device_id**: required, unique ID of device, string. Example:`LY0123456789`

**message_type**: required, message type of this message, string. valid
value see the table below:

|Value|Description|Direction|
|-----|-----------|---------|
| update | update device status by first time connect on server| device ---> server|
| action | command from server| server ---> device |
| get | get device status | server ---> device |
| set | set device parameters | server ---> device |

**device_params**: required, include all device current status, json object.
1. **battery**: remain quantity of electricity of battery, int;
2. **optical_sensor_status**: optical sensor status, 0: disable, 1: enable, boolean.
3. **lumen**: lumen percent from max brightness to dark.
4. **curtain_position**: position of curtain, percent, full open dedicate 100%.
5. **server_ip**: IPv4 string of server IP, string.
6. **server_port**: server port, int.
7. **optical_work_time**: optical sensor work time, `[end_hour, end_minute, end_second, start_hour, start_minute, start_second]`, int array.
8. **curtain_work_time**: curtain auto open or close time, coding format are the same with `optical_work_time`.
9. **curtain_repeater**: This field dedicate that which day in a week does the curtain auto close or open. This field is a bitwise flags. See the table below for details:

|Binary Value| Day |
|---|---|
|0000 000**1**| Sunday |
|0000 00**1**0| Monday |
|0000 0**1**00| Tuesday |
|0000 **1**000| Wednesday |
|000**1** 0000| Thursday |
|00**1**0 0000| Friday |
|0**1**00 0000| Saturday |

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

**message_type**: This field must be a fixed value:`get`.

**device_params**: **`all`** **OR** contains device status fields[`battery`, `optical_sensor_status`, `lumen`, `curtain_position`, `server_ip`, `server_port`, `optical_work_time`, `curtain_work_time`, `curtain_repeater`]. Set the value of `all` to get all the parmas from device. Get one or more just set the value of param name, and seperate with a "|". The seperate symbol is not necessary, and it just for friendly readable. In fact seperate symbol can be any what you like.

Example Response:
```json
{
	"device_id":	"LY0123456789",
	"message_type":	"update",
	"device_params":	{
		"battery":	0,
		"optical_sensor_status":	0,
		"lumen":	11,
		"curtain_position":	10,
		"server_ip":	"192.168.2.114",
		"server_port":	4567,
		"optical_work_time":	[0, 10, 11, 0, 10, 13],
		"curtain_work_time":	[0, 30, 8, 0, 30, 18],
		"curtain_repeater":	65
	},
	"report_time":	11255080
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

Example Response:
```json
{
    "device_id": "LY0123456789",
    "message_type": "update",
    "device_params": {
        "battery": 80,
        "curtain_position": 50
    },
    "report_time": 1123349289
}
```

#### 3. Set device params
Param list are used for device normally work, and some of them was dedicate the device status. So, **NOT** all the params can be set by the remote terminal or other method, such as `battery` & `lumen`. It was be detect by sensor on device or PCB board. If a unavailable param setting required by external method, it will raise a error tips. 

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
In most cases, a command was an **Async** task. So, the response could not promise that the task has been executed well all the time.

Example 2: Run a defined command
```json
{
    "device_id": "LY0123456789",
    "message_type": "action",
    "command": "say_hello",
    "args": "some args"
}
```
**command**: required, pre-defined command name, string.

**args**: optional, this field are used by command. If a `command` need `args`, then this field is required, otherwise it is optional.

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
