#!/usr/bin/env python3
"""
Flipper Zero Bridge Client - Agent Side
Huonyx AI Smartwatch

This script allows the Huonyx agent (or any automation) to send Flipper Zero
CLI commands to the watch via Supabase Realtime Broadcast, and receive results.

Usage:
    # Send a single command
    python flipper_bridge.py --command "ir tx NEC 0x04 0x08"

    # Interactive mode
    python flipper_bridge.py --interactive

    # Listen for results only
    python flipper_bridge.py --listen

Environment Variables:
    SUPABASE_URL    - Your Supabase project URL
    SUPABASE_KEY    - Your Supabase anon key

Architecture:
    Agent -> Supabase Broadcast -> Watch (ESP32) -> Flipper Zero BLE
    Flipper Zero BLE -> Watch (ESP32) -> Supabase Broadcast -> Agent
"""

import os
import sys
import json
import time
import argparse
import asyncio
from datetime import datetime

try:
    import websockets
except ImportError:
    print("Install websockets: pip install websockets")
    sys.exit(1)


# ── Configuration ─────────────────────────────────────────

SUPABASE_URL = os.environ.get("SUPABASE_URL", "")
SUPABASE_KEY = os.environ.get("SUPABASE_KEY", "")
CHANNEL_TOPIC = "realtime:flipper-bridge"

# Event types (must match watch firmware)
EVT_FLIPPER_COMMAND = "flipper_command"
EVT_FLIPPER_RESULT = "flipper_result"
EVT_FLIPPER_STATUS = "flipper_status"
EVT_WATCH_STATUS = "watch_status"
EVT_AGENT_MESSAGE = "agent_message"


class FlipperBridge:
    """Supabase Realtime client for Flipper Zero command relay."""

    def __init__(self, url: str, key: str):
        self.url = url.rstrip("/")
        self.key = key
        self.ws = None
        self.ref_counter = 0
        self.join_ref = None
        self.connected = False
        self.joined = False
        self.pending_commands = {}
        self.cmd_counter = 0

    def _ws_url(self) -> str:
        """Build the WebSocket URL for Supabase Realtime."""
        host = self.url.replace("https://", "").replace("http://", "")
        return f"wss://{host}/realtime/v1/websocket?apikey={self.key}&vsn=1.0.0"

    def _next_ref(self) -> str:
        self.ref_counter += 1
        return str(self.ref_counter)

    async def connect(self):
        """Connect to Supabase Realtime WebSocket."""
        ws_url = self._ws_url()
        print(f"[BRIDGE] Connecting to: {ws_url[:80]}...")

        self.ws = await websockets.connect(ws_url)
        self.connected = True
        print("[BRIDGE] WebSocket connected")

        # Join the channel
        await self._join_channel()

    async def _join_channel(self):
        """Join the flipper-bridge broadcast channel."""
        ref = self._next_ref()
        self.join_ref = ref

        join_msg = {
            "topic": CHANNEL_TOPIC,
            "event": "phx_join",
            "ref": ref,
            "join_ref": ref,
            "payload": {
                "config": {
                    "broadcast": {"ack": False, "self": False},
                    "presence": {"enabled": False},
                    "private": False,
                },
                "access_token": self.key,
            },
        }

        await self.ws.send(json.dumps(join_msg))
        print(f"[BRIDGE] Sent phx_join for {CHANNEL_TOPIC}")

        # Wait for join confirmation
        while True:
            msg = await self.ws.recv()
            data = json.loads(msg)

            if data.get("event") == "phx_reply":
                status = data.get("payload", {}).get("status", "")
                if status == "ok" and data.get("topic") == CHANNEL_TOPIC:
                    self.joined = True
                    print("[BRIDGE] Successfully joined channel")
                    return
                elif status != "ok":
                    raise Exception(f"Join failed: {status}")

    async def send_flipper_command(self, command: str) -> int:
        """Send a Flipper CLI command via broadcast."""
        if not self.joined:
            raise Exception("Not joined to channel")

        self.cmd_counter += 1
        cmd_id = self.cmd_counter

        broadcast_msg = {
            "topic": CHANNEL_TOPIC,
            "event": "broadcast",
            "ref": self._next_ref(),
            "join_ref": self.join_ref,
            "payload": {
                "type": "broadcast",
                "event": EVT_FLIPPER_COMMAND,
                "payload": {
                    "command": command,
                    "cmd_id": cmd_id,
                    "source": "agent",
                    "timestamp": int(time.time() * 1000),
                },
            },
        }

        await self.ws.send(json.dumps(broadcast_msg))
        self.pending_commands[cmd_id] = {
            "command": command,
            "sent_at": datetime.now(),
        }
        print(f"[CMD #{cmd_id}] Sent: {command}")
        return cmd_id

    async def send_agent_message(self, message: str):
        """Send a text message to the watch display."""
        if not self.joined:
            return

        broadcast_msg = {
            "topic": CHANNEL_TOPIC,
            "event": "broadcast",
            "ref": self._next_ref(),
            "join_ref": self.join_ref,
            "payload": {
                "type": "broadcast",
                "event": EVT_AGENT_MESSAGE,
                "payload": {
                    "message": message,
                    "source": "agent",
                },
            },
        }

        await self.ws.send(json.dumps(broadcast_msg))
        print(f"[MSG] Sent to watch: {message}")

    async def _send_heartbeat(self):
        """Send Phoenix heartbeat."""
        heartbeat = {
            "topic": "phoenix",
            "event": "heartbeat",
            "payload": {},
            "ref": self._next_ref(),
        }
        await self.ws.send(json.dumps(heartbeat))

    async def listen(self, callback=None):
        """Listen for incoming messages (results, status updates)."""
        heartbeat_interval = 30
        last_heartbeat = time.time()

        while True:
            try:
                # Send heartbeat periodically
                if time.time() - last_heartbeat > heartbeat_interval:
                    await self._send_heartbeat()
                    last_heartbeat = time.time()

                # Wait for message with timeout
                try:
                    msg = await asyncio.wait_for(self.ws.recv(), timeout=5.0)
                except asyncio.TimeoutError:
                    continue

                data = json.loads(msg)
                event = data.get("event", "")

                if event == "broadcast":
                    payload = data.get("payload", {})
                    inner_event = payload.get("event", "")
                    inner_payload = payload.get("payload", {})

                    if inner_event == EVT_FLIPPER_RESULT:
                        cmd_id = inner_payload.get("cmd_id", 0)
                        result = inner_payload.get("result", "")
                        success = inner_payload.get("success", False)

                        print(f"\n[RESULT #{cmd_id}] {'OK' if success else 'FAIL'}")
                        print(f"  {result}")

                        if cmd_id in self.pending_commands:
                            del self.pending_commands[cmd_id]

                        if callback:
                            callback("result", inner_payload)

                    elif inner_event == EVT_FLIPPER_STATUS:
                        state = inner_payload.get("state", "unknown")
                        device = inner_payload.get("device", "")
                        rssi = inner_payload.get("rssi", 0)
                        print(f"\n[FLIPPER] State: {state}, Device: {device}, RSSI: {rssi}")

                        if callback:
                            callback("flipper_status", inner_payload)

                    elif inner_event == EVT_WATCH_STATUS:
                        battery = inner_payload.get("battery", 0)
                        wifi = inner_payload.get("wifi", False)
                        gw = inner_payload.get("gateway", False)
                        flip = inner_payload.get("flipper", False)
                        heap = inner_payload.get("free_heap", 0)
                        print(
                            f"\n[WATCH] Battery: {battery}%, WiFi: {wifi}, "
                            f"Gateway: {gw}, Flipper: {flip}, Heap: {heap}"
                        )

                        if callback:
                            callback("watch_status", inner_payload)

                elif event == "phx_reply":
                    pass  # Heartbeat ack, etc.

            except websockets.ConnectionClosed:
                print("[BRIDGE] Connection closed, reconnecting...")
                await self.connect()

    async def disconnect(self):
        """Disconnect from Supabase."""
        if self.ws:
            await self.ws.close()
            self.connected = False
            self.joined = False
            print("[BRIDGE] Disconnected")


# ── CLI Interface ─────────────────────────────────────────


async def interactive_mode(bridge: FlipperBridge):
    """Interactive command-line interface."""
    print("\n╔══════════════════════════════════════╗")
    print("║   Flipper Zero Bridge - Agent Side   ║")
    print("║   Type Flipper CLI commands below     ║")
    print("║   Type 'quit' to exit                 ║")
    print("║   Type 'msg:...' to send to watch     ║")
    print("╚══════════════════════════════════════╝\n")

    # Start listener in background
    listen_task = asyncio.create_task(bridge.listen())

    try:
        while True:
            # Read input (non-blocking)
            cmd = await asyncio.get_event_loop().run_in_executor(
                None, lambda: input("\nflipper> ")
            )

            cmd = cmd.strip()
            if not cmd:
                continue

            if cmd.lower() == "quit":
                break

            if cmd.startswith("msg:"):
                await bridge.send_agent_message(cmd[4:].strip())
            else:
                await bridge.send_flipper_command(cmd)

    except (KeyboardInterrupt, EOFError):
        pass
    finally:
        listen_task.cancel()
        await bridge.disconnect()


async def single_command_mode(bridge: FlipperBridge, command: str, timeout: int = 15):
    """Send a single command and wait for result."""
    cmd_id = await bridge.send_flipper_command(command)

    # Wait for result
    start = time.time()
    while time.time() - start < timeout:
        try:
            msg = await asyncio.wait_for(bridge.ws.recv(), timeout=1.0)
            data = json.loads(msg)

            if data.get("event") == "broadcast":
                payload = data.get("payload", {})
                if payload.get("event") == EVT_FLIPPER_RESULT:
                    inner = payload.get("payload", {})
                    if inner.get("cmd_id") == cmd_id:
                        result = inner.get("result", "")
                        success = inner.get("success", False)
                        print(f"\n{'OK' if success else 'FAIL'}: {result}")
                        await bridge.disconnect()
                        return 0 if success else 1
        except asyncio.TimeoutError:
            continue

    print(f"\nTimeout waiting for result (>{timeout}s)")
    await bridge.disconnect()
    return 2


async def listen_mode(bridge: FlipperBridge):
    """Listen for all events."""
    print("\n[LISTEN] Listening for events (Ctrl+C to stop)...\n")
    try:
        await bridge.listen()
    except KeyboardInterrupt:
        pass
    finally:
        await bridge.disconnect()


async def main():
    parser = argparse.ArgumentParser(description="Flipper Zero Bridge - Agent Side")
    parser.add_argument("--url", default=SUPABASE_URL, help="Supabase project URL")
    parser.add_argument("--key", default=SUPABASE_KEY, help="Supabase anon key")
    parser.add_argument("--command", "-c", help="Single Flipper CLI command to send")
    parser.add_argument("--message", "-m", help="Text message to send to watch")
    parser.add_argument("--interactive", "-i", action="store_true", help="Interactive mode")
    parser.add_argument("--listen", "-l", action="store_true", help="Listen for events")
    parser.add_argument("--timeout", "-t", type=int, default=15, help="Command timeout (seconds)")
    args = parser.parse_args()

    url = args.url or SUPABASE_URL
    key = args.key or SUPABASE_KEY

    if not url or not key:
        print("Error: Set SUPABASE_URL and SUPABASE_KEY environment variables")
        print("  or use --url and --key arguments")
        sys.exit(1)

    bridge = FlipperBridge(url, key)
    await bridge.connect()

    if args.command:
        exit_code = await single_command_mode(bridge, args.command, args.timeout)
        sys.exit(exit_code)
    elif args.message:
        await bridge.send_agent_message(args.message)
        await bridge.disconnect()
    elif args.listen:
        await listen_mode(bridge)
    else:
        await interactive_mode(bridge)


if __name__ == "__main__":
    asyncio.run(main())
