# KopSkoot – Install & Setup Guide

This takes about **5 minutes**. You need **Windows 10/11 (64-bit)** and **OBS Studio 31 or newer**.

---

## 1. Install

1. **Close OBS completely**, including the icon in the system tray near the clock.
2. Download **`KopSkoot-vX.X.X-windows.zip`** from the [Releases page](../../releases) and **unzip** it (right-click → *Extract All*).
3. Inside you'll find two folders: **`bin`** and **`data`**.
4. Open File Explorer, paste this into the address bar and press Enter:
   ```
   C:\ProgramData\obs-studio\plugins
   ```
   If the folder doesn't exist, create it. *ProgramData* is a hidden folder, but pasting the address still works.
5. Make a new folder there called **`realm-killsense`**, so you end up with:
   ```
   C:\ProgramData\obs-studio\plugins\realm-killsense\
   ```
   (The folder keeps this name for now so that your settings carry over between versions.)
6. Copy the **`bin`** and **`data`** folders into it. If Windows asks, choose **Replace the files**.
7. Start OBS. Go to **Docks → KopSkoot** and the black-and-red KopSkoot panel appears. You can drag it anywhere.

> **Updating?** Do exactly the same: close OBS, copy the new `bin` and `data` over the old ones, choose **Replace**. Your settings and gamertag are kept.

---

## 2. First-time setup

In the **KopSkoot** dock:

1. **Source:** pick the OBS source that shows your game, e.g. *Game Capture* or your capture card (*Xbox Capture*, *Elgato*, …). Press **Refresh** if it's not in the list.
2. **Game profile:** pick your game, e.g. **THE FINALS** or **World of Warships: Legends**.
3. **Player → Set gamertag:** type your in-game name exactly as it shows in the game, then OK.
4. Press **Start Detection**. *Detector state* should say **Running**.
5. **THE FINALS only:** in a match, while you are alive and your name shows bottom-left, press **Capture my name plate**. Check the yellow box is around your name, then **Save**. This stops kills from counting while you spectate.
6. Press **Test Kill Event**. *Kills this session* should go up by 1. Then press **Reset Session** to start from 0.

Tip: in **Settings**, tick *Start detection automatically when OBS starts*.

---

## 3. Show kills on stream (optional)

**Free counter**
1. In OBS, add a **Text (GDI+)** source to your scene.
2. KopSkoot dock → **Settings → Event Outputs → On-screen counter** → pick that text source.
3. Change the text if you like, e.g. `KILLS {kills} | DEATHS {deaths} | K/D {kd}`.

**Animated overlays & alerts**
KopSkoot sends every kill and death over **OBS WebSocket** (*Tools → WebSocket Server Settings → Enable*). Premium REALM overlay styles (animated pop-ups, streak banners, game themes) are available to Patreon members. Streamer.bot and similar tools can listen for KopSkoot events too (vendor name `RealmKillSense`).

---

## 4. Troubleshooting

| Problem | Fix |
|---|---|
| No **KopSkoot** under *Docks* | Check the folders are at `C:\ProgramData\obs-studio\plugins\realm-killsense\bin\64bit\` and `...\data\`. Restart OBS. |
| Detector says *Waiting for source* | The chosen source is hidden or turned off. Pick the source that actually shows your game. |
| Kills not counted | Correct source and game profile? Game at **1920×1080** with the default HUD size? Open **Debug View** during a kill and send us a screenshot. |
| Kills counted while spectating | Set your gamertag and use **Capture my name plate** (THE FINALS). |
| Too many kills counted | Make sure you're on the newest version and game profile, then report it with your OBS log. |
| "OCR unavailable" | Windows needs an English language pack: *Settings → Time & language → Language & region*. |

**Report a bug:** in OBS go to **Help → Log Files → Upload Current Log File**, copy the link, and post it with a screenshot of the dock in an **Issue** on GitHub or in the Ultimate Beardy Discord.

---

## 5. Uninstall

Close OBS and delete:
```
C:\ProgramData\obs-studio\plugins\realm-killsense
```
To remove your settings too, also delete `%APPDATA%\obs-studio\plugin_config\realm-killsense`.

---

<sub>KopSkoot runs entirely on your PC, never touches your game, and never uploads anything. It is an independent plugin, not affiliated with the OBS Project or any game publisher.</sub>
