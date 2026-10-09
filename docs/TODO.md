# To do

How this list works (the all-projects format, agreed 2026-10-07; from ETH_Touch_PWM):
- **Your notes** (top): write anything for Claude here. At the start of a session Claude reads
  it, answers questions and summarises the open tasks; when the day's status report is written,
  the notes are turned into items below and this section is emptied.
- **Open**: oldest first. **Done**: newest first, with date and commit. Done items older than
  about two weeks move to `docs/TODO_DONE.md`.
- Tags (alphabetical; the same set in every project, copy one into an item):
  - `[Board]` needs a board on the bench
  - `[Decide]` needs a decision
  - `[Docs]` documentation
  - `[Eye]` screensaver / standby eye
  - `[HA]` Home Assistant / MQTT
  - `[LCD]` round screen
  - `[Net]` WiFi / network
  - `[Question]` needs an answer
  - `[SD]` SD card (ETH_Touch_PWM)
  - `[Sensor]` air, light and presence sensors
  - `[Web]` web page

## Your notes

- `[Web]` `[HA]` Add Auto mode minimum and maximum VOC settings to web config page and HA. 


## Open

- [ ] `[LCD]` Reference the "Automatic" mode on the LCD after the BME688 arrives - Since we moved
      the IP address to the settings page, we can extend the ends of the RPM bar lower, back
      either to, or nearer, 270 degrees sweep. (2026-09-26)
      Claude: the sensor is now the SGP41 (fitted 2026-10-06); this waits on Auto mode below
- [ ] `[LCD]` LCD - When we adjust the LCD buttons for the BME-688 sensor & auto-mode, see if we
      can't keep the four main speed buttons - LOW/MED/HIGH/MAX - above the horizontal. The
      bottom edge of the LOW button sitting at the 270* line, and the bottom of the MAX button
      sitting at 90*. Then the OFF and AUTO buttons extend lower from those points, each of them
      a further 45*, or whatever we restrict the drawing to in order to stay clear of the Clock.
      (2026-09-26)
      Claude: do it together with the item above and Auto mode
- [ ] `[Sensor]` `[HA]` `[Decide]` Auto mode from the VOC Index: pick the thresholds from a day or two
      of VOC history in HA (VOC learning started 2026-10-06; each restart restarts it). Auto
      ignores the index for about the first hour after a restart (option A, user 2026-10-06)
- [ ] `[Sensor]` `[Board]` Presence: AMG8833 thermal camera ordered (user, 2026-10-06). When it
      arrives: wire it to the always-on 3.3 V and SDA/SCL (GPIO 38/39), check the boot log lists
      0x69, test (away, sitting, leaning in, hot iron with nobody there, a long still sit), then
      the "presence for X seconds wakes the screen" rule
- [ ] `[Board]` GPIO 4 pull-down resistor: waits on the FPC breakout for GPIO 4 access
      (status report 05)
- [ ] `[Web]` Card titles in Title Case, the all-projects rule added to `D:\GitHub\WEB_STYLE.md`
      section 5 (user, 2026-10-08; done in ETH_Touch_PWM `d3cd279`): e.g. `Fan Settings`,
      `Display Mode`, `Firmware Update (OTA)`. Small words lower case unless first; names keep
      their spelling (MQTT, WiFi, LCD, OTA); field labels, hints and buttons stay in sentence case

## Done

- [x] `[LCD]` Swiping between LCD pages is barely usable and very unfriendly: relook at the swipe
      functions (user, 2026-10-07; after the docs split). Measured first: during a swipe LVGL's
      clock ran at 20-80 % of real time and each redraw took 110-240 ms. Fixed: LVGL reads the
      real clock; the RPM arc can't be dragged (swipes from the edge change page; segment taps
      reach the screen edge); two 40-line draw buffers sent by DMA. User: "That did it"
      (2026-10-07, this commit). Not done, by choice: a faster slide animation (later, if wanted)
- [x] `[Docs]` Split the big `docs/CLAUDE.md` into a short root `CLAUDE.md` and
      `docs/PROJECT_HISTORY.md`, like the other projects: the guide renamed (history kept),
      with a current-status section; the root file rewritten short (2026-10-07, this commit)
- [x] `[LCD]` `[Web]` `[HA]` Restart button (your request): LCD on a new System page (left-most;
      firmware version, uptime, Restart held 2 s, fills while held), web System tab card above
      Factory Reset (confirm, login), HA button "Restart" (ignored in the first 30 s after boot,
      so a stray retained message can't loop it). All three checked by the user (2026-10-07,
      this commit)
- [x] `[Web]` Left-side tabs (ETH_Touch_PWM layout, `D:\GitHub\WEB_STYLE.md`), standard tab names
      Dashboard, Fan Control, Home Assistant, Network, System. Sidebar lights WiFi / MQTT / Fan
      controller, top bar with the board's clock and a login dialog; Navy & gold only (no theme
      picker; may be revisited). Your layout changes: OTA below Display & Interface on System;
      on phones only the LCD picture first, Brightness / Auto as its own card between Presence
      & Light and LEDs. Checked by the user on PC and phone (2026-10-07, this commit)
- [x] `[Docs]` TODO.md in the all-projects format (Your notes / Open / Done, tags); the items
      carried over word for word (2026-10-07, this commit)
- [x] `[Web]` `[HA]` Need to add some control over the built-in/hardwired RGB LEDs. Take cues from
      the WLED GitHub project, but I only need a few options. Something like:
      - All LEDs flashing, and I can change the color. Could also use as a warning/caution to user.
      - Rainbow changing colors moving around the knob in a circle.
      - ?? Any suggestions??
      Answer: done 2026-10-06, 584d624. Off, Solid, Flash, Breathe and Rainbow, with colour,
      brightness (capped at 100 of 255) and speed, on the web Home tab and in Home Assistant;
      off in standby (on in standby since 2026-10-07, 429c112). Later ideas: colour from fan
      speed, colour from air quality (SGP41), a warning flash. (written 2026-10-03)
- [x] `[Docs]` Update to STATUS_REPORT_05 - Waiting on you: (done 2026-09-27, 199b230)
      - Uploaded last night's firmware with the eight new eyes (Dragon 3 - 10). They are all
        fine. I received the final image today and it is uploaded and ready to be processed and
        take it's place as Dragon 11.
      - Display Mode card is in place and looks good.
      - Industrial Fan is rewired and works great.
      - NF-A20 Fan Auto Configure re-run complete - tested - deleted as not for this project.
      - Received the final artwork from the artist
- [x] `[Sensor]` `[Docs]` Future BME688 sensor changed to future SGP41 sensor, combined with SHT41
      Temp/Hum sensor to improve the Gas sensor's accuracy. (plan updated in docs/CLAUDE.md)
      (done 2026-09-27, 199b230)
- [x] `[Sensor]` `[Docs]` Notice of possible future hardware - APDS9999 Proximity, Lux Light & Color
      sensor for presence/motion sensing and control. Possibly incorporate into future
      firmware, so plan with that in mind if it needs something from other parts of the
      system. (noted in docs/CLAUDE.md with the presence rule) (done 2026-09-27, 199b230)
- [x] `[Web]` Webserver - Config tab - Move "Save Configuration" button from bottom to in between
      the Display & Interface card, and the Clock card. Most settings on Config will be "set
      and forget", but the eye selection will change fairly often. (second button added,
      bottom one kept) (done 2026-09-27, 199b230)
- [x] `[Docs]` `[Question]` Added eye_blinking.mp4 video from the eye artist in /docs/images as a
      reference. (README video uploaded by the user on github.com, 2026-09-27)
      - Question: Can this .mp4 be put in the README.md, and be visible on the web when
        arriving at the GitHub repo?
        Answer: not from a file in the repo (GitHub only shows a link); uploading it in the
        README editor on github.com gives a player. Done by the user.
- [x] `[Docs]` Repo organization: Created sub-directory - /assets/eye_art/image_sheets (recorded as
      a move, doc paths fixed) (done 2026-09-27, 199b230)
      - Moved the sheets of multiple images there as reference/archival storage, to leave
        -eye_art- as only the active files used in the build. Discuss if there are better
        options.
- [x] `[Web]` Webserver - Main page - Quick Start Preset buttons - need to change color and act like
      Radio buttons, similar to those at the bottom of the page in the "Display Mode" section.
      (done 2026-09-26, 7190fb5)
- [x] `[Web]` Same page - "Manual Speed Control" slider - make it react in real time, not needing
      the "Apply Speed" button, which is then removed. (done 2026-09-26, 7190fb5)
- [x] `[Web]` Webserver - Create a new page - "Fan Settings" - Place it between WiFi and Config
      tabs. Move everything specific to the fan from the Config page to this new one.
      (done 2026-09-26, 7190fb5)
- [x] `[Web]` Webserver - top of all pages - "Status: WiFi" - ADD the network SSID or name.
      Something like "Status: WiFi Connected to 'ssid/name' (done 2026-09-26, 7190fb5)
- [x] `[LCD]` On the LCD Home page - move the "Settings" page from the right side, to the left
      side, so that when the fan is not running, a knob turn left will move to it; from there,
      a right turn will return to Home. (done 2026-09-26, 1130099)
- [x] `[LCD]` On the LCD Settings page - add a small button to disable the screensaver (set it's
      time to 0). This should not be a permanent change like the web page provides, but only
      last until it is toggled off. Please discuss options for resetting this other than a
      button toggle. (done 2026-09-26, 1130099)
- [x] `[Web]` Webserver - Config page - the Time Zone and Format settings got moved out of a card -
      please straighten that out. (done 2026-09-26, 1130099)
- [x] `[Net]` `[Question]` Will this present what I think is called 'mDNS'? Where it will provide a
      name to the LAN, so I can type, for example, "fanknob.local" in a browser when I'm on the
      network? If so, add a name block where I can set that name.
      Answer: yes. Device name field added (WiFi tab -> Network Configuration, default
      "fanknob"): http://fanknob.local:8080. Some Android phones don't support .local names;
      the IP still works. (done 2026-09-26, 7190fb5)
      - Is there a way to keep the port :8080, but not have to type it?
        Answer: yes. Port 80 now redirects to :8080, so just type fanknob.local (or the IP) in
        the browser. (done 2026-09-26, 1130099)
