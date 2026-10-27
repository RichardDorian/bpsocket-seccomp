# `bpsocket-seccomp`

This is a proof of concept to demonstrate the possibility of using the `seccomp` feature of the Linux kernel to emulate system calls.

Right now, this poc only returns `ENOSYS` (not implemented error) for `socket`, `bind`, `recvfrom` and `sendto` system calls but the
success value can be freely set like I do for the error.

The goal is for Bundle Protocol v7 to have a POSIX compatible API. This poc may be used to make programs believe there is support
for the Bundle Protocol at the kernel level (which there is not and probably won't be since the daemon runs in userspace).

Since we're emulating system calls, this works for any program, no matter the lib, even if the binary is statically linked.

## Possibilities

| Name                                   | Type       | Launcher | Socket API | Description                                                                                               | Example            |
| -------------------------------------- | ---------- | -------- | ---------- | --------------------------------------------------------------------------------------------------------- | ------------------ |
| Launcher process + built-in supervisor | Daemonless | Yes      | Yes        | `bprun` forks, installs the `seccomp` filter and execs the target. Parent process is the supervisor.      | `bprun ./app arg1` |
| Launcher process + supervisor daemon   | Daemon     | Yes      | Yes        | `bprun` installs the `seccomp` filter, execs the target. Daemon is the supervisor                         | `bprun ./app arg1` |
| Bootstrap code + built-in supervisor   | Daemonless | No       | Yes        | The bootstrap code (as a lib) does what `bprun` does in daemonless mode.                                  | `./app arg1`       |
| Bootstrap code + supervisor daemon     | Daemon     | No       | Yes        | The bootstrap code (as a lib) does what `bprun` does in daemon mode.                                      | `./app arg1`       |
| `libbpsocket` directly                 | Daemonless | No       | No         | The program doesn't install the `seccomp` filter and uses `libbpsocket` instead of the libc/system calls. | `./app arg1`       |

The launcher process/bootstrap code can support both daemon and daemonless modes at the same time and be configured through environment variables (enabling all in on one mode/fallback).

> `libbpsocket` doesn't exist yet but if the seccomp idea goes through, a library will be responsible for the logic (Unix sockets, adapters to BPAs) and the seccomp shim will use it.

This proof of concept is a very incomplete implementation of the 1st possibility (launcher process + built-in supervisor).

## Daemon or daemonless?

`seccomp` requires another process (the supervisor) to receive and handle filtered system calls (`SECCOMP_RET_USER_NOTIF` action).

There are two ways of doing this:

- A single long living process (a daemon) for **all** processes.
- A supervisor process for each process that starts and stops at the same time the process does (daemonless).

It is also possible to do a combination of the two to have the advantages of the two at the same time. The launcher process or the bootstrap
library can connect to the daemon if available and fallback to the built-in supervisor (or vice versa).
