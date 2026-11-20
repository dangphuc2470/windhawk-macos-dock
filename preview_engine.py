import re
import os
import sys
import time
import subprocess
from pathlib import Path

WORKSPACE = Path(r"c:\ProgramData\Windhawk\EditorWorkspace")
MOD_CPP = WORKSPACE / "mod.wh.cpp"
ARTIFACT_DIR = Path(r"C:\Users\phucdnh\.gemini\antigravity-ide\brain\6b9edc8c-8f2c-4c3c-84ce-253fc1d1bd7e")
OUTPUT_HTML = ARTIFACT_DIR / "preview.html"
OUTPUT_PNG = ARTIFACT_DIR / "dock_preview.png"
EDGE_EXE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"

def extract_params(cpp_text):
    params = {
        "kBadgeOffsetX": -2.0,
        "kBadgeOffsetY": 2.0,
        "kBadgeOutlineThickness": 0.0,
        "kInnerIconScale": 0.85,
        "iconSize": 48.0,
    }
    
    m_ox = re.search(r"constexpr\s+float\s+kBadgeOffsetX\s*=\s*([-\d\.]+)f?;", cpp_text)
    if m_ox: params["kBadgeOffsetX"] = float(m_ox.group(1))
    
    m_oy = re.search(r"constexpr\s+float\s+kBadgeOffsetY\s*=\s*([-\d\.]+)f?;", cpp_text)
    if m_oy: params["kBadgeOffsetY"] = float(m_oy.group(1))
    
    m_ot = re.search(r"constexpr\s+float\s+kBadgeOutlineThickness\s*=\s*([-\d\.]+)f?;", cpp_text)
    if m_ot: params["kBadgeOutlineThickness"] = float(m_ot.group(1))
    
    m_scale = re.search(r"constexpr\s+double\s+kInnerIconScale\s*=\s*([-\d\.]+);", cpp_text)
    if m_scale: params["kInnerIconScale"] = float(m_scale.group(1))
    
    return params

def generate_html(params):
    icon_size = params["iconSize"]
    corner_radius = icon_size * 0.225
    scale = params["kInnerIconScale"]
    badge_ox = params["kBadgeOffsetX"]
    badge_oy = params["kBadgeOffsetY"]
    outline = params["kBadgeOutlineThickness"]
    
    outline_css = f"border: {outline}px solid #FFFFFF;" if outline > 0 else "border: none;"
    
    html = f"""<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta http-equiv="refresh" content="2">
<title>Windhawk MacOS Dock - Compose Preview</title>
<style>
  * {{ box-sizing: border-box; margin: 0; padding: 0; }}
  body {{
    background: #18181c;
    font-family: -apple-system, BlinkMacSystemFont, "SF Pro Text", "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    min-height: 100vh;
    overflow: hidden;
  }}
  .preview-wrapper {{
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 20px;
  }}
  .dock-container {{
    background: rgba(255, 255, 255, 0.18);
    backdrop-filter: blur(30px) saturate(180%);
    -webkit-backdrop-filter: blur(30px) saturate(180%);
    border: 1px solid rgba(255, 255, 255, 0.25);
    border-radius: 20px;
    padding: 10px 18px 12px 18px;
    display: flex;
    align-items: flex-end;
    gap: 16px;
    box-shadow: 0 16px 36px rgba(0, 0, 0, 0.4), inset 0 1px 0 rgba(255, 255, 255, 0.3);
  }}
  .item-slot {{
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 4px;
  }}
  .button-container {{
    position: relative;
    width: {icon_size}px;
    height: {icon_size}px;
    display: flex;
    align-items: center;
    justify-content: center;
  }}
  /* MacAppBackground (White rounded card behind icon) */
  .mac-card {{
    position: absolute;
    width: {icon_size}px;
    height: {icon_size}px;
    background: #FFFFFF;
    border-radius: {corner_radius}px;
    border: 0.5px solid rgba(0, 0, 0, 0.08);
    box-shadow: 0 3px 8px rgba(0, 0, 0, 0.18);
    z-index: 1;
  }}
  /* Inner scaled icon */
  .inner-icon {{
    position: absolute;
    width: {icon_size}px;
    height: {icon_size}px;
    transform: scale({scale});
    transform-origin: center center;
    z-index: 2;
    display: flex;
    align-items: center;
    justify-content: center;
    border-radius: {corner_radius}px;
    overflow: hidden;
  }}
  .app-svg {{
    width: 100%;
    height: 100%;
  }}
  /* Badge notification */
  .badge {{
    position: absolute;
    top: 0;
    right: 0;
    z-index: 999;
    transform: translate({badge_ox}px, {badge_oy}px);
    background: #FF3B30;
    min-width: 17px;
    height: 17px;
    border-radius: 99px;
    padding: 0 4.5px;
    display: flex;
    align-items: center;
    justify-content: center;
    color: #FFFFFF;
    font-size: 10.5px;
    font-weight: 700;
    line-height: 1;
    {outline_css}
    box-shadow: 0 2px 5px rgba(0, 0, 0, 0.35);
  }}
  .badge-text {{
    transform: translateY(-0.5px);
  }}
  /* Running indicator dot */
  .dot {{
    width: 4px;
    height: 4px;
    border-radius: 50%;
    background: rgba(255, 255, 255, 0.85);
    box-shadow: 0 0 4px rgba(255, 255, 255, 0.6);
  }}
  .dot-spacer {{
    height: 4px;
  }}
  /* Divider */
  .dock-divider {{
    width: 1px;
    height: 38px;
    background: rgba(255, 255, 255, 0.2);
    margin: 0 2px;
    align-self: center;
  }}
  .info-bar {{
    background: rgba(255, 255, 255, 0.06);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 12px;
    padding: 6px 18px;
    display: flex;
    gap: 20px;
    font-size: 11.5px;
    color: #8e8e93;
    font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  }}
  .info-bar b {{
    color: #30d158;
    font-weight: 600;
  }}
</style>
</head>
<body>
<div class="preview-wrapper">
  <div class="dock-container">
    <!-- Finder -->
    <div class="item-slot">
      <div class="button-container">
        <div class="mac-card"></div>
        <div class="inner-icon">
          <svg class="app-svg" viewBox="0 0 48 48">
            <defs>
              <linearGradient id="finderGrad" x1="0%" y1="0%" x2="0%" y2="100%">
                <stop offset="0%" stop-color="#4CB5F5"/>
                <stop offset="100%" stop-color="#1F77D0"/>
              </linearGradient>
            </defs>
            <rect width="48" height="48" rx="10" fill="url(#finderGrad)"/>
            <circle cx="17" cy="19" r="2.5" fill="#1F77D0"/>
            <circle cx="31" cy="19" r="2.5" fill="#1F77D0"/>
            <path d="M 15 27 Q 24 35 33 27" fill="none" stroke="#1F77D0" stroke-width="2.5" stroke-linecap="round"/>
          </svg>
        </div>
      </div>
      <div class="dot"></div>
    </div>

    <!-- Safari -->
    <div class="item-slot">
      <div class="button-container">
        <div class="mac-card"></div>
        <div class="inner-icon">
          <svg class="app-svg" viewBox="0 0 48 48">
            <defs>
              <linearGradient id="safariGrad" x1="0%" y1="0%" x2="100%" y2="100%">
                <stop offset="0%" stop-color="#FF5E3A"/>
                <stop offset="100%" stop-color="#FF2A68"/>
              </linearGradient>
            </defs>
            <rect width="48" height="48" rx="10" fill="url(#safariGrad)"/>
            <circle cx="24" cy="24" r="14" fill="none" stroke="white" stroke-width="2"/>
            <polygon points="24,14 27,24 24,34 21,24" fill="white"/>
          </svg>
        </div>
      </div>
      <div class="dot"></div>
    </div>

    <!-- Messages (Single Digit Badge "1") -->
    <div class="item-slot">
      <div class="button-container">
        <div class="mac-card"></div>
        <div class="inner-icon">
          <svg class="app-svg" viewBox="0 0 48 48">
            <defs>
              <linearGradient id="msgGrad" x1="0%" y1="0%" x2="100%" y2="100%">
                <stop offset="0%" stop-color="#34C759"/>
                <stop offset="100%" stop-color="#28CD41"/>
              </linearGradient>
            </defs>
            <rect width="48" height="48" rx="10" fill="url(#msgGrad)"/>
            <path d="M12 24 C12 17.5 17.5 12 24 12 C30.5 12 36 17.5 36 24 C36 30.5 30.5 36 24 36 C21.5 36 19.2 35.2 17.3 33.8 L12 36 L13.5 30.8 C12.5 28.8 12 26.5 12 24 Z" fill="white"/>
          </svg>
        </div>
        <div class="badge">
          <div class="badge-text">1</div>
        </div>
      </div>
      <div class="dot"></div>
    </div>

    <!-- Mail (Double Digit Badge "12") -->
    <div class="item-slot">
      <div class="button-container">
        <div class="mac-card"></div>
        <div class="inner-icon">
          <svg class="app-svg" viewBox="0 0 48 48">
            <defs>
              <linearGradient id="mailGrad" x1="0%" y1="0%" x2="100%" y2="100%">
                <stop offset="0%" stop-color="#00C6FF"/>
                <stop offset="100%" stop-color="#0072FF"/>
              </linearGradient>
            </defs>
            <rect width="48" height="48" rx="10" fill="url(#mailGrad)"/>
            <rect x="12" y="15" width="24" height="18" rx="3" fill="none" stroke="white" stroke-width="2.5"/>
            <polyline points="12,17 24,26 36,17" fill="none" stroke="white" stroke-width="2.5"/>
          </svg>
        </div>
        <div class="badge">
          <div class="badge-text">12</div>
        </div>
      </div>
      <div class="dot-spacer"></div>
    </div>

    <div class="dock-divider"></div>

    <!-- Trash -->
    <div class="item-slot">
      <div class="button-container">
        <div class="mac-card"></div>
        <div class="inner-icon">
          <svg class="app-svg" viewBox="0 0 48 48">
            <rect width="48" height="48" rx="10" fill="#E5E5EA"/>
            <path d="M16 16 L32 16 M20 16 L20 13 C20 12 21 11 22 11 L26 11 C27 11 28 12 28 13 L28 16 M18 16 L19 35 C19 36 20 37 21 37 L27 37 C28 37 29 36 29 35 L30 16" fill="none" stroke="#8E8E93" stroke-width="2.5" stroke-linecap="round"/>
          </svg>
        </div>
      </div>
      <div class="dot-spacer"></div>
    </div>
  </div>

  <div class="info-bar">
    <div>OffsetX: <b>{badge_ox}px</b></div>
    <div>OffsetY: <b>{badge_oy}px</b></div>
    <div>Scale: <b>{scale}</b></div>
    <div>Outline: <b>{outline}px</b></div>
  </div>
</div>
</body>
</html>
"""
    return html

def render():
    if not MOD_CPP.exists():
        print(f"File not found: {MOD_CPP}")
        return False
    cpp_text = MOD_CPP.read_text(encoding="utf-8")
    params = extract_params(cpp_text)
    
    html = generate_html(params)
    OUTPUT_HTML.write_text(html, encoding="utf-8")
    
    cmd = [
        EDGE_EXE,
        "--headless",
        f"--screenshot={OUTPUT_PNG}",
        "--window-size=620,240",
        "--hide-scrollbars",
        f"file:///{str(OUTPUT_HTML).replace(os.sep, '/')}"
    ]
    subprocess.run(cmd, check=True, capture_output=True)
    print(f"[PREVIEW RENDERED] Parameters: {params}")
    return True

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--watch":
        print("Watching mod.wh.cpp for live Jetpack-Compose style feedback loop...")
        last_mtime = 0
        while True:
            try:
                mtime = os.path.getmtime(MOD_CPP)
                if mtime != last_mtime:
                    last_mtime = mtime
                    render()
                time.sleep(0.5)
            except KeyboardInterrupt:
                break
    else:
        render()
