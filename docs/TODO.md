CLAUDE Code notes for WiFi Fan Knob

## I am going to keep this as a running list of things to attend to during active sessions.



20260926

 - [x] Webserver - Main page - Quick Start Preset buttons - need to change color and act like Radio buttons, similar to those at the bottom of the page in the "Display Mode" section. (done 2026-09-26, 7190fb5)

 - [x] Same page - "Manual Speed Control" slider - make it react in real time, not needing the "Apply Speed" button, which is then removed. (done 2026-09-26, 7190fb5)

 - [x] Webserver - Create a new page - "Fan Settings" - Place it between WiFi and Config tabs. Move everything specific to the fan from the Config page to this new one. (done 2026-09-26, 7190fb5)

 - [x] Webserver - top of all pages - "Status: WiFi" - ADD the network SSID or name. Something like "Status: WiFi Connected to 'ssid/name' (done 2026-09-26, 7190fb5)



Question? - Will this present what I think is called 'mDNS'? Where it will provide a name to the LAN, so I can type, for example, "fanknob.local" in a browser when I'm on the network? If so, add a name block where I can set that name.

   Answer: yes. Device name field added (WiFi tab -> Network Configuration, default "fanknob"): http://fanknob.local:8080. Some Android phones don't support .local names; the IP still works. (done 2026-09-26, 7190fb5)

