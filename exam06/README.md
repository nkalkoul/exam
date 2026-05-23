# Exam Rank 06 — `mini_serv`

Guide complet pour comprendre **entièrement** le programme et le réécrire de mémoire le jour de l'exam.

> **Fichier à rendre :** `mini_serv.c` (uniquement)
> **Compilation testée :** `cc -Wall -Wextra -Werror mini_serv.c`

---

## 1. TL;DR — Checklist du jour J

1. Créer `mini_serv.c`.
2. Mettre les **6 includes**.
3. **Copier-coller** `extract_message` + `str_join` depuis le `main.c` fourni (ne pas les réécrire de tête).
4. Écrire **variables globales**, `fatal`, `send_all`.
5. Écrire le `main` : check args → socket/bind/listen → init `fd_set` → boucle `select`.
6. Compiler avec `-Wall -Wextra -Werror`, tester avec **plusieurs `nc`**.
7. Vérifier au caractère près les 5 chaînes obligatoires (section 9).

---

## 2. Le sujet (résumé)

Écrire un serveur de chat. Il écoute sur un **port** (1er argument) sur **127.0.0.1 uniquement** et relaie les messages entre clients.

Comportement exigé :
- **Mauvais nb d'arguments** → écrire sur stderr `Wrong number of arguments\n` et `exit(1)`.
- **Erreur d'un appel système avant d'accepter des connexions** OU **échec d'allocation mémoire** → écrire sur stderr `Fatal error\n` et `exit(1)`.
- Programme **non-bloquant**. Un client peut être lent : s'il ne lit pas, on ne le déconnecte **PAS**.
- **Aucun `#define`** dans le programme.
- **Écoute uniquement 127.0.0.1**.
- À la connexion : le client reçoit un **id** (le 1er = `0`, puis dernier id + 1). On envoie à **tous les autres** : `server: client %d just arrived\n`.
- Un message peut contenir **plusieurs `\n`**. À chaque message reçu, on le renvoie à **tous les autres** clients, **précédé de `client %d: ` devant CHAQUE ligne**.
- À la déconnexion : on envoie à tous les autres `server: client %d just left\n`.
- **Aucune fuite mémoire ni de fd.**
- Envoyer les messages **le plus vite possible**, pas de buffer inutile.

### Fonctions autorisées
```
write, close, select, socket, accept, listen, send, recv, bind,
strstr, malloc, realloc, free, calloc, bzero, atoi, sprintf,
strlen, exit, strcpy, strcat, memset
```
> ⚠️ `printf`, `fcntl`, `signal`, `htons`/`htonl`... `htons`/`htonl` ne sont **pas** dans la liste mais sont nécessaires : ils sont tolérés (macros/réseau). `fcntl` sert **seulement** à tester en local, **jamais** dans le rendu final.

---

## 3. Compiler & tester

```bash
cc -Wall -Wextra -Werror mini_serv.c -o mini_serv
./mini_serv 8080
```

Dans d'autres terminaux (le sujet dit explicitement d'utiliser `nc`) :
```bash
nc 127.0.0.1 8080      # client 0
nc 127.0.0.1 8080      # client 1
```
Taper du texte dans un `nc` → il doit apparaître dans les autres, préfixé `client N: `.

Erreurs à provoquer pour vérifier :
```bash
./mini_serv            # -> "Wrong number of arguments"  (exit 1)
```

---

## 4. Vue d'ensemble (architecture)

C'est un serveur **mono-thread, multiplexé** avec `select()` :

```
        +-----------------------------+
        |   socket d'écoute (sockfd)  |
        +--------------+--------------+
                       |
                  select() bloque jusqu'à activité
                       |
        +--------------+--------------------------------+
        | fd == sockfd ?                                |
        |   OUI -> accept() : nouveau client            |
        |   NON -> recv() :                             |
        |            n<=0  -> déconnexion (close)        |
        |            n>0   -> bufferise + diffuse lignes |
        +-----------------------------------------------+
```

Idée clé : **un seul thread**, on ne bloque jamais. `select()` nous dit quels `fd` sont prêts ; on les traite un par un.

Pourquoi `select` voit aussi l'ensemble **write** (`writes`) ? Le tester de l'exam configure les fd pour que `recv`/`send` **bloquent** si `select` n'a pas été appelé avant. En passant `active` à la fois en lecture **et** en écriture à `select`, on garantit que les `send` qui suivent ne bloqueront pas.

---

## 5. Les structures de données (variables globales)

```c
int     ids[65536];     // ids[fd] = id du client branché sur ce fd
char    *bufs[65536];   // bufs[fd] = données reçues mais pas encore complètes (1 buffer/fd)
int     next_id = 0;    // prochain id à distribuer
fd_set  reads, writes, active;  // active = set maître ; reads/writes = copies pour select
int     maxfd;          // plus grand fd ouvert -> borne des boucles + 1er arg de select
char    tmp[42 + 65536];// buffer scratch pour sprintf (notifications + préfixe)
```

- On **indexe par fd** : simple et rapide, pas de struct ni de liste chaînée.
- `active` est la **source de vérité** des fd ouverts. À chaque tour on fait `reads = writes = active;` car `select()` **modifie** les sets qu'on lui passe.
- `tmp` n'est utilisé que pour les **petites** chaînes (`server: client %d ...` et `client %d: `). Les vrais messages clients sont envoyés directement, pas via `tmp`.

> Note : `fd_set` ne gère réellement que `FD_SETSIZE` (1024) fd. Les tableaux en 65536 sont surdimensionnés ; ça n'a aucune importance pour le tester.

---

## 6. Les 2 fonctions FOURNIES (à copier du `main.c`)

⚠️ **Ne les réécris pas de mémoire.** Le jour J, `main.c` est fourni : copie-les telles quelles. Comprends-les juste pour savoir comment t'en servir.

### `extract_message` — découpe UNE ligne complète
```c
int extract_message(char **buf, char **msg);
```
- Cherche le **premier `\n`** dans `*buf`.
- Si trouvé : met dans `*msg` la ligne complète (jusqu'au `\n` inclus), laisse le **reste** dans `*buf`, retourne **1**.
- Si pas de `\n` (ligne incomplète) : laisse tout dans `*buf`, `*msg = 0`, retourne **0**.
- Échec d'allocation : retourne **-1**.
- ⚠️ L'appelant doit **`free(*msg)`** après usage.

C'est ce qui permet de gérer « un message peut contenir plusieurs `\n` » : on boucle `while (extract_message(...) == 1)` pour sortir les lignes une par une.

### `str_join` — concatène et libère l'ancien
```c
char *str_join(char *buf, char *add);
```
- Retourne un **nouveau** buffer = `buf` + `add`, et **`free(buf)`** au passage.
- Retourne `NULL` si l'allocation échoue.
- Sert à **accumuler** ce qui arrive par bouts : `bufs[fd] = str_join(bufs[fd], buf);`.

---

## 7. Le code expliqué, bloc par bloc

### BLOC 1 — Includes (toujours les mêmes)
```c
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
```

### BLOC 2 — `extract_message` + `str_join`
→ copiées du `main.c` fourni (section 6).

### BLOC 3 — Variables globales
→ section 5.

### BLOC 4 — `fatal` et `send_all`
```c
void fatal(void)
{
    write(2, "Fatal error\n", 12);   // 12 = longueur exacte
    exit(1);
}

void send_all(int except, char *msg)
{
    for (int fd = 0; fd <= maxfd; fd++)
        if (FD_ISSET(fd, &active) && fd != except)
            send(fd, msg, strlen(msg), MSG_NOSIGNAL);
}
```
- `send_all` envoie `msg` à **tous les fd ouverts sauf `except`**.
  - À l'arrivée : `except = client` → le nouveau ne reçoit pas son propre « just arrived ».
  - Sur un message : `except = fd` (l'émetteur) → on ne renvoie pas au sender.
- `MSG_NOSIGNAL` : évite que le programme reçoive **SIGPIPE** (et meure) si on `send` vers un socket dont l'autre bout a fermé la lecture. (`signal()` n'étant pas autorisé, c'est la façon propre de s'en protéger.)

### BLOC 5 — `main`

**5a. Vérification des arguments**
```c
if (ac != 2)
{
    write(2, "Wrong number of arguments\n", 26);  // 26 = longueur exacte
    exit(1);
}
```

**5b. Socket → bind → listen**
```c
int sockfd = socket(AF_INET, SOCK_STREAM, 0);
if (sockfd < 0) fatal();

struct sockaddr_in addr;
bzero(&addr, sizeof(addr));
addr.sin_family = AF_INET;
addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 UNIQUEMENT
addr.sin_port = htons(atoi(av[1]));

if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) fatal();
if (listen(sockfd, 128) < 0) fatal();
```
- `INADDR_LOOPBACK` = 127.0.0.1 → respecte « écouter uniquement 127.0.0.1 ».
- `htons(atoi(av[1]))` : le port en ordre réseau.

**5c. Initialisation du `fd_set`**
```c
FD_ZERO(&active);
FD_SET(sockfd, &active);
maxfd = sockfd;
```

**5d. Boucle principale**
```c
while (1)
{
    reads = writes = active;                       // re-copie : select modifie les sets
    if (select(maxfd + 1, &reads, &writes, 0, 0) < 0) fatal();

    for (int fd = 0; fd <= maxfd; fd++)
    {
        if (!FD_ISSET(fd, &reads)) continue;       // ce fd n'a rien à signaler

        if (fd == sockfd)   // ---- NOUVEAU CLIENT ----
        {
            int client = accept(sockfd, 0, 0);
            if (client < 0) fatal();
            ids[client] = next_id++;               // attribue l'id
            bufs[client] = NULL;                   // pas encore de données
            FD_SET(client, &active);               // l'ajoute au set maître
            if (client > maxfd) maxfd = client;    // met à jour la borne
            sprintf(tmp, "server: client %d just arrived\n", ids[client]);
            send_all(client, tmp);                 // -> tous SAUF le nouveau
        }
        else                // ---- MESSAGE OU DÉCONNEXION ----
        {
            char buf[65536 + 1];
            int n = recv(fd, buf, 65536, 0);

            if (n <= 0)     // 0 = fermeture propre, <0 = erreur -> déconnexion
            {
                sprintf(tmp, "server: client %d just left\n", ids[fd]);
                send_all(fd, tmp);                 // -> tous SAUF celui qui part
                free(bufs[fd]);                    // pas de fuite mémoire
                bufs[fd] = NULL;
                FD_CLR(fd, &active);               // retire du set
                close(fd);                         // pas de fuite de fd
            }
            else            // ---- MESSAGE REÇU ----
            {
                buf[n] = 0;                         // termine la chaîne
                bufs[fd] = str_join(bufs[fd], buf); // accumule
                if (!bufs[fd]) fatal();             // alloc échouée
                char *msg;
                while (extract_message(&bufs[fd], &msg) == 1)
                {
                    sprintf(tmp, "client %d: ", ids[fd]);
                    send_all(fd, tmp);              // préfixe DEVANT chaque ligne
                    send_all(fd, msg);              // la ligne elle-même
                    free(msg);                      // libère la ligne extraite
                }
            }
        }
    }
}
```

Points subtils :
- `recv(fd, buf, 65536, 0)` puis `buf[n] = 0` : indispensable avant de traiter comme une chaîne.
- On **bufferise** (`str_join`) puis on **extrait ligne par ligne** (`extract_message`) → gère les messages partiels et les multi-`\n`.
- Le préfixe `client %d: ` est envoyé **devant chaque ligne** (dans la boucle `while`), pas une fois par `recv`.
- `maxfd` ne fait que **croître** : c'est volontaire et suffisant (on itère un peu plus large, sans bug).

---

## 8. Les chaînes EXACTES (à connaître par cœur)

Une faute de frappe ici = exam raté. Recopie-les à l'identique :

| Quand | Chaîne | Sortie |
|---|---|---|
| Mauvais nb d'args | `Wrong number of arguments\n` (26 car.) | stderr, `exit(1)` |
| Erreur syscall / alloc | `Fatal error\n` (12 car.) | stderr, `exit(1)` |
| Connexion | `server: client %d just arrived\n` | tous sauf le nouveau |
| Déconnexion | `server: client %d just left\n` | tous sauf celui qui part |
| Préfixe de ligne | `client %d: ` (espace final, **pas** de `\n`) | tous sauf l'émetteur |

---

## 9. Pièges fréquents (= points perdus)

1. **Chaînes inexactes** (espaces, `\n`, `:`) → la table ci-dessus.
2. **`reads = writes = active;` à CHAQUE tour** : `select()` écrase les sets.
3. **`select(maxfd + 1, ...)`** : c'est `+ 1`, pas `maxfd`.
4. **`buf[n] = 0`** après `recv` avant de manipuler la chaîne.
5. **Préfixe devant chaque ligne**, via la boucle `while (extract_message == 1)`.
6. **`except`** correct dans `send_all` (nouveau = `client`, message/départ = `fd`).
7. **Fuites** : `free(msg)` après chaque extraction, `free(bufs[fd])` à la déconnexion, `close(fd)`.
8. **127.0.0.1 uniquement** : `htonl(INADDR_LOOPBACK)`, pas `INADDR_ANY`.
9. **Aucun `#define`** dans le rendu.
10. **`recv` retourne `<= 0`** = déconnexion (0 = propre, <0 = erreur).
11. Ne **jamais** réécrire `extract_message`/`str_join` de tête : copie-les du `main.c`.
12. **`MSG_NOSIGNAL`** sur les `send` pour ne pas mourir d'un `SIGPIPE`.

---

## 10. Plan de mémorisation (ordre d'écriture le jour J)

```
1. Includes (6)                         BLOC 1
2. Copier extract_message + str_join    BLOC 2  (depuis main.c)
3. Globals (ids, bufs, next_id,         BLOC 3
   reads/writes/active, maxfd, tmp)
4. fatal() + send_all()                 BLOC 4
5. main :                               BLOC 5
   a) check ac != 2
   b) socket + bind + listen
   c) FD_ZERO/FD_SET/maxfd
   d) while(1) : select
        - fd == sockfd : accept
        - sinon recv :
            n<=0 : left + close
            n>0  : str_join + while(extract) : préfixe + msg
```

Astuce : retiens la **structure** (ces 5 blocs), pas le code mot à mot. Le code découle de la logique une fois la structure en tête.

---

## 11. Code complet de référence

Le fichier `mini_serv.c` de ce dossier est la version finale, compilable telle quelle (avec les 2 fonctions fournies déjà incluses pour pouvoir l'étudier/compiler). Le jour J, tu obtiens ces 2 fonctions depuis le `main.c` fourni — voir `subjects/mini_serv/main.c`.

```bash
cc -Wall -Wextra -Werror mini_serv.c -o mini_serv && ./mini_serv 8080
```
