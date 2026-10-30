<<<<<<< HEAD
# sls
Secure LS
=======
# sls - remote directory listing via SSH (scp-style)

A small utility that lists the contents of a remote directory over SSH
using the same `user@host:path` syntax as `scp`.

## Author

Aki Korhonen <spammagnet@eibrak.com>

## License

- Primary: **GPL-2.0-or-later**
- Attribution also given for **LGPL-2.0-or-later** (see source header)
  for any derivative work that links against LGPL libraries.

See the copyright header in `sls.c` and the full text in `COPYING`.

## Build

```bash
gcc -Wall -Wextra -O2 -o sls sls.c
```

Or use the included Makefile:

```bash
make
make install
```

## Usage

```text
sls [ssh-options] [ls-options] [user@]host:path
sls [ssh-options] [ls-options] [user@]host
```

SSH options (same as scp, passed to `ssh`):

| Option | Meaning |
|--------|---------|
| `-P port` | Port |
| `-i identity_file` | Private key |
| `-o ssh_option` | Extra ssh_config option (repeatable) |
| `-F ssh_config` | Alternate config file |
| `-J destination` | ProxyJump |
| `-4` / `-6` | Force IPv4 / IPv6 |
| `-C` | Compression |
| `-v` | Verbose |
| `-q` | Quiet |

All other options are passed straight through to the remote `ls(1)`.

### Examples

```bash
sls -l user@example.com:/var/log
sls -P 2222 -i ~/.ssh/id_ed25519 -la host:.
sls -o StrictHostKeyChecking=no -h user@server:/home/me
sls -J bastion.example.com -la internal:/var
```

Authentication and connection settings are handled entirely by the system `ssh` client.
This means **`~/.ssh/config` is fully respected** (Port, User, IdentityFile, host aliases, ProxyJump, ControlMaster, etc.).
Command-line SSH options override the config file.

## How it works

1. Parse the target specifier exactly as `scp` does (`[user@]host[:path]`).
2. Quote the remote path for a POSIX shell.
3. Build a remote command of the form `ls [opts] 'path'`.
4. `exec` the local `ssh` binary with that command.

This follows the same remote-execution principle used by classic `scp`
(OpenSSH) without reimplementing the SSH protocol.

## Notes

- Requires a working `ssh` in `$PATH`.
- Remote side must have a POSIX-compatible `ls`.
- For recursive listings use the normal `ls` flags (`-R`).
- Exit status is the exit status of the remote `ls` (or of `ssh` on connection failure).
>>>>>>> fba3eed (initial commit)
