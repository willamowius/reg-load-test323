# reg-load-test323

A registration load tester for H.323 gatekeepers.

`reg-load-test` creates a configurable number of H.323 endpoints and registers each of them with a gatekeeper.
The endpoints stay registered (keeping their registrations alive) until the program is stopped, at which point they all unregister cleanly.

Built on [PTLib](https://github.com/willamowius/ptlib) and [H323Plus](https://github.com/willamowius/h323plus).

## Building

Build and install PTLib and H323Plus first, then:

```sh
make optnoshared
```

The Makefile looks for H323Plus in `$OPENH323DIR`, defaulting to `~/h323plus`:

```sh
OPENH323DIR=/path/to/h323plus make optnoshared
```

The binary is placed in the `obj_*` directory created by the build.

## Usage

```sh
reg-load-test -g <gatekeeper> [options]
```

| Option | Description | Default |
|---|---|---|
| `-g`, `--gatekeeper <addr>` | Gatekeeper address (required) | – |
| `-c`, `--count <n>` | Number of endpoints to register (1–1000) | 1 |
| `-d`, `--delay <ms>` | Delay between registrations in milliseconds | 100 |
| `-u`, `--usernameprefix <prefix>` | Prefix for endpoint aliases | `ep` |
| `-s`, `--servername <name>` | Name identifying this tester instance in aliases | random 20-letter string |
| `-p`, `--password <pw>` | Gatekeeper password (H.235) for all endpoints | – |
| `-i`, `--interface <ip[:port]>` | Local interface to bind to, optionally with base port | all interfaces |
| `-b`, `--baseport <port>` | Base TCP port for the H.323 listeners | random |
| `-t`, `--trace` | Enable tracing; repeat for more detail (`-ttt`) | off |
| `-o`, `--output <file>` | Write trace output to a file | stderr |
| `--h46018enable` | Enable H.460.18 (only if H323Plus was built with H.460.18 support) | off |

IPv6 addresses for `-i` can be given with or without brackets, eg. `[2001:db8::1]:20000`.

### Endpoint aliases

Each endpoint registers with the alias

```
<usernameprefix>_<servername>_<n>
```

where `n` runs from 1 to the endpoint count, eg. `ep_loadhost1_1`, `ep_loadhost1_2`, … The server name keeps aliases unique when running several instances against the same gatekeeper; if you don't set one, a random name is generated on each start.

### Listener ports

Every endpoint needs its own TCP listener, even though no calls are ever made. Endpoint *n* listens on base port + *n*, so make sure that range is free. When running several instances on the same host, give each one a different base port with `-b` or `-i`.

### Stopping

Press Ctrl-C (or send `SIGTERM` / `SIGQUIT`). All registered endpoints unregister from the gatekeeper before the program exits.

## Examples

Register 500 endpoints, 50 ms apart:

```sh
reg-load-test -g 192.168.1.10 -c 500 -d 50
```

Two instances on the same host, running on different port ranges:

```sh
reg-load-test -g gk.example.com -c 1000 -b 20000
reg-load-test -g gk.example.com -c 1000 -b 30000
```

Authenticated registrations with H.460.18 and a trace file:

```sh
reg-load-test -g gk.example.com:1719 -c 100 -p secret --h46018enable -ttt -o trace.log
```

## License

GNU General Public License v3.0 – see [LICENSE](LICENSE).

Copyright (c) 2026 Jan Willamowius
