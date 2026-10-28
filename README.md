# Hackterm — un hack XScreenSaver

Un mur de consoles où un intrus fictif scanne, brute-force, escalade et
exfiltre. Toutes les adresses IP, tous les mots de passe et tous les hashs
sont générés au hasard : rien ne référence quoi que ce soit de réel, et
aucun réseau n'est touché.

Écrit pour XScreenSaver 6.x (X11 / Xft), licence MIT comme le reste de
l'arbre.

## Contenu

    hacks/hackterm.c             le hack (C, ~1200 lignes)
    hacks/config/hackterm.xml    panneau de réglages pour xscreensaver-settings
    scripts/build-hack.sh        compilation à la main (objets communs inclus)

## Ce que ça fait

Chaque console est dessinée comme une fenêtre GNOME Terminal : barre de
titre anthracite, titre centré, boutons réduire/agrandir/fermer à droite,
et le texte directement sous la barre.

Jusqu'à 6 panneaux indépendants, chacun avec son rôle et son invite
(`root@kali`, `root@target`, `root@sniffer`, `msf6`). Chaque panneau tire
au hasard parmi 34 scénarios et ne rejoue jamais deux fois de suite le
même :

- reconnaissance : nmap, gobuster, nikto, transfert de zone DNS, ldap
- accès : hydra, ssh, sqlmap, kerberoast, LFI, .env exposé, S3 public
- exécution : msfconsole, exploit python, reverse shell, docker (API 2375)
- post-exploitation : /etc/shadow, clé SSH privée, crontab persistante,
  capabilities POSIX, CVE noyau, redis, mysql, smb, aircrack, john

La commande s'écrit caractère par caractère (pauses aléatoires après les
espaces), le curseur bloc clignote, puis la sortie tombe ligne par ligne —
vite pour du local, lentement pour ce qui simule un scan réseau. Le
scrollback défile et chaque panneau repart sur un nouveau scénario après
1 à 3 secondes.

## Thèmes

`phosphor` (défaut) est 100 % monochrome : tout le texte, titres compris,
est du même vert `#33ff33` sur noir pur. Les autres sont là pour varier :
`green` (vert + accents colorés), `amber`, `blue`, `dracula`, `solarized`,
`mono`, `red`.

## Compiler

Depuis un arbre XScreenSaver configuré (`./configure` déjà passé) :

    cd ~/xscreensaver/hacks
    cp ~/xscreensaver-hacks/hacks/hackterm.c .
    mkdir -p scripts && cp ~/xscreensaver-hacks/scripts/build-hack.sh scripts/
    sh scripts/build-hack.sh hackterm

## Tester

    export DISPLAY=:0
    xscreensaver-command -deactivate
    ./hackterm -panels 4 -speed 1.6

Options : `-panels 1..6`, `-speed 0.2..4`, `-theme <nom>`, `-font`,
`-font-size`, `-titles` / `-no-titles`, `-help`.

## Installer dans le démon

    sudo install -m 755 hackterm /usr/libexec/xscreensaver/hackterm
    sudo install -m 644 ~/xscreensaver-hacks/hacks/config/hackterm.xml \
         /usr/share/xscreensaver/config/hackterm.xml
    xscreensaver-command -restart

Le hack apparaît alors sous « Hackterm » dans la liste, avec son panneau
de réglages (terminaux, thème, police, taille, vitesse, barres de titre).

## Notes

- Aucun binaire ni fichier `.o` n'est versionné ici.
- Ce hack est purement décoratif : les sorties sont générées pour
  l'ambiance, aucun réseau n'est touché.
