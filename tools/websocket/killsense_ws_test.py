#!/usr/bin/env python3
"""End-to-end test of KopSkoot inside a running OBS via obs-websocket v5.

What it does (this is also a reference for Streamer.bot / custom clients):
  1. connects to obs-websocket and subscribes to Vendor events
  2. builds a "KillSense Test" scene from data/test-assets (background + two
     hidden kill-feed overlays)
  3. calls the vendor request RealmKillSense/StartDetection for that scene
  4. shows/hides the overlays like kills happening in a game
  5. prints every RealmKillSense VendorEvent and checks the kill count
  6. exercises TestKill, GetStatus and ResetSession

Requires: pip install websockets
Usage:  python killsense_ws_test.py --assets <repo>/data/test-assets [--url ws://127.0.0.1:4455] [--password PW]
"""
import argparse
import asyncio
import base64
import hashlib
import json
import os
import sys
import uuid

import websockets

VENDOR = "RealmKillSense"
EVENT_SUB_VENDORS = 1 << 9


class Client:
    def __init__(self, ws):
        self.ws = ws
        self.pending = {}
        self.vendor_events = []

    async def reader(self):
        async for raw in self.ws:
            msg = json.loads(raw)
            op, d = msg["op"], msg["d"]
            if op == 7:  # RequestResponse
                fut = self.pending.pop(d["requestId"], None)
                if fut:
                    fut.set_result(d)
            elif op == 5 and d.get("eventType") == "VendorEvent":
                ev = d["eventData"]
                if ev.get("vendorName") == VENDOR:
                    self.vendor_events.append(ev)
                    print("VendorEvent %-12s %s" % (ev["eventType"], json.dumps(ev["eventData"])))

    async def request(self, rtype, data=None):
        rid = str(uuid.uuid4())
        fut = asyncio.get_running_loop().create_future()
        self.pending[rid] = fut
        await self.ws.send(json.dumps({"op": 6, "d": {"requestType": rtype, "requestId": rid, "requestData": data or {}}}))
        return await asyncio.wait_for(fut, 10)

    async def vendor(self, rtype, data=None):
        r = await self.request("CallVendorRequest", {"vendorName": VENDOR, "requestType": rtype, "requestData": data or {}})
        return r.get("responseData", {}).get("responseData", {})


async def identify(ws, password):
    hello = json.loads(await ws.recv())["d"]
    ident = {"rpcVersion": 1, "eventSubscriptions": EVENT_SUB_VENDORS}
    if "authentication" in hello:
        a = hello["authentication"]
        secret = base64.b64encode(hashlib.sha256((password + a["salt"]).encode()).digest()).decode()
        ident["authentication"] = base64.b64encode(hashlib.sha256((secret + a["challenge"]).encode()).digest()).decode()
    await ws.send(json.dumps({"op": 1, "d": ident}))
    msg = json.loads(await ws.recv())
    if msg["op"] != 2:
        raise RuntimeError("identify failed: %s" % msg)


async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="ws://127.0.0.1:4455")
    ap.add_argument("--password", default="")
    ap.add_argument("--assets", required=True)
    ap.add_argument("--scene", default="KillSense Test")
    args = ap.parse_args()
    assets = os.path.abspath(args.assets)

    async with websockets.connect(args.url, subprotocols=["obswebsocket.json"], max_size=None) as ws:
        await identify(ws, args.password)
        c = Client(ws)
        reader = asyncio.create_task(c.reader())

        await c.request("CreateScene", {"sceneName": args.scene})
        ids = {}
        for name, file, enabled in (("ks-bg", "killsense-test-background.png", True),
                                    ("ks-kill-row1", "killsense-test-kill-row1.png", False),
                                    ("ks-kill-row2", "killsense-test-kill-row2.png", False)):
            r = await c.request("CreateInput", {"sceneName": args.scene, "inputName": name, "inputKind": "image_source",
                                                "inputSettings": {"file": os.path.join(assets, file)},
                                                "sceneItemEnabled": enabled})
            if not r["requestStatus"]["result"]:  # already exists: look the item up
                r = await c.request("GetSceneItemId", {"sceneName": args.scene, "sourceName": name})
            ids[name] = r["responseData"]["sceneItemId"]
        await c.request("SetCurrentProgramScene", {"sceneName": args.scene})

        async def show(name, on):
            await c.request("SetSceneItemEnabled", {"sceneName": args.scene, "sceneItemId": ids[name], "sceneItemEnabled": on})

        print("StartDetection ->", await c.vendor("StartDetection", {"sourceName": args.scene, "profileId": "sample-killfeed"}))
        await asyncio.sleep(2)
        print("-- single kill-feed entry")
        await show("ks-kill-row1", True)
        await asyncio.sleep(3)
        await show("ks-kill-row1", False)
        await asyncio.sleep(2.5)
        print("-- two entries (rapid second kill)")
        await show("ks-kill-row2", True)
        await asyncio.sleep(3)
        await show("ks-kill-row2", False)
        await asyncio.sleep(1.5)

        vision_kills = [e for e in c.vendor_events if e["eventType"] == "Kill" and not e["eventData"].get("test")]
        print("TestKill ->", await c.vendor("TestKill"))
        await asyncio.sleep(0.5)
        status = await c.vendor("GetStatus")
        print("GetStatus ->", json.dumps(status))
        print("ResetSession ->", await c.vendor("ResetSession"))
        await asyncio.sleep(0.5)
        print("StopDetection ->", await c.vendor("StopDetection"))
        reader.cancel()

        ok = (len(vision_kills) == 3 and status.get("sessionKills") == 4
              and any(e["eventType"] == "SessionReset" for e in c.vendor_events))
        print("vision kills: %d (expected 3), session kills after test event: %s (expected 4) -> %s"
              % (len(vision_kills), status.get("sessionKills"), "PASS" if ok else "FAIL"))
        return 0 if ok else 2


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
