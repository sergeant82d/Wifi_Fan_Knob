# A running list of things to attend to during active sessions.

## 20260926

- [x] Webserver - Main page - Quick Start Preset buttons - need to change color and act like Radio buttons, similar to those at the bottom of the page in the "Display Mode" section. (done 2026-09-26, 7190fb5)

- [x] Same page - "Manual Speed Control" slider - make it react in real time, not needing the "Apply Speed" button, which is then removed. (done 2026-09-26, 7190fb5)

- [x] Webserver - Create a new page - "Fan Settings" - Place it between WiFi and Config tabs. Move everything specific to the fan from the Config page to this new one. (done 2026-09-26, 7190fb5)

- [x] Webserver - top of all pages - "Status: WiFi" - ADD the network SSID or name. Something like "Status: WiFi Connected to 'ssid/name' (done 2026-09-26, 7190fb5)

- [ ] Reference the "Automatic" mode on the LCD after the BME688 arrives - Since we moved the IP address to the settings page, we can extend the ends of the RPM bar lower, back either to, or nearer, 270 degrees sweep.

- [x] On the LCD Home page - move the "Settings" page from the right side, to the left side, so that when the fan is not running, a knob turn left will move to it; from there, a right turn will return to Home. (done 2026-09-26, 1130099)

- [x] On the LCD Settings page - add a small button to disable the screensaver (set it's time to 0). This should not be a permanent change like the web page provides, but only last until it is toggled off. Please discuss options for resetting this other than a button toggle. (done 2026-09-26, 1130099)

- [x] Webserver - Config page - the Time Zone and Format settings got moved out of a card - please straighten that out. (done 2026-09-26, 1130099)


**Questions:**
1. Will this present what I think is called 'mDNS'? Where it will provide a name to the LAN, so I can type, for example, "fanknob.local" in a browser when I'm on the network? If so, add a name block where I can set that name.

   Answer: yes. Device name field added (WiFi tab -> Network Configuration, default "fanknob"): http://fanknob.local:8080. Some Android phones don't support .local names; the IP still works. (done 2026-09-26, 7190fb5)

	- Is there a way to keep the port :8080, but not have to type it?

	  Answer: yes. Port 80 now redirects to :8080, so just type fanknob.local (or the IP) in the browser. (done 2026-09-26, 1130099)


---
