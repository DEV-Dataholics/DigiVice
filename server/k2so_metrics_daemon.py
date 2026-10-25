#!/usr/bin/env python3
"""
K-2SO Digibadge Server Metrics Daemon (DGM-SRV-02 - Mission & Tactical HUD Edition)
Exposes real-time hardware vitals, droid operational status, and active mission gates
on HTTP port 8000 for ESP32 Digivice HUD polling.
"""

import json
import os
import subprocess
import time
import psutil
import uvicorn
from fastapi import FastAPI

app = FastAPI(title="K-2SO Digibadge Metrics Daemon", version="2.0.0")

# Prime non-blocking CPU percent counter
psutil.cpu_percent(interval=None)

STATE_FILE = "/tmp/k2so_live_mission.json"

DEFAULT_MISSION = {
    "active": False,
    "protocol": "STANDBY",
    "ticket_id": None,
    "ticket_folio": "STANDBY",
    "stage": "IDLE",
    "attempt": 0,
    "circuit_breaker": False,
    "gates": {
        "g0": "IDLE",
        "g1": "IDLE",
        "g2": "IDLE",
        "g3": "IDLE"
    },
    "last_resolved_ticket": 291,
    "elapsed_sec": 0.0,
}


def get_service_state(service_name: str) -> str:
    try:
        res = subprocess.run(
            ["systemctl", "is-active", service_name],
            capture_output=True,
            text=True,
            timeout=0.5,
        )
        return res.stdout.strip() or "unknown"
    except Exception:
        return "unknown"


def get_mission_state() -> dict:
    mission = dict(DEFAULT_MISSION)
    if os.path.exists(STATE_FILE):
        try:
            with open(STATE_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
                mission.update(data)
                # Compute elapsed time dynamically if active
                if mission.get("active") and mission.get("updated_at"):
                    delta = time.time() - mission.get("updated_at")
                    if delta > 300:  # Stale mission timeout after 5 minutes of no heartbeats
                        mission["active"] = False
                        mission["stage"] = "TIMEOUT"
        except Exception:
            pass
    return mission


@app.get("/metrics")
def get_metrics():
    cpu_val = round(psutil.cpu_percent(interval=0.1), 1)
    ram_val = round(psutil.virtual_memory().percent, 1)
    disk_val = round(psutil.disk_usage("/").percent, 1)
    load_1m, load_5m, load_15m = [round(x, 2) for x in os.getloadavg()]
    mission_data = get_mission_state()

    return {
        "status": "online",
        "host": "K-2SO",
        "cpu": cpu_val,
        "ram": ram_val,
        "disk": disk_val,
        "load_1m": load_1m,
        "load_5m": load_5m,
        "uptime_sec": int(time.time() - psutil.boot_time()),
        "poller": get_service_state("resolve-poller.service"),
        "ollama": get_service_state("ollama.service"),
        "mission": mission_data,
    }


@app.get("/health")
def health():
    return {"ok": True, "host": "K-2SO"}


if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000, log_level="warning")
