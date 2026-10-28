# Hackterm — an XScreenSaver hack

A wall of consoles where a fictional intruder scans, brute-forces, escalates
and exfiltrates. Every IP address, password and hash is generated at random:
none of it refers to anything real, and no network is touched.

Written for XScreenSaver 6.x (X11 / Xft), MIT licence like the rest of the
tree.

🇫🇷 Version française : [README.md](README.md)

![Hackterm en plein écran, random layout, with a wallpaper](docs/hackterm.png)

## Contents

    hacks/hackterm.c             the hack (C, ~1630 lines)
    hacks/config/hackterm.xml    settings panel for xscreensaver-settings
    scripts/build-hack.sh        manual build (common objects included)
    scripts/bg-convert.sh        converts a wallpaper to PNG
    scripts/release.sh           writes a tag + release

## What it does

Each console is drawn as a terminal window: dark grey title bar, centred
title, minimise / maximise / close buttons on the right, and the text
directly under the bar.

Up to 6 independent panels, each with its own role and prompt (`root@kali`,
`root@target`, `root@sniffer`, `msf6`). Each panel picks at random among 34
scenarios and never replays the same one twice in a row:

- reconnaissance: nmap, gobuster, nikto, DNS zone transfer, ldap
- access: hydra, ssh, sqlmap, kerberoast, LFI, exposed .env, public S3
- execution: msfconsole, python exploit, reverse shell, docker (API 2375)
- post-exploitation: /etc/shadow, private SSH key, persistent crontab, POSIX
  capabilities, kernel CVE, redis, mysql, smb, aircrack, john

The command is typed character by character (random pauses after spaces),
the block cursor blinks, then the output falls line by line — fast for local
work, slow for anything that mimics a network scan. The scrollback scrolls
and each panel moves on to a new scenario after 1 to 3 seconds.

## Options

All options take the form `-option value`. Defaults are in parentheses.

| option | default | effect |
|---|---|---|
| `-panels N` | `4` | number of consoles, 1 to 6 |
| `-theme NAME` | `phosphor` | colour palette, see below |
| `-font FAMILY` | `Monospace` | fontconfig family, or a full Xft spec |
| `-font-size N` | `11` | font size in points |
| `-speed N` | `1.0` | typing speed; 0.2 = very slow, 4 = fast |
| `-layout grid\|random` | `grid` | regular grid, or windows scattered at random |
| `-overlap` / `-no-overlap` | `-overlap` | in random layout, allow or forbid overlap |
| `-move-interval SEC` | `15` | in random layout, seconds between moves; `0` = never |
| `-bg-image FILE` | none | PNG wallpaper instead of the vector backdrop |
| `-titles` / `-no-titles` | `-titles` | show or hide the title bars |
| `-delay µs` | `30000` | delay between frames (30000 ≈ 33 fps) |
| `-help` | — | list the options and exit |

The hack also accepts the usual XScreenSaver options: `--root`,
`--window-id`, `--visual`, `--fps` / `--no-fps`, `--mono`.

### Themes

`phosphor` (default) is 100 % monochrome: all text, titles included, is the
same green `#33ff33` on pure black. The others vary:

    phosphor    single green on black
    green       green + cyan/yellow/red/magenta accents
    amber       orange tube, warm tones
    blue        night-blue background, icy cyan
    dracula     dark purple, neon green and pink
    solarized   #002b36 petrol blue, grey-green text
    mono        black and white
    red         reddened black, red accents

### Random layout

`-layout random` arranges the consoles as windows dropped at random: varied
sizes and positions, overlaps allowed. The backdrop is then a technical
decor (grid, radar rings, circuit traces, scraps of text) — or your own
picture if `-bg-image` is given.

Two safeguards: windows are painted largest first, so a small console sits in
front of a big one instead of being buried; and each placement avoids
covering more than 12 % of a neighbour.

- `-no-overlap` forbids any overlap: up to 480 positions tried, with
  progressively smaller windows if the screen is crowded. A periodic move is
  cancelled when no free spot exists.
- `-move-interval N` sets how often windows move (`0` freezes them).

### Wallpaper

`-bg-image /path/image.png` replaces the vector backdrop with your picture:
scaled to "cover" (aspect ratio preserved, centre-cropped) and darkened to
65 % so the consoles stay readable.

**PNG only**: the XScreenSaver image loader does not decode JPEG (libjpeg
was not compiled in). Linux Mint wallpapers are almost all JPEG, so convert
them once:

    scripts/bg-convert.sh /usr/share/backgrounds/linuxmint/sele_ring.jpg
    # -> ~/.local/share/hackterm/sele_ring.png

Then:

    ./hackterm -layout random -bg-image ~/.local/share/hackterm/sele_ring.png

The script takes an output path as second argument, caps the image at
2560 px wide and falls back to `convert` or PIL when ffmpeg is missing.

## Prebuilt binary

A ready-to-use archive is attached to every release:

    https://github.com/marcpm2510/xscreensaver-hacks/releases/latest

    gh release download -R marcp2510/xscreensaver-hacks -p "*.tar.gz"
    tar xzf hackterm-1.0.0-linux-x86_64.tar.gz
    sudo install -m 755 hackterm /usr/libexec/xscreensaver/hackterm
    sudo install -m 644 hackterm.xml /usr/share/xscreensaver/config/hackterm.xml
    xscreensaver-command -restart

Then add `hackterm --root` to the `programs:` section of `~/.xscreensaver` —
the daemon does not regenerate that file on its own.

Built on Linux Mint 22 / Ubuntu 24.04, x86_64, glibc ≥ 2.38; dynamically
linked against libX11, libXt, libXft, libfontconfig, libpng16. On an older
distribution, rebuild from source (next section).

## Publishing a release

Binaries ship as tagged releases (`v1.0.0`, `v1.1.0`, …). Everything is
scripted:

    ./scripts/release.sh 1.1.0 --dry-run   # show what would be published
    ./scripts/release.sh 1.1.0             # tag + push + GitHub release

The script takes the compiled binary from `~/xscreensaver/hacks/hackterm`
(override with `HACKTERM_SRC`), strips it, packs it with the XML and
`bg-convert.sh` into `dist/hackterm-<version>-linux-x86_64.tar.gz`, creates
the git tag, pushes it, then publishes the release with notes generated from
the commits. It refuses to reuse an existing tag.

## Building

From a configured XScreenSaver tree (`./configure` already run):

    cd ~/xscreensaver/hacks
    cp ~/xscreensaver-hacks/hacks/hackterm.c .
    cp ~/xscreensaver-hacks/scripts/{build-hack.sh,bg-convert.sh} scripts/
    sh scripts/build-hack.sh hackterm

`build-hack.sh` compiles the hack and links it against the common objects
(`screenhack`, `ximage-loader`, `ansi-tty`, `colors`, `hsv`, `textclient`…)
with `-DSTANDALONE -DHAVE_CONFIG_H`; without those defines PNG loading is
compiled out of the binary.

## Testing

    export DISPLAY=:0
    xscreensaver-command -deactivate
    ./hackterm -panels 4 -speed 1.6

A few useful combinations:

    ./hackterm -panels 6 -font-size 9                   dense wall
    ./hackterm -layout random -panels 5 -no-overlap     scattered windows
    ./hackterm -theme amber -font "Classic Console" -font-size 8 -no-titles
    ./hackterm -layout random -bg-image ~/.local/share/hackterm/sele_ring.png

## Installing into the daemon

    sudo install -m 755 hackterm /usr/libexec/xscreensaver/hackterm
    sudo install -m 644 ~/xscreensaver-hacks/hacks/config/hackterm.xml \
         /usr/share/xscreensaver/config/hackterm.xml
    xscreensaver-command -restart

The hack then shows up as "Hackterm" in the list, with its settings panel:
number of consoles, font size and family, speed, layout, overlap, move
interval, wallpaper, theme, title bars.

## Notes

- No binary and no `.o` file is versioned here.
- The hack is purely decorative: the output is generated for atmosphere, no
  network is touched.
- An unknown font name falls back to `fixed`, an unknown theme to
  `phosphor`, a non-PNG image is ignored with an explicit message.
