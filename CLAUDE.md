# WiFi Fan Knob

ESP32-S3 fan controller for a solder fume extractor, on the Elecrow CrowPanel 1.28" rotary
display. Full project guide, hardware notes, decisions and to-do list: **`docs/CLAUDE.md`**.
Read it before starting work.

## Working principles

1. Don't assume. Don't hide confusion. Surface tradeoffs.
2. Minimum code that solves the problem. Nothing speculative.
3. Touch only what you must. Clean up only your own mess.
4. Define success criteria. Loop until verified.

## Essentials

- **The user's to-do list is `docs/TODO.md`.** Check it at the start of a session. Work items
  only when the user asks; when one is done, tick it (`- [x]`) and add the date and commit.
  Questions there need an answer, not code.
- Build and flash: `~/.platformio/penv/Scripts/pio.exe run -t upload` (board on COM13, native
  USB). Uploads often fail with "port busy / access denied" (UPS software grabs the port):
  retry a few times. Close any serial reader of your own before flashing.
- Board: `http://192.168.10.102:8080` (static IP). `/api/status`, `/api/config` and
  `/api/fans` read without a login; changes need the user's web login.
- Edits containing `\n` or special characters: write a Python script with the Write tool and
  run it. Bash heredocs mangle escapes. Check the web page script with node after editing
  `web/index.html`.
- `docs/DISPLAY_GUIDE.md`: how to change the LCD (for the user). Keep it current with LCD changes.
- Status reports: `docs/Status_Reports/STATUS_REPORT_NN.md`. The user asks for the previous
  day's report before starting a new day's work; write it only when asked.
- Commit and push when the user confirms a change works on the board; mark anything
  untested as such in commit messages and docs.
