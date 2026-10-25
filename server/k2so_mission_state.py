"""
k2so_mission_state.py
=====================
Thread-safe and atomic mission telemetry state reporter for K-2SO & TiRA.
Synchronizes live ticket execution and gate status with digivice_metrics_server.py
"""

import json
import os
import tempfile
import time
from typing import Dict, Any, Optional

STATE_FILE = "/tmp/k2so_live_mission.json"

DEFAULT_STATE: Dict[str, Any] = {
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
        "g3": "IDLE",
    },
    "last_resolved_ticket": 291,
    "elapsed_sec": 0.0,
    "updated_at": 0.0,
}

_mission_start_time: Optional[float] = None


def read_mission_state() -> Dict[str, Any]:
    """Reads current live mission state with fallback to defaults."""
    state = dict(DEFAULT_STATE)
    if os.path.exists(STATE_FILE):
        try:
            with open(STATE_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
                state.update(data)
        except Exception:
            pass
    return state


def update_mission_state(
    active: bool = True,
    protocol: str = "STANDBY",
    ticket_id: Optional[int] = None,
    ticket_folio: str = "STANDBY",
    stage: str = "IDLE",
    attempt: int = 1,
    circuit_breaker: bool = False,
    gates: Optional[Dict[str, str]] = None,
    last_resolved_ticket: Optional[int] = None,
) -> Dict[str, Any]:
    """
    Atomically writes mission state to /tmp/k2so_live_mission.json.
    Atomic rename prevents torn reads from metrics poller.
    """
    global _mission_start_time
    now = time.time()

    if active:
        if _mission_start_time is None:
            _mission_start_time = now
        elapsed = round(now - _mission_start_time, 1)
    else:
        _mission_start_time = None
        elapsed = 0.0

    current = read_mission_state()
    current_gates = current.get("gates", dict(DEFAULT_STATE["gates"]))
    if gates:
        current_gates.update(gates)

    new_state = {
        "active": active,
        "protocol": protocol,
        "ticket_id": ticket_id if ticket_id is not None else current.get("ticket_id"),
        "ticket_folio": ticket_folio if ticket_folio != "STANDBY" else current.get("ticket_folio", "STANDBY"),
        "stage": stage,
        "attempt": attempt,
        "circuit_breaker": circuit_breaker,
        "gates": current_gates,
        "last_resolved_ticket": last_resolved_ticket or current.get("last_resolved_ticket", 291),
        "elapsed_sec": elapsed,
        "updated_at": now,
    }

    try:
        temp_dir = os.path.dirname(STATE_FILE) or "/tmp"
        with tempfile.NamedTemporaryFile("w", dir=temp_dir, delete=False, encoding="utf-8") as tf:
            json.dump(new_state, tf)
            temp_name = tf.name
        os.replace(temp_name, STATE_FILE)
    except Exception:
        pass

    return new_state


def reset_mission_to_standby(last_resolved: Optional[int] = None):
    """Resets state to STANDBY."""
    global _mission_start_time
    _mission_start_time = None
    update_mission_state(
        active=False,
        protocol="STANDBY",
        ticket_id=None,
        ticket_folio="STANDBY",
        stage="IDLE",
        attempt=0,
        circuit_breaker=False,
        gates={"g0": "IDLE", "g1": "IDLE", "g2": "IDLE", "g3": "IDLE"},
        last_resolved_ticket=last_resolved,
    )


if __name__ == "__main__":
    import sys
    print("Initializing Standby Mission State...")
    reset_mission_to_standby(291)
    print("State:", read_mission_state())
