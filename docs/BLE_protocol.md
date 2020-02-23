## Intelligent Curtain BLE Protocol

**BLE Service: uuid**

**Characteristic: uuid**

**Write/Read/Notify**

>Notice

#### 1. Data Format
Data package consist with: 1 byte header + 1 byte command + 1 byte index +
17(max) bytes data.


| Segment | Content | Length |
| --- | --- | ---|
| Header | 0x55 | 1 |
| Command | 0x00: System Command, 0x01: Read Data, 0x02: Setting | 1 |
| Index | | 2 |
| Data | | 3 ~ 19 |

#### 2. Commands

##### 2.1 Curtain adjust

Start Adjust:

| 0 | 1 | 2 |
| --- | --- | --- |
| Header | Command | Index |
| 0x55 | 0x00 | 0x01 |

Response:

| 0 | 1 | 2 |
| --- | --- | --- |
| Header | Command | Index |
| 0x55 | 0x00 | 0x01 |

##### 2.2 Device reboot

Start Adjust:

| 0 | 1 | 2 |
| --- | --- | --- |
| Header | Command | Index |
| 0x55 | 0x00 | 0x02 |

Response:

| 0 | 1 | 2 |
| --- | --- | --- |
| Header | Command | Index |
| 0x55 | 0x00 | 0x02 |

##### 2.3 Set server IP & port

Start Adjust:

| 0 | 1 | 2 | 3~6 | 7~8 |
| --- | --- | --- | --- | --- |
| Header | Command | Index | IP | Port |
| 0x55 | 0x00 | 0x03 | IP address | port |

Response:

| 0 | 1 | 2 | 3~6 | 7~8 |
| --- | --- | --- | --- | --- |
| Header | Command | Index | IP | Port |
| 0x55 | 0x00 | 0x03 | IP address | port |

##### 2.4 Read server IP & port

Start Adjust:

| 0 | 1 | 2 | 3~6 | 7~8 |
| --- | --- | --- | --- | --- |
| Header | Command | Index | IP | Port |
| 0x55 | 0x00 | 0x04 | IP address | port |

Response:

| 0 | 1 | 2 | 3~6 | 7~8 |
| --- | --- | --- | --- | --- |
| Header | Command | Index | IP | Port |
| 0x55 | 0x00 | 0x04 | IP address | port |

##### 2.5 Read device status

Start Adjust:

| 0 | 1 | 2 |
| --- | --- | --- |
| Header | Command | Index |
| 0x55 | 0x01 | 0x01 |

Response:

| 0 | 1 | 2 | 3~8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Header | Command | Index | Device ID | Battery | Battery Temperature| Battery Status | Curtain ratio | Optical status | Lumin | Mode | Light Gate |
| 0x55 | 0x01 | 0x01 | 6 bytes Device ID | 0~100 | 1 byte | 0x00, 0x01, 0x02 | 0~100 | 0x00, 0x01 | 0~100 | 0x01, 0x02, 0x00 | 0~100 |

##### 2.6 Set time

Start Adjust:

| 0 | 1 | 2 | 3~4 | 5 | 6 | 7 | 8 | 9 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Header | Command | Index | Year | Month | Day | Hour | Minute | Second
| 0x55 | 0x02 | 0x01 | 2 bytes | | | | | |

Response:

| 0 | 1 | 2 | 3 |
| --- | --- | --- | --- |
| Header | Command | Index | Data |
| 0x55 | 0x02 | 0x01 | 0x01 |
