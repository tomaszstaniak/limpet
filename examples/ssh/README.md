# Native SSH integration preview

This is a mainline x86_64 experiment, not a production SSH distribution.
`client.c` derives from AROS `workbench/network/sshd/ssh.c` (copyright notice
retained). The client uses normal DOS input/output handles plus the optional
[XTerm status query](../../docs/ssh-stream-contract.md) for EOF and geometry.
libssh supplies SSH, and the remote server supplies the PTY.
This does **not** implement a local AROS POSIX PTY.

## Build

Run `sh tools/build-ssh.sh` from the project root. The script downloads pinned
libssh 0.12.2 from its official release site, checks the archive SHA-256, applies
the local home-directory fallback patch and builds against mainline's existing
OpenSSL. `AROS_GCC` and `AROS_SDK` can select matching tools/SDKs. Output:
`build/ssh/ssh` and `build/ssh/check-transfer`. No SDK or mainline source files
are overwritten. The terminal
worker must also be rebuilt (`make package`) for native status-query support.

The platform adapter supplies native socket library calls and the SDK's
`select` to `WaitSelect` mapping. The SDK omits the public `inet_pton` macro,
so the adapter calls the implemented Roadshow vector 101. Interface-name lookup
uses Miami's API. If no HOME is configured, libssh uses the full program-directory
path for its default path expansion; no environment variable is changed.

PIC must be disabled: the first linked binary contained GOT relocations and was
rejected by the native loader. The build now rejects GOT/TLS relocation types.
Compilation of libssh treats implicit function declarations and integer/pointer
conversions as errors. Upstream deprecation warnings remain.

## Isolated acceptance setup

Use a disposable mainline guest and a separate host OpenSSH instance listening
only on `127.0.0.1:22222`, not the host's normal SSH service. Generate new test
host/client keys. Configure that instance with only the test public key,
password/PAM/agent/TCP/X11 forwarding disabled, and normal strict ownership
checks. Put its authorized public key beneath an owner-controlled path rather
than a world-writable `/tmp` ancestor. Never disable host-key checking to make
the test pass.

In the disposable guest, provision:

- `RAM:xterm-ssh/client_key`: disposable private authentication key.
- `RAM:xterm-ssh/known_hosts`: the copied client's private format,
  `hostname SHA256-colon-separated-hex-fingerprint`, **not** OpenSSH's file format.
  Derive the expected fingerprint from the locally generated server public key,
  independently of the network connection.

With QEMU user networking the host is `10.0.2.2`. Start from the XTERM Shell:

```text
RAM:xterm-dos/ssh -p 22222 -l TESTUSER 10.0.2.2
```

The acceptance server uses a forced, clean `/bin/sh -i` with a controlled prompt,
not the user's interactive shell configuration. Any welcome colors/clearing
come from bytes printed by that remote test script, not screenshot editing.
Stop the private server after testing. Never package keys or credential-bearing
test ISOs with the terminal.

## Remote terminal settings

Mainline `tcgetattr()` only fills ICANON. libssh's default PTY request copied
incomplete flags/control characters and disabled remote echo and normal CR/LF
processing in the first interactive test. The client now explicitly requests
canonical input, echo, signals, CR-to-NL input, NL-to-CRLF output and standard
Ctrl-C/Ctrl-D/erase/kill characters using
[RFC 4254 terminal modes](https://www.rfc-editor.org/rfc/rfc4254#section-8).
The remote application may subsequently change its own PTY modes normally.

## Limits still requiring work

- Updated XTERM handlers supply initial geometry and resize notifications through
  polling. Old/other handlers fall back to 80x24 and WaitForChar; that fallback
  still cannot distinguish idle input from EOF.
- Local F5 EOF is recognized and sends channel EOF after buffered input drains.
  Remote PTY behavior varies: a screen-output test needed idle-grace cancellation
  (return 20), while the final captured-output test returned 0. If the peer does
  not finish, the client cancels after an idle grace period, not reporting success.
  The exhausted local Shell then closes; use a new XTERM stream to reconnect.
  Use remote `exit` or raw Ctrl-D for normal logout. Ctrl-D is a remote terminal
  character, not local stream EOF.
- The bounded pump preserves partial input/output writes and services both
  remote streams. Network loss, long-running connections, output-handler stalls
  and full-window upload stress still need broader native coverage. DOS Write
  remains synchronous; bounded buffers do not make a blocked sink asynchronous.
- Command mode does not forward local stdin; it sends channel EOF and drains
  stdout/stderr. Pipelines into remote commands are not implemented.
- Password fallback and unknown-host prompts retain upstream limitations;
  acceptance uses preprovisioned trust and public-key authentication only.
- This trust format does not distinguish ports; migrate to standard libssh
  known-host verification before providing a general-purpose client.
- SCP code is inherited, untested and outside this terminal acceptance.
- No claims for DNS/IPv6, remote full-screen editors, multiple concurrent SSH
  sessions, aarch64 or distribution compatibility from the x86_64 demo.

See `docs/ssh-feasibility.md` for recorded runtime evidence.

## Exact-byte transfer regression

`check-transfer.c` is a native acceptance driver, not an SSH feature. Compile
with the same mainline compiler/SDK as the client. With `RAM:xterm-ssh` present,
run it with one quoted command argument:

```text
check-transfer "RAM:xterm-dos/ssh -p 22222 -l TESTUSER 10.0.2.2 bulk"
```

The private server's forced `bulk` command must emit 8192 repetitions of
`0123456789abcdef0123456789abcdef\n` to stdout and
`fedcba9876543210fedcba9876543210\n` to stderr, then `STDOUT_DONE\n` and
`STDERR_DONE\n` respectively, and exit 0. The driver saves each stream separately
in `RAM:xterm-ssh/bulk-out` and `bulk-err`, verifies all 270347 bytes of each and
checks for trailing data. It replaces those two test files on each run.
