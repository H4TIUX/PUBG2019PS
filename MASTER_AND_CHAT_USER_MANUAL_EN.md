# PUBG 2019: Master Server, Server Advertising and Chat Server

This guide describes the PUBG 2019 launcher. The game, the account service and an Internet server are not included. The master server and the chat server are two separate programs. The master lists rooms; the chat server carries global messages. Neither relays game traffic.

## How a player-hosted server becomes visible

1. The host selects NET, signs into the account service and creates a room with **List publicly** enabled.
2. The host launcher sends a temporary advertisement to the first configured master. It includes the room name, map, mode, player counts, room code and ports. The master verifies the account session, derives the reachable address from the incoming connection and retains the listing while updates continue.
3. Other launchers poll all configured masters and combine the results. A stopped or stale host disappears after the master's expiry period (currently 30 seconds). A private room is not listed publicly.
4. A player selects a room and connects directly to the host. A listing does not prove that the host's room and game ports are reachable.

The first master is used for hosting and chat; the client reads listings from up to eight masters. Configure the addresses in **Settings → Master Servers**, one per line. A hostname or IPv4 address without a port uses 18900. The chat endpoint uses the same hostname and scheme, with the master's port plus one: for example, `http://master.example:18900/` implies `http://master.example:18901/`. If several masters are configured, there is one global chat for the first master only. Masters do not automatically exchange lists or chat messages.

## Running the master and chat services

On the external machine, install the launcher's supporting files. If using the launcher's **Dedicated** tab, start **Master** and **Chat Server** separately. They may run on the same external machine, which does not have to be a player's home computer. The master executable is `NetDirectory/PUBG2019NetDirectory.exe`; the chat executable is `ChatServer/ChatServer.exe`.

For manual operation, the programs accept `--bind 0.0.0.0 --port 18900` for master and `--bind 0.0.0.0 --port 18901` for chat. The account verification endpoint can be overridden with `--auth-url`; the account service itself must be available. Keep services supervised or restart them after a reboot. The launcher shows process status for services it started; that status does not prove public Internet reachability.

A host on a router needs inbound forwarding and firewall permissions:

| Machine | Protocol/port | Purpose |
| --- | --- | --- |
| Master machine | TCP 18900 | Advertise and list rooms |
| Chat machine | TCP 18901 | Global chat and presence |
| Normal game host | TCP 18887 | Launcher room connection and room chat |
| Normal game host | UDP 8888 | Game traffic |
| Parallel dedicated host | TCP and UDP 8000–10000 | Allocated room and game ports |

Open only the ports appropriate to the machine. Regular players normally need outbound connections only. The supplied Configure-Firewall.ps1 helps configure game-host rules; it does not configure the external master or chat machine, router or VPS firewall.

## Chat behaviour

After NET login, the launcher contacts the Chat Server derived from the first master address. The Chat Server checks the session with the account service and obtains the displayed username from it. Global messages and presence are held in memory; messages are not durable and disappear when the chat server restarts. Room chat remains separate. A dedicated room host does not participate in chat. A disconnected chat service does not stop server advertising; the launcher reports that the global chat is unavailable and retries.

## Security and troubleshooting

The current default endpoints use HTTP. Account session credentials sent over plain HTTP can be observed on the network. For Internet use, provide HTTPS for the master and chat endpoints, with trusted certificates and reverse proxies configured for both ports; configure the master as HTTPS in the launcher. The Chat Server itself listens in plain HTTP and requires a TLS reverse proxy for HTTPS. The account service's transport must be secured separately. Do not publish account passwords, session credentials, or private launcher keys.

If the list is empty, check NET login, the configured master, that the master is running and TCP 18900 is reachable, and that the host enabled public listing. If a room is visible but cannot be joined, check its host TCP/UDP ports and router forwarding. If chat says disconnected, check TCP 18901, the chat process, the account service and the derived host and port. A locally running test or process status is not a substitute for testing from another Internet connection.

## HTTPS reverse proxy example

`NetDirectory/deployment/nginx-master-chat.conf.example` shows an IPv4-only Nginx configuration serving the master on HTTPS port 443 and chat on HTTPS port 444. Replace `example.com` and both certificate paths; obtain and maintain a valid certificate for the domain before enabling it. Set the launcher master address to `https://example.com/`: it derives port 444 for chat.

Run the included updated directory with `--bind 127.0.0.1 --port 18900 --trust-loopback-proxy` and the Chat Server with `--bind 127.0.0.1 --port 18901`. Allow public TCP 443 and 444 to the proxy, but do not expose backend ports 18900/18901. The proxy must replace (not append) `X-PUBG-Client-IP` with `$remote_addr`. The updated directory trusts that header only with the explicit flag and refuses to enable the flag on a non-loopback bind. This preserves the connecting host's address in advertisements; using the older directory behind a reverse proxy would otherwise advertise the proxy's address. Local processes on the same server can access loopback; only trusted services should run there.

The launcher's Start Master button uses its normal direct-binding mode; it does not set up this HTTPS proxy. Use a service manager and the explicit commands above for proxy mode. This example secures the launcher-to-master/chat links only. Set each service's `--auth-url` to a separately secured HTTPS account endpoint if one is available; this package cannot enable HTTPS on an externally controlled account service. The example was not deployed to a public server and needs a real domain, valid certificate, Nginx and Internet reachability checks.
