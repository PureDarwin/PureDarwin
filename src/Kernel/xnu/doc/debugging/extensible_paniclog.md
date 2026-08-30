# Extensible Paniclog

This documentation discusses the API and features of the extensible paniclog in XNU's panic flow.

## Overview

With this feature we want to provide an infrastructure for kexts / dexts to insert their system state into the paniclog. Currently there is no way of knowing the kext or dext state unless we take a full coredump. With this feature, they can drop relevant state information that will end up in the paniclog and can be used to triage panics.

## UUID ↔ buffer data mapping

All clients who adopt this infrastructure will have to use a UUID that maps to a format of the buffer data. Clients will have to provide a mapping that specifies how to decode the data. This mapping will be used to decode the data in DumpPanic or a tool integrated into MPT.

## IOKit APIs

Source Code: `iokit/IOKit/IOExtensiblePaniclog.h`

```c
static bool createWithUUID(uuid_t uuid, const char *data_id, uint32_t max_len, ext_paniclog_create_options_t options, IOExtensiblePaniclog **out);
```

This is the first API that is called by a kext to initialize an IOExtensiblePaniclog instance. It takes a UUID, data_id, max len, and options as input and emits an instance in the out pointer. The data id takes a short description of the buffer and the maximum length is 32 bytes.

```c
int setActive();
int setInactive();
```

These functions are called to make an IOExtensiblePaniclog instance active or inactive. An instance is collected and put into the panic file only if it's active. It's ignored in the panic path if it's inactive.

```c
int insertData(void *addr, uint32_t len);
```

This function inserts the data pointed to by addr into the IOExtensiblePaniclog instance. It will copy the data into the buffer from offset 0.

```c
int appendData(void *addr, uint32_t len);
```

This function appends the data pointed to by addr into the IOExtensiblePaniclog instance. It will position the data after the previous insert or append.

```c
void *claimBuffer();
```

This function returns the buffer of the IOExtensiblePaniclog instance. This function also sets the used length of the handle to the max length. The entire buffer is copied out when the system panic after this function call. yieldBuffer() has to be called before using insertData() or appendData(). 

```c
int yieldBuffer(uint32_t used_len);
```

This function is called to yield the buffer and set the used_len for the buffer.

```c
int setUsedLen(uint32_t used_len)
```

This function is called to set the used len of the buffer.

## DriverKit APIs

Source Code: `iokit/DriverKit/IOExtensiblePaniclog.iig`

```cpp
static kern_return_t Create(OSData *uuid, OSString *data_id, uint32_t max_len, IOExtensiblePaniclog **out);
```

This is first API that is called by a dext to initialize an IOExtensiblePaniclog instance. It takes a UUID, data_id and the max len as input and emits an instance in the out pointer. The data id takes a short description of the buffer and the maximum length is 32 bytes.

```cpp
kern_return_t SetActive();
kern_return_t SetInactive();
```

These functions are called to make an IOExtensiblePaniclog instance active or inactive. An instance is collected and put into the panic file only if it's active. It's ignored in the panic path if it's inactive.

```cpp
kern_return_t InsertData(OSData *data);
```

This function inserts the data pointed to by addr into the IOExtensiblePaniclog instance. It will copy the data into the buffer from offset 0.

```cpp
kern_return_t AppendData(OSData *data);
```

This function appends the data pointed to by addr into the IOExtensiblePaniclog instance. It will position the data after the previous insert or append.

```cpp
kern_return_t ClaimBuffer(uint64_t *addr, uint64_t *len);
```

This function is called to get a pointer to the ext paniclog buffer. After this function is called, the user is responsible for copying data into the buffer. The entire buffer is copied when a system panics. After claiming the buffer, YieldBuffer() has to be called to set the used_len of the buffer before calling InsertData() or AppendData().

```cpp
kern_return_t YieldBuffer(uint32_t used_len);
```

This function is called to yield the buffer and set the used_len for the buffer.

```cpp
kern_return_t SetUsedLen(uint32_t used_len);
```

This function is called to set the used len of the buffer.

## Low-Level Kernel APIs

Source Code: `osfmk/kern/ext_paniclog.h`

### ExtensiblePaniclog Handle Struct

```c
typedef struct ext_paniclog_handle {
	LIST_ENTRY(ext_paniclog_handle) handles;
	uuid_t uuid;
	char data_id[MAX_DATA_ID_SIZE];
	void *buf_addr;
	uint32_t max_len;
	uint32_t used_len;
    ext_paniclog_create_options_t options;
    ext_paniclog_flags_t flags;
	uint8_t active;
} ext_paniclog_handle_t;
```

We employ handles in XNU to guarantee the effective management of buffer lifecycles, prevent nested panics from occurring during access from the panic path, and build a durable and expandable API. The primary reason for using handles is to allow XNU to oversee the entire buffer lifecycle. By keeping track of the buffer's state and managing its deallocation, we can avoid potential issues that may arise during panic scenarios.

```c
ext_paniclog_handle_t *ext_paniclog_handle_alloc_with_uuid(uuid_t uuid, const char *data_id, uint32_t max_len, ext_paniclog_create_options_t);
```

This function will be called to initialize a buffer of the specified length. For all subsequent operations we use this handle as input. It takes a UUID, data_id, max len, and options as input and emits an instance in the out pointer. The data id takes a short description of the buffer and the maximum length is 32 bytes. This function will return a handle on success and NULL on failure.

```c
int ext_paniclog_handle_set_active(ext_paniclog_handle_t *handle);
```

This function sets the handle as active. In active state, this buffer will get picked up by the panic path and put into the panic file.

```c
int ext_paniclog_handle_set_inactive(ext_paniclog_handle_t *handle);
```

This function sets the handle as inactive.

```c
void ext_paniclog_handle_free(ext_paniclog_handle_t *handle)
```

This functions deallocates all the memory that is allocated in the alloc function. The handle has to a be a valid and this function should only be called after handle_alloc is called.

```c
int ext_paniclog_insert_data(ext_paniclog_handle_t *handle, void *addr, size_t len)
```

This function is called to insert the data from a buffer to the handle buffer. This function will take a handle that has been previously allocated, an address to the buffer and length of the buffer. This function will return 0 on success and a negative value on failure.

```c
int ext_paniclog_append_data(ext_paniclog_handle_t *handle, void *addr, uint32_t len);
```

This function is called to append to the data that is already present in the buffer.

```c
void *ext_paniclog_get_buffer(ext_paniclog_handle_t *handle)
```

This function is called to get a pointer to the ext paniclog buffer. To modify the buffer after getting the pointer use the `ext_paniclog_claim_buffer()`.

```c
void *ext_paniclog_claim_buffer(ext_paniclog_handle_t *handle);
```

This function is called to get a pointer to the ext paniclog buffer. After this function is called, the user is responsible for copying data into the buffer. The entire buffer is copied when a system panics. After claiming the buffer, `ext_paniclog_yield_buffer()` has to be called to set the `used_len` of the buffer before calling `ext_paniclog_insert_data()` or `ext_paniclog_append_data()`.

```c
int ext_paniclog_yield_buffer(ext_paniclog_handle_t *handle, uint32_t used_len);
```

This function is called to yield the buffer and set the used_len for the buffer.

```c
int ext_paniclog_set_used_len(ext_paniclog_handle_t *handle, uint32_t used_len);
```

This function is called to set the used len of the buffer.

## panic_with_data APIs

```c
void panic_with_data(uuid_t uuid, void *addr, uint32_t len, uint64_t debugger_options_mask, const char *format, ...);
```

This function is called when a kernel client is panicking and wants to insert the data into the extensible panic log. We treat this as a special case and put this data at the start of the extensible panic log region. The client has to supply the UUID to decode the buffer that is pushed to the paniclog.

```c
int panic_with_data(char *uuid, void *addr, uint32_t len, uint32_t flags, const char *msg);
```

This provides the same functionality as panic_with_data() for userspace clients.

## Special Options

### `EXT_PANICLOG_OPTIONS_ADD_SEPARATE_KEY`

If the `EXT_PANICLOG_OPTIONS_ADD_SEPARATE_KEY` option is set when creating an ExtensiblePaniclog handle, the Data ID / buffer data (key / value) pair will be added directly to the paniclog instead of under the "ExtensiblePaniclog" key.

## Implementation

### Estimating the panic log size

We want to add the utilization metrics of the panic log to the panic.ips file. This will give us an idea of the percentage of the panic log we currently use and how big each section in the panic log is. We will use this data to estimate how big the other log section usually is and ensure that we leave enough space for this section when inserting the extensible panic log. We will cut off the extensible panic log if we cannot fit all the buffers into the free region.

### Registering a buffer + Writing data to the buffer

We have APIs exposed at different layers so that a client can use whatever suits it best. In DriverKit and IOKit cases, they call the `createWithUUID` or `Create` methods to create an IOExtensiblePaniclog instance and use that instance to insert or append data to a buffer.

Lower level clients use `ext_paniclog_handle_alloc_with_uuid` to allocate a handle and use that handle to insert data using `ext_paniclog_insert_data` and `ext_paniclog_append_data` functions.

When a kernel client is panicking, it has the option to call `panic_with_data()`, which just takes a UUID, buffer address and length. This API makes sure that we copy this data in to the extensible panic log.

### Insert data into the extended panic log

Current structure of the panic log is as follows:

```
-------------------------
-      Panic Header     -
-------------------------
-                       -
-       Panic Log       -
-                       -
-------------------------
-                       -
-      Stack shots      -
-                       -
-------------------------
-                       -
-       Other Log       -
-                       -
-------------------------
-       Misc Data       -
-------------------------
-                       -
-                       -
-         Free          -
-                       -
-                       -
-------------------------
```

We want to use the free part of the panic log to insert the extensible panic log. After we insert the stackshots, we calculate and see how much space we have in the panic log to insert the extensible panic log. These calculations will use the data that we collect from our utilization metrics and leave out space for the other log section. We then go through the ext_paniclog linked list and start inserting the buffers into the panic log region until we fill out size we calculated. After this, we move onto inserting data into the other log section.

## Format / structure of the extensible panic log:

```
+---------+------------+---------+---------+------------+------------+---------+---------+---------+-----------+------------+----------+
|         |            |         |         |            |            |         |         |         |           |            |          |
|Version  | No of logs | UUID 1  | Flags 1 | Data ID 1  | Data len 1 | Data 1  | UUID 2  | Flags 2 | Data ID 2 | Data len 2 | Data 2   |
|         |            |         |         |            |            |         |         |         |           |            |          |
+---------+------------+---------+---------+------------+------------+---------+---------+---------+-----------+------------+----------+
```

## Extract and format the extensible panic log into the panic.ips file

In DumpPanic, we will extract this data from the panic log region and format it to be readable. We can group the data according to uuid and sort it with the data_id of the data. An example of the extensible panic log data in the panic.ips file shown below.

```
{
    "ExtensiblePanicLog": {
        "<UUID_1>": [
            {
                "DataID": "0x1"
                "data" : <buffer1>
            },
            {
                "DataID": "0x2"
                "data" : <buffer2>
            }
        ],
        "<UUID_2>": [
            {
                "DataID": "0x1"
                "data" : <buffer1>
            },
            {
                "DataID": "0x2"
                "data" : <buffer2>
            }
        ],
    },
    "SeparateFieldDataID1": "Separate buffer value here 1",
    "SeparateFieldDataID2": "Separate buffer value here 2",
}
```

Notice that there are two fields below ExtensiblePanicLog in the panic.ips example above. If you were to pass the option `EXT_PANICLOG_CREATE_OPTIONS_ADD_SEPARATE_KEY` to the handle create function, DumpPanic would process that handle as seen above, by adding it as a field directly to the panic file instead of including it in the ExtensiblePanicLog field.

## Example code

### IOKit Example

#### Creating the handle

```c
char uuid_string_1[] = "E2070C7E-A1C3-41DF-ABA4-B9921DACCD87";
bool res;
kern_return_t ret;
 
uuid_t uuid_1;
uuid_parse(uuid_string_1, uuid_1);
 
res = IOExtensiblePaniclog::createWithUUID(uuid_1, "Lha ops 1", 1024, EXT_PANICLOG_OPTIONS_NONE, &paniclog_handle_1);
if (res == false) {
    DEBUG_LOG ("Failed to create ext paniclog handle: %d\n", res);
}
 
DEBUG_LOG("Created panic log handle 1 with UUID: %s\n", uuid_string_1);
 
char uuid_string_2[] = "28245A8F-04CA-4932-8A38-E6C159FD9C92";
uuid_t uuid_2;
uuid_parse(uuid_string_2, uuid_2);
res = IOExtensiblePaniclog::createWithUUID(uuid_2, "Lha ops 2", 1024, EXT_PANICLOG_OPTIONS_NONE, &paniclog_handle_2);
if (res == false) {
    DEBUG_LOG ("Failed to create ext paniclog handle: %d\n", res);
}
 
DEBUG_LOG("Created panic log handle 2 with UUID: %s\n", uuid_string_2);
```

#### Inserting the data

```c
DEBUG_LOG ("%s\n", __FUNCTION__);
char buff[1024] = {0};
snprintf(buff, 1024, "HW access Dir: %u Type: %u Address: %llu\n", input->direction, input->type, input->address);
    
char buff1[1024] = {0};
    
paniclog_handle_1->insertData(buff, (uint32_t)strlen(buff));
paniclog_handle_1->setActive();

paniclog_handle_2->insertData(input, sizeof(HardwareAccessParameters));
paniclog_handle_2->setActive();
```

### DriverKit Example

#### Creating the handle

```cpp
OSData *uuid_data = OSData::withBytes(&uuid_3[0], sizeof(uuid_t));
if (!uuid_data) {
    IOLog("Data was not created\n");
    return NULL;
}
 
OSString *data_id = OSString::withCString("DriverKit OP 1");

ret = IOExtensiblePaniclog::Create(uuid_data, data_id, 64, kIOExtensiblePaniclogOptionsNone, &paniclog_handle_3);
if (ret != kIOReturnSuccess) {
    IOLog("Failed to create paniclog handle 3\n");
    return NULL;
}
IOLog("EXT_PANICLOG: Created panic log handle 3 with UUID: %s\n", uuid_string_3);
```

#### Inserting the data

```cpp
ret = paniclog_handle_3->ClaimBuffer(&addr, &len);
if (ret != kIOReturnSuccess) {
    IOLog("EXT_PANICLOG: Failed to claim buffer. Ret: %x\n", ret);
    return NULL;
}
 
IOLog("EXT_PANICLOG: Got buffer address %llu, %llu", addr, len);
 
buff1 = (char *)addr;
 
IOLog("EXT_PANICLOG: Ignoring write for now");
memcpy(buff1, buff, strlen(buff));
 
paniclog_handle_3->YieldBuffer((uint32_t)strlen(buff));
 
paniclog_handle_3->SetActive();
```

