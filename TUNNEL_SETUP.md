# playit.gg Tunnel Setup

This guide explains how to use the integrated playit.gg tunnel to make your server publicly accessible.

## Prerequisites

You need **one** of the following installed:
- **Docker** (recommended): https://docs.docker.com/get-docker/
- **playit binary**: https://playit.gg/download

## Getting Your Secret Key

1. Go to https://playit.gg/account/agents/new-docker
2. Create a new agent and copy the secret key
3. **Save the key** - you'll need it to start the tunnel

## Using the Tunnel

### Method 1: Using a Secret File (Recommended)

Create a file called `.playit-secret` in the project root with your secret key:

```bash
echo "your-secret-key-here" > .playit-secret
```

Then start your server and run:
```
/tunnel start
```

### Method 2: Providing Key Directly

Start your server and run:
```
/tunnel start your-secret-key-here
```

## Server Commands

- `/tunnel start [secret-key]` - Start the tunnel (uses `.playit-secret` if no key provided)
- `/tunnel stop` - Stop the tunnel
- `/tunnel status` - Check if tunnel is running
- `/help` - Show all available commands
- `/stop` - Stop the server

## Finding Your Connection Address

After starting the tunnel, go to https://playit.gg/account to see your public tunnel address.

Your friend can then connect to that address to join your server!

## Logs

Tunnel output is logged to `logs/playit.log` if you need to troubleshoot.

## How It Works

The playit.gg agent creates a tunnel from your local server to a public address. This works similar to the e4mc mod for Minecraft - no port forwarding needed!

The implementation:
- Tries to use Docker first (most reliable)
- Falls back to native playit binary if Docker isn't available
- Automatically stops the tunnel when the server shuts down
