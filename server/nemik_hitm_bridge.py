"""
nemik_hitm_bridge.py
====================
TiRA v2.0 (Project Nemik) Dual-Gate Cryptographic HiTM Bridge.
Manages executive authorization workflows for:
  - HiTM Gate 1: Work Intake & Sovereign Crucible Launch (0 Cloud Tokens)
  - HiTM Gate 2: Cloud Escalation Firewall (Antigravity SDK Pilot)
"""

import base64
import hashlib
import hmac
import html
import json
import os
import smtplib
import ssl
import time
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
from typing import Any, Dict, Optional

# Load environment configuration
ENV_FILE = "/opt/tira-worker/.env" if os.path.exists("/opt/tira-worker/.env") else os.path.join(os.path.dirname(__file__), ".env")
if os.path.exists(ENV_FILE):
    try:
        with open(ENV_FILE, "r", encoding="utf-8") as ef:
            for line in ef:
                line = line.strip()
                if line and not line.startswith("#") and "=" in line:
                    k, v = line.split("=", 1)
                    os.environ.setdefault(k.strip(), v.strip())
    except Exception:
        pass

CTO_EMAIL = os.getenv("TIRA_HITM_CTO_EMAIL", "lmorales@dataholics.com.mx")
RESOLVE_BASE_URL = os.getenv("RESOLVE_BASE_URL", "https://resolve.dataholics.com.mx")
INGESTION_KEY = os.getenv("TIRA_INGESTION_KEY", "b34c8e26d2794d8bfa19065a202066e7cdbf3e997abfd4d5")

SMTP_HOST = os.getenv("SMTP_HOST", "mail.dataholics.com.mx")
SMTP_PORT = int(os.getenv("SMTP_PORT", "465"))
SMTP_USER = os.getenv("SMTP_USER", "data_support@dataholics.com.mx")
SMTP_PASS = os.getenv("SMTP_PASS", "")

try:
    from k2so_mission_state import update_mission_state
except ImportError:
    def update_mission_state(**kwargs):
        pass


def send_smtp_email(to_addr: str, subject: str, html_content: str, from_name: str = "TiRA v2.0 Project Nemik") -> bool:
    """Dispatches SSL SMTP email with RFC-compliant headers."""
    try:
        msg = MIMEMultipart("alternative")
        msg["Subject"] = subject
        msg["From"] = f"{from_name} <{SMTP_USER}>"
        msg["To"] = to_addr
        msg.attach(MIMEText(html_content, "html", "utf-8"))

        ctx = ssl.create_default_context()
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE

        with smtplib.SMTP_SSL(SMTP_HOST, SMTP_PORT, context=ctx, timeout=15) as srv:
            srv.login(SMTP_USER, SMTP_PASS)
            srv.sendmail(SMTP_USER, [to_addr], msg.as_string())
        return True
    except Exception as e:
        print(f"[NemikBridge] SMTP dispatch failed: {e}")
        return False


def dispatch_hitm_gate1_work_auth(
    ticket_id: int,
    subject: str,
    company_name: str,
    company_slug: str,
    requester_name: str,
    requester_email: str,
    blueprint: Dict[str, Any],
    confidence: int = 90,
    risk: str = "LOW"
) -> bool:
    """
    HiTM Gate 1: Work Intake & Sovereign Crucible Authorization.
    Dispatches cryptographic 1-click approval link to My CTO (Luis Morales).
    """
    expires = int(time.time()) + (7 * 86400)
    sig_approve = hmac.new(
        INGESTION_KEY.encode(),
        f"{ticket_id}|approve_track3|{expires}|none".encode(),
        hashlib.sha256
    ).hexdigest()

    approve_url = f"{RESOLVE_BASE_URL}/api/triage/action?ticket_id={ticket_id}&action=approve_track3&expires={expires}&token=none&sig={sig_approve}&sender={requester_email}"
    servicedesk_url = f"{RESOLVE_BASE_URL}/servicedesk.html#ticket-{ticket_id}"

    # Update Mission State on node and ESP32 Digivice
    update_mission_state(
        active=True,
        protocol="ANDOR",
        ticket_id=ticket_id,
        ticket_folio=f"[TKT-{ticket_id}]",
        stage="HITM_WORK_AUTH",
        attempt=1
    )

    safe_subject = html.escape(subject)
    safe_company = html.escape(company_name)
    safe_requester = html.escape(requester_name)
    scope_label = html.escape(blueprint.get("scope_label", "FEATURE / BUGFIX"))
    target_files = blueprint.get("files_to_modify", []) + blueprint.get("files_to_create", [])
    files_html = "".join(f"<li style='margin-bottom:4px;'><code>{html.escape(f)}</code></li>" for f in target_files) if target_files else "<li><code>Automatic P10 Target Detection</code></li>"

    html_email = f"""<!DOCTYPE html><html><head><meta charset='utf-8'>
<style>
  body{{margin:0;padding:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;color:#e2e8f0;background:#090d16;}}
  .wrap{{max-width:620px;margin:25px auto;background:#0f172a;border-radius:12px;border:1px solid #1e293b;overflow:hidden;box-shadow:0 8px 30px rgba(0,0,0,0.5);}}
  .hdr{{background:linear-gradient(135deg,#0284c7,#0369a1);padding:22px;border-bottom:2px solid #38bdf8;}}
  .hdr h2{{margin:0;color:#ffffff;font-size:18px;font-weight:800;letter-spacing:-0.02em;}}
  .hdr p{{margin:4px 0 0;color:#e0f2fe;font-size:12px;font-weight:500;}}
  .badge-zero{{display:inline-block;background:#059669;color:#ffffff;padding:4px 10px;border-radius:20px;font-size:11px;font-weight:700;margin-top:10px;letter-spacing:0.04em;}}
  .content{{padding:26px;font-size:14px;line-height:1.6;}}
  .card{{background:#1e293b;border:1px solid #334155;border-radius:8px;padding:16px;margin:18px 0;}}
  .btn-approve{{display:inline-block;background:#0284c7;color:#ffffff;text-decoration:none;padding:12px 24px;border-radius:6px;font-weight:700;font-size:14px;box-shadow:0 4px 12px rgba(2,132,199,0.3);}}
  .btn-desk{{display:inline-block;background:#334155;color:#94a3b8;text-decoration:none;padding:12px 18px;border-radius:6px;font-weight:600;font-size:13px;margin-left:8px;}}
  .ftr{{background:#0b0f19;padding:14px 24px;text-align:center;font-size:11px;color:#64748b;border-top:1px solid #1e293b;}}
</style></head><body>
<div class='wrap'>
  <div class='hdr'>
    <h2>🛡️ TiRA v2.0: Project Nemik • HiTM Gate 1</h2>
    <p>Dataholics Autonomous Engineering • Node 192.168.1.40 (K-2SO)</p>
    <div class='badge-zero'>⚡ 0 CLOUD TOKENS • 100% LOCAL SOVEREIGN CRUCIBLE</div>
  </div>
  <div class='content'>
    <p>Good day, <strong>My CTO (Luis Morales)</strong>,</p>
    <p>Ticket <strong>#{ticket_id}</strong> has been received and customer acknowledgment has been dispatched. K-2SO has concluded its sovereign pre-flight analysis.</p>
    
    <div class='card'>
      <div style='font-size:15px;font-weight:700;color:#38bdf8;margin-bottom:8px;'>{safe_company} • Ticket #{ticket_id}</div>
      <div style='color:#cbd5e1;margin-bottom:10px;'><strong>Subject:</strong> {safe_subject}</div>
      <div style='font-size:12px;color:#94a3b8;margin-bottom:6px;'><strong>Requester:</strong> {safe_requester} ({requester_email})</div>
      <div style='font-size:12px;color:#94a3b8;margin-bottom:6px;'><strong>Scope:</strong> {scope_label} | <strong>Confidence:</strong> {confidence}% | <strong>Risk:</strong> {risk}</div>
      <div style='font-size:12px;color:#94a3b8;margin-top:10px;'><strong>Candidate Target Files:</strong></div>
      <ul style='font-size:12px;color:#cbd5e1;padding-left:20px;margin:6px 0 0;'>
        {files_html}
      </ul>
    </div>

    <p style='text-align:center;font-weight:600;color:#cbd5e1;margin:22px 0 16px;'>
      Authorize K-2SO to launch the Autonomous Andor Crucible?
    </p>

    <div style='text-align:center;margin-bottom:24px;'>
      <a href='{approve_url}' class='btn-approve'>🚀 Authorize K-2SO Lead Crucible</a>
      <a href='{servicedesk_url}' class='btn-desk'>🔍 View in ServiceDesk</a>
    </div>

    <p style='font-size:11px;color:#64748b;text-align:center;'>
      🔒 HiTM Gate 1 Active: No repository modifications or database seeds will execute without your authorization.
    </p>
  </div>
  <div class='ftr'>Dataholics Resolve • TiRA v2.0 Project Nemik Architecture</div>
</div></body></html>"""

    email_subject = f"[TiRA Nemik HiTM #1] Ticket #{ticket_id}: {subject} — {scope_label} ({confidence}%)"
    return send_smtp_email(CTO_EMAIL, email_subject, html_email, from_name="TiRA v2.0 Project Nemik")


def dispatch_hitm_gate2_cloud_auth(
    ticket_id: int,
    subject: str,
    company_name: str,
    company_slug: str,
    failing_file: str,
    compiler_error: str,
    diff_snippet: Optional[str] = None,
    estimated_tokens: int = 2500
) -> bool:
    """
    HiTM Gate 2: Antigravity Cloud Escalation Firewall.
    Dispatched when K-2SO local retries fail or architectural wall is hit.
    Strictly halts cloud egress until My CTO authorizes.
    """
    expires = int(time.time()) + (7 * 86400)
    sig_cloud = hmac.new(
        INGESTION_KEY.encode(),
        f"{ticket_id}|approve_nemik_cloud|{expires}|none".encode(),
        hashlib.sha256
    ).hexdigest()

    approve_url = f"{RESOLVE_BASE_URL}/api/triage/action?ticket_id={ticket_id}&action=approve_nemik_cloud&expires={expires}&token=none&sig={sig_cloud}&sender={CTO_EMAIL}"
    servicedesk_url = f"{RESOLVE_BASE_URL}/servicedesk.html#ticket-{ticket_id}"

    # Package local diagnostic snapshot into /tmp
    diag_packet = {
        "ticket_id": ticket_id,
        "subject": subject,
        "company": company_name,
        "company_slug": company_slug,
        "failing_file": failing_file,
        "compiler_error": compiler_error,
        "diff_snippet": diff_snippet or "",
        "estimated_tokens": estimated_tokens,
        "timestamp": time.time(),
    }
    diag_path = f"/tmp/k2so_ticket_{ticket_id}_cloud_request.json"
    try:
        with open(diag_path, "w", encoding="utf-8") as f:
            json.dump(diag_packet, f, indent=2)
    except Exception:
        pass

    # Update Mission State on node and ESP32 Digivice
    update_mission_state(
        active=True,
        protocol="ANDOR",
        ticket_id=ticket_id,
        ticket_folio=f"[TKT-{ticket_id}]",
        stage="HITM_CLOUD_AUTH",
        circuit_breaker=True,
        attempt=2
    )

    safe_subject = html.escape(subject)
    safe_company = html.escape(company_name)
    safe_file = html.escape(failing_file)
    safe_error = html.escape(compiler_error)
    est_cost = round(estimated_tokens * 0.0000015, 4)

    html_email = f"""<!DOCTYPE html><html><head><meta charset='utf-8'>
<style>
  body{{margin:0;padding:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;color:#e2e8f0;background:#090d16;}}
  .wrap{{max-width:620px;margin:25px auto;background:#0f172a;border-radius:12px;border:1px solid #1e293b;overflow:hidden;box-shadow:0 8px 30px rgba(0,0,0,0.5);}}
  .hdr{{background:linear-gradient(135deg,#b91c1c,#991b1b);padding:22px;border-bottom:2px solid #ef4444;}}
  .hdr h2{{margin:0;color:#ffffff;font-size:18px;font-weight:800;letter-spacing:-0.02em;}}
  .hdr p{{margin:4px 0 0;color:#fecaca;font-size:12px;font-weight:500;}}
  .badge-cloud{{display:inline-block;background:#7f1d1d;color:#fca5a5;padding:4px 10px;border-radius:20px;font-size:11px;font-weight:700;margin-top:10px;border:1px solid #ef4444;}}
  .content{{padding:26px;font-size:14px;line-height:1.6;}}
  .card{{background:#1e293b;border:1px solid #334155;border-radius:8px;padding:16px;margin:18px 0;}}
  .err-box{{background:#2d1215;border-left:4px solid #ef4444;padding:12px;border-radius:4px;font-family:monospace;font-size:12px;color:#fca5a5;margin:10px 0;overflow-x:auto;}}
  .budget-box{{background:#0f172a;border:1px solid #0284c7;border-radius:6px;padding:12px;margin:14px 0;font-size:12px;color:#38bdf8;}}
  .btn-cloud{{display:inline-block;background:#dc2626;color:#ffffff;text-decoration:none;padding:12px 24px;border-radius:6px;font-weight:700;font-size:14px;box-shadow:0 4px 12px rgba(220,38,38,0.4);}}
  .btn-desk{{display:inline-block;background:#334155;color:#94a3b8;text-decoration:none;padding:12px 18px;border-radius:6px;font-weight:600;font-size:13px;margin-left:8px;}}
  .ftr{{background:#0b0f19;padding:14px 24px;text-align:center;font-size:11px;color:#64748b;border-top:1px solid #1e293b;}}
</style></head><body>
<div class='wrap'>
  <div class='hdr'>
    <h2>🚨 TiRA v2.0: Project Nemik • HiTM Gate 2</h2>
    <p>Cloud Escalation Firewall • Node 192.168.1.40 (K-2SO)</p>
    <div class='badge-cloud'>⚠️ LOCAL RETRIES EXHAUSTED • CLOUD EGRESS FROZEN</div>
  </div>
  <div class='content'>
    <p>Urgent Notice for <strong>My CTO (Luis Morales)</strong>,</p>
    <p>K-2SO has encountered a deterministic verification wall on Ticket <strong>#{ticket_id}</strong> ({safe_company}). The local autonomous engine has <strong>halted</strong> and frozen code egress to protect production and tokens.</p>
    
    <div class='card'>
      <div style='font-size:14px;font-weight:700;color:#ef4444;margin-bottom:6px;'>Failing Target: <code>{safe_file}</code></div>
      <div style='color:#cbd5e1;font-size:13px;margin-bottom:8px;'><strong>Subject:</strong> {safe_subject}</div>
      <div class='err-box'>{safe_error}</div>
    </div>

    <div class='budget-box'>
      <strong>📊 Surgical Cloud Escalation Budget:</strong><br>
      • Ingestion Scope: Pre-pruned AST slice (&le; 50 lines, zero credentials)<br>
      • Engine: Antigravity Frontier Reasoning (Gemini 3.8 Flash)<br>
      • Projected Token Spend: <strong>~{estimated_tokens:,} tokens (~${est_cost} USD)</strong>
    </div>

    <p style='text-align:center;font-weight:600;color:#cbd5e1;margin:22px 0 16px;'>
      Authorize Antigravity to take over the flight lead and synthesize the surgical fix?
    </p>

    <div style='text-align:center;margin-bottom:24px;'>
      <a href='{approve_url}' class='btn-cloud'>⚡ Authorize Antigravity Cloud Pilot</a>
      <a href='{servicedesk_url}' class='btn-desk'>🔍 Hold for Manual Dev</a>
    </div>

    <p style='font-size:11px;color:#64748b;text-align:center;'>
      🔒 HiTM Gate 2 Active: Zero tokens will be consumed and zero data will leave the local LAN without your authorization.
    </p>
  </div>
  <div class='ftr'>Dataholics Resolve • TiRA v2.0 Project Nemik Architecture</div>
</div></body></html>"""

    email_subject = f"[TiRA Nemik HiTM #2] Ticket #{ticket_id}: Authorization for Antigravity Cloud Pilot ({safe_company})"
    return send_smtp_email(CTO_EMAIL, email_subject, html_email, from_name="TiRA v2.0 Project Nemik")


if __name__ == "__main__":
    print(f"Nemik HiTM Bridge loaded. Target CTO: {CTO_EMAIL}")
