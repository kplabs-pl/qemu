# KP Labs POSIX Device
## Introduction
POSIX Device provides memory-mapped region that allows emulated guest to execute I/O operations on host machine. Operations are designed similary to ARM Semihosting specification with aim to be as architecture-agnostic as possible - there should be no assembly required.

## Usage
Device can be added to QEMU instance by specifying `-device kp-posix` argument. 

There are two device properties that can be used to customize POSIX Device behavior:
* `addr` - base address in guest address space where POSIX Device will be mapped (default: `0xF0040010`)
* `chardev` - id of character device used as debug out channel (standard error if not specified)

## Registers
Two registers are available, 32-bit wide access only:
* `+0` - ID register, read-only, always reads as 0x50534958
* `+4` - Command register, write-only, executes command on write.

## Invoking operation
Commands are executed by writing address of command block into Command Register. Commands are executed synchronously as part of write operation, next guest CPU instruction is executed after command is fully executed.

Command Block is a buffer of 8 32-bit values:
* `commandBlock[0]` - command code, specifies operation to execute
* `commandBlock[1..6]` - 7 registers (`R0`..`R6`) used to pass input arguments to host machine and return values to guest.

Values is command block should use endianess native to guest CPU. It is not necessary to initialize all registers if they are not used by executed operation.

In multithreaded environments it is safe to use POSIX device assuming command block is not accessed concurrently by different threads.

Invoking unknown command is reported as guest error and return values are undefined.

Typical usage looks like:
```
uint32_t block[8];
block[0] = CommandCode;
block[1] = Arg0;
block[2] = Arg1;
POSIX->Command = &block;
inspect_return_values(block[1], block[2]);
```


## Operations

### `0x00` - Get available operations
TODO

### `0x01` - Exit
Terminates QEMU process with specified error code. Write to Command Register will be the last instruction executed by guest CPU.

| Register | Input     | Output |
|----------|-----------|--------|
| R0       | Exit code | -      |

### `0x02` - Output debug string
Outputs provided buffer to configured debug character device or standard error output if chardev is not specified. Buffer does not need to be NUL-terminated.

| Register | Input                  | Output |
|----------|------------------------|--------|
| R0       | Buffer address         | -      |
| R1       | Buffer length in bytes | -      |

### `0x03` - Open file
Opens file on host machine using specified mode.

| Register | Input                     | Output                       |
|----------|---------------------------|------------------------------|
| R0       | File name buffer address  | Error code (0 for success)   |
| R1       | File name length in bytes | File descriptor (0 on error) |
| R2       | Open mode                 | -                            |

Open modes are defined exactly like ARM Semihosting `SYS_OPEN` call:

| Mode | `fopen` mode | `open` mode                                |
|------|--------------|--------------------------------------------|
| 0    | `r`          | `O_RDONLY`                                 |
| 1    | `rb`         | `O_RDONLY | O_BINARY`                      |
| 2    | `r+`         | `O_RDWR`                                   |
| 3    | `r+b`        | `O_RDWR | O_BINARY`                        |
| 4    | `w`          | `O_WRONLY | O_CREAT | O_TRUNC`             |
| 5    | `wb`         | `O_WRONLY | O_CREAT | O_TRUNC | O_BINARY`  |
| 6    | `w+`         | `O_RDWR | O_CREAT | O_TRUNC`               |
| 7    | `w+b`        | `O_RDWR | O_CREAT | O_TRUNC | O_BINARY`    |
| 8    | `a`          | `O_WRONLY | O_CREAT | O_APPEND`            |
| 9    | `ab`         | `O_WRONLY | O_CREAT | O_APPEND | O_BINARY` |
| 10   | `a+`         | `O_RDWR | O_CREAT | O_APPEND`              |
| 11   | `a+b`        | `O_RDWR | O_CREAT | O_APPEND | O_BINARY`   |

Reserved file name `:tt` allows guest CPU to access QEMU standard output and standard error:

| File name | Modes     | QEMU output     |
|-----------|-----------|-----------------|
| `:tt`     | 4,5,6,7   | Standard output |
| `:tt`     | 8,9,10,11 | Stnardard error |

Using `:tt` with any other open mode will be reported to guest as `EBADF` error.


### `0x04` - Write file
Writes buffer to file at current position and advances position by number of written bytes.

| Register | Input                  | Output                     |
|----------|------------------------|----------------------------|
| R0       | File descriptor        | Error code (0 for success) |
| R1       | Buffer address         | Number of bytes written    |
| R2       | Buffer length in bytes | -                          |

### `0x05` - Read fle
Reads from file into buffer at current position and advances position by number of read bytes

| Register | Input                  | Output                     |
|----------|------------------------|----------------------------|
| R0       | File descriptor        | Error code (0 for success) |
| R1       | Buffer address         | Number of bytes read       |
| R2       | Buffer length in bytes | -                          |

### `0x06` - Seek file
Sets absolute position in file.

| Register | Input                      | Output                     |
|----------|----------------------------|----------------------------|
| R0       | File descriptor            | Error code (0 for success) |
| R1       | Absolute position in bytes | -                          |

### `0x07` - Close file
Closes file descriptor.

| Register | Input           | Output                     |
|----------|-----------------|----------------------------|
| R0       | File descriptor | Error code (0 for success) |

Closing `:tt` file will invalidate file descriptor but will not close QEMU's stdout/stderr files.

### `0x08` - Remove file
Removes file.

| Register | Input                     | Output                     |
|----------|---------------------------|----------------------------|
| R0       | File name buffer address  | Error code (0 for success) |
| R1       | File name length in bytes | -                          |


### `0x09` - Get command line arguments
Gets the arguments specified on the command line.

| Register | Input                      | Output               |
|----------|----------------------------|----------------------|
| R0       | Buffer address             | Number of bytes read |
| R1       | Buffer length in bytes     | -                    |
| R2       | Offset in arguments string | -                    |
