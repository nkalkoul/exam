# exam06 — mini_serv

Cours complet pour comprendre, retenir et **refaire en conditions d'examen** l'exercice `mini_serv` de l'exam rank 06 de 42.

```
.
├── mini_serv.c              # la solution
└── subjects/mini_serv/
    ├── subject.fr.txt       # sujet (FR)
    └── main.c               # squelette fourni par 42 (fonctions a copier)
```

> ⚠️ `mini_serv` est l'exercice **le plus dur** de l'exam06 (les autres niveaux sont plus simples : `ft_popen`, `picoshell`, `sandbox`...). Si tu sais le refaire de tete, tu valides l'exam.

---

## 1. Le sujet en une phrase

Écrire un **serveur de chat TCP** mono-processus, non-bloquant, sur `127.0.0.1`, qui :
- attribue un **id** à chaque client (0, 1, 2, ...),
- annonce arrivées et départs à tous,
- **relaie** chaque ligne reçue à tous les autres, préfixée de `client %d: `.

Le tout **sans fuite** mémoire/fd, **sans bloquer** sur un client lent, et avec un unique `select()`.

### Les chaînes exactes (à ne JAMAIS traduire ni modifier)
```c
"Wrong number of arguments\n"       // stderr, exit(1), si argc != 2
"Fatal error\n"                     // stderr, exit(1), erreur syscall / malloc
"server: client %d just arrived\n"  // a tous, a la connexion
"client %d: "                       // prefixe AVANT CHAQUE ligne relayee
"server: client %d just left\n"     // a tous, a la deconnexion
```

---

## 2. Les concepts réseau (le minimum vital)

### 2.1 Le cycle d'un socket serveur TCP
```
socket()  ->  bind()  ->  listen()  ->  accept()  ->  recv()/send()  ->  close()
  |            |            |             |
  cree le fd   lie a une    passe en      accepte UNE connexion,
               adresse:port mode passif   renvoie un NOUVEAU fd client
```

- **`socket(AF_INET, SOCK_STREAM, 0)`** → un fd pour de l'IPv4 + TCP.
- **`bind(s, &addr, len)`** → attache le socket à `127.0.0.1:port`.
- **`listen(s, backlog)`** → le socket devient « à l'écoute ».
- **`accept(s, 0, 0)`** → quand un client se connecte, renvoie un **nouveau fd** dédié à ce client. Le fd d'écoute `s`, lui, reste ouvert pour les prochains.

### 2.2 L'adresse : `struct sockaddr_in`
```c
struct sockaddr_in a;
bzero(&a, sizeof(a));
a.sin_family = AF_INET;
a.sin_addr.s_addr = htonl(2130706433); // 2130706433 == 0x7F000001 == 127.0.0.1
a.sin_port = htons(atoi(av[1]));        // port en "network byte order"
```
- `htonl` / `htons` = **H**ost **TO** **N**etwork (**L**ong / **S**hort) : convertit l'ordre des octets de la machine vers l'ordre réseau (big-endian).
- `2130706433` en décimal **est** `127.0.0.1`. C'est l'astuce pour ne pas utiliser `inet_addr` (non autorisé). À retenir par cœur.

### 2.3 Pourquoi `select()` ?
Un seul thread doit gérer **N clients à la fois** + le socket d'écoute. Sans `select`, un `recv` bloquerait sur un client silencieux pendant qu'un autre attend. `select()` répond à la question : **« parmi ces fds, lesquels sont prêts MAINTENANT ? »**

```c
int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout);
```
- `nfds` = **plus grand fd + 1** (d'où la variable `maxfd`).
- `readfds` : « qui a des données à lire / une connexion à accepter ? »
- `writefds` : « à qui puis-je écrire sans bloquer ? » (clé pour le client paresseux).
- **`select` MODIFIE les sets en place** : après l'appel, ils ne contiennent plus que les fds *prêts*. → il faut les **reconstruire à chaque tour** depuis un set maître (`active`).

### 2.4 Les 4 macros `fd_set`
| Macro | Rôle |
|---|---|
| `FD_ZERO(&set)` | vide le set |
| `FD_SET(fd, &set)` | ajoute `fd` |
| `FD_CLR(fd, &set)` | retire `fd` |
| `FD_ISSET(fd, &set)` | teste si `fd` est dedans (≠ 0 si oui) |

---

## 3. Architecture de la solution

Trois zones : les **globales**, les **deux fonctions copiées** du `main.c` fourni, et le **`main`** (boucle `select`).

### 3.1 Globales et helpers
```c
typedef struct s_client { int id; char *buf; } t_client;

t_client clients[1024];        // index = fd du client ; buf = accumulateur de lignes
fd_set   active, rset, wset;   // active = set maitre ; rset/wset = copies pour select
int      maxfd, nextid;        // plus grand fd suivi ; prochain id a distribuer

void fatal() { write(2, "Fatal error\n", 12); exit(1); }
```
**Idée maîtresse** : on indexe le tableau `clients` **par le fd**. Le fd 7 ? → `clients[7]`. Simple et rapide, pas de recherche.

```c
void send_all(int ex, char *s) {
    for (int i = 0; i <= maxfd; i++)
        if (FD_ISSET(i, &wset) && i != ex)
            send(i, s, strlen(s), 0);
}
```
Envoie `s` à **tous les fds prêts en écriture** (`wset`) **sauf** `ex` (l'expéditeur). C'est ici que le client paresseux est protégé : s'il n'est pas dans `wset`, on ne lui envoie pas → **aucun blocage, aucune déconnexion**.

### 3.2 Les deux fonctions fournies (à COPIER, pas à inventer)

`extract_message` et `str_join` sont **données dans `main.c`**. En examen, tu les copies-colles. Mais il faut comprendre ce qu'elles font.

**`str_join(buf, add)`** — concatène `add` à la fin de `buf`, libère l'ancien `buf`, renvoie le nouveau (gère `buf == NULL`). → sert à **accumuler** les morceaux reçus par `recv` dans `clients[i].buf`.

**`extract_message(&buf, &msg)`** — extrait **UNE** ligne complète (jusqu'au premier `\n` inclus) :
| Retour | Signification |
|---|---|
| `1` | une ligne extraite → `*msg` = la ligne, `*buf` = le reste |
| `0` | pas de `\n` → message incomplet, on garde tout dans `buf` |
| `-1` | échec `calloc` |

C'est ce duo qui résout les deux pièges classiques :
- **message découpé** sur plusieurs `recv` → on accumule jusqu'au `\n`.
- **plusieurs lignes en un `recv`** → la boucle `while (extract_message(...) > 0)` les sort une par une.

### 3.3 Le `main` — la boucle d'événements
```c
while (1) {
    rset = wset = active;                              // 1. reconstruire les sets
    if (select(maxfd + 1, &rset, &wset, 0, 0) < 0) continue;
    for (i = 0; i <= maxfd; i++) {
        if (!FD_ISSET(i, &rset)) continue;             // 2. fd pas pret en lecture -> suivant
        if (i == s) {                                  // 3. socket d'ecoute -> nouvelle connexion
            if ((c = accept(s, 0, 0)) < 0) continue;
            if (c > maxfd) maxfd = c;
            clients[c].id = nextid++; clients[c].buf = 0; FD_SET(c, &active);
            sprintf(m, "server: client %d just arrived\n", clients[c].id);
            send_all(c, m); break;
        }
        if ((r = recv(i, b, 119999, 0)) <= 0) {        // 4. client deconnecte (recv <= 0)
            sprintf(m, "server: client %d just left\n", clients[i].id);
            send_all(i, m); free(clients[i].buf);
            FD_CLR(i, &active); close(i); break;
        }
        b[r] = 0;                                      // 5. donnees recues
        if (!(clients[i].buf = str_join(clients[i].buf, b))) fatal();
        while ((e = extract_message(&clients[i].buf, &l)) > 0) {
            sprintf(m, "client %d: ", clients[i].id);  // 6. relayer ligne par ligne
            send_all(i, m); send_all(i, l); free(l);
        }
        if (e == -1) fatal();
    }
}
```

**Pourquoi les `break` ?** Après un `accept` (on a modifié `active`) ou un `close` (on a fermé un fd), les sets `rset`/`maxfd` ne reflètent plus la réalité. On **casse la boucle `for`** pour repartir d'un `select` propre. C'est plus sûr que de continuer à itérer sur des fds devenus invalides.

**Pourquoi `recv` <= 0 = déconnexion ?** `recv` renvoie `0` quand le client a fermé proprement (EOF), `< 0` sur erreur. Dans les deux cas → on annonce le départ, on libère, on ferme.

**L'ordre du relais** : `send_all(i, m)` envoie le préfixe `client %d: `, puis `send_all(i, l)` envoie la ligne (qui contient déjà son `\n`). Comme `extract_message` coupe juste après le `\n`, **chaque ligne** repasse dans la boucle → chaque ligne reçoit son préfixe. ✅

---

## 4. Conformité au sujet (checklist de correction)

| Exigence | Où dans le code |
|---|---|
| `argc != 2` → `Wrong number of arguments\n`, exit 1 | `if (ac != 2) {...}` |
| Erreur syscall avant `accept` → `Fatal error\n`, exit 1 | `fatal()` sur `socket`/`bind`/`listen` |
| Échec `malloc` → `Fatal error\n`, exit 1 | `fatal()` sur `str_join`/`extract_message` |
| Écoute **uniquement** 127.0.0.1 | `htonl(2130706433)` |
| Pas de `#define` | aucun `#define` |
| Non-bloquant, ne déconnecte pas les lents | `select` + filtre `wset` dans `send_all` |
| id 0 puis +1 | `clients[c].id = nextid++` |
| `server: client %d just arrived\n` à tous | après `accept` |
| relais avec `client %d: ` devant chaque ligne | boucle `extract_message` |
| `server: client %d just left\n` à tous | branche `recv <= 0` |
| Aucune fuite mémoire / fd | `free(clients[i].buf)`, `free(l)`, `close(i)` |

### Vérifié sur cette machine
- **Compilation** `cc -Wall -Wextra -Werror` : propre.
- **Fonctionnel** : args invalides, arrivée/départ, message simple, **message multi-lignes** (préfixe sur chaque ligne), **message partiel** (`par` + `tial\n` → une seule ligne `client 1: partial`), **3 clients** (ids 0/1/2 corrects).
- **Valgrind** `--leak-check=full --track-fds=yes` : `6 allocs, 6 frees`, `0 bytes in use at exit`, `0 errors`. (Les 5 fd restants = stdin/out/err + log valgrind + socket d'écoute, normal puisqu'on tue le serveur en vol.)

---

## 5. Les pièges qui font échouer (ce que le correcteur teste)

1. **Oublier de reconstruire `rset`/`wset`** : `select` les écrase. Sans `rset = wset = active;` en tête de boucle, ça marche un tour puis casse.
2. **`maxfd` pas mis à jour** : `select(maxfd+1, ...)` ignorerait les fds au-dessus → nouveaux clients invisibles.
3. **Pas de bufferisation** : envoyer dès le `recv` casse le test « message découpé en deux paquets ».
4. **Préfixe une seule fois** au lieu de **chaque ligne** d'un message multi-ligne.
5. **Renvoyer au sender** : `send_all` doit exclure `ex`.
6. **Fuites** : ne pas `free(clients[i].buf)` à la déconnexion, ou ne pas `free(l)` après chaque ligne.
7. **fd leak** : oublier `close(i)` (et/ou `FD_CLR`).
8. **`#define`** présent → interdit par le sujet.
9. **Mauvais message d'erreur** : oublier le `\n`, mauvais `exit code`, ou écrire sur `stdout` au lieu de `stderr`.
10. **`0.0.0.0`** au lieu de `127.0.0.1` (`htonl(INADDR_ANY)` interdit ici).
11. **Bloquer sur un client lent** : envoyer sans filtrer par `wset`.

---

## 6. Méthode pour l'apprendre (vraiment le retenir)

Le but n'est pas de mémoriser 117 lignes par cœur, mais de **reconstruire** le programme à partir de quelques ancres.

### Les 6 ancres à mémoriser
1. **Le tableau indexé par fd** : `t_client clients[1024]` avec `{ id, buf }`.
2. **`127.0.0.1 = htonl(2130706433)`** (le nombre magique).
3. **`rset = wset = active;` en tête de boucle** (sinon select casse tout).
4. **`send_all(ex, s)`** : boucle `FD_ISSET(i, &wset) && i != ex`.
5. **Le squelette du `for`** : `if (i == s) accept ; else if (recv<=0) leave ; else relay`.
6. **Le duo `str_join` (accumule) + `extract_message` dans un `while`** (relaie ligne par ligne).

### Plan d'entraînement (5 jours)
- **J1** : comprendre sockets + `select` (cette section 2). Dessine le schéma `socket→bind→listen→accept` sans regarder.
- **J2** : écris `main` qui accepte 1 client et echo ce qu'il tape. Sans relais, sans buffer.
- **J3** : ajoute le multi-client (`select`, `clients[]`, `maxfd`, arrivée/départ).
- **J4** : ajoute la bufferisation (`str_join` + `extract_message`) et le relais préfixé.
- **J5** : refais **tout** de zéro, chronométré, puis valgrind. Recommence jusqu'à < 25 min sans erreur.

### Test de maîtrise
Tu sais le faire quand tu peux **expliquer à voix haute** pourquoi chaque `break` est là, pourquoi on filtre par `wset`, et pourquoi `extract_message` est appelé dans un `while` et pas un `if`.

---

## 7. Reproduire en conditions d'examen

### Contexte réel de l'exam
Dans le rendu, 42 te fournit **`main.c`** contenant déjà `extract_message` et `str_join`. **Tu les copies tels quels.** Tu n'as donc à écrire « que » : les includes, les globales, `fatal`, `send_all`, et le `main`.

### Ordre de frappe recommandé (≈ 20-25 min)
1. **Includes + prototype `sprintf`** (évite d'inclure `<stdio.h>`) :
   ```c
   #include <string.h>
   #include <unistd.h>
   #include <stdlib.h>
   #include <sys/socket.h>
   #include <netinet/in.h>
   int sprintf(char *, const char *, ...);
   ```
2. **Globales + `fatal`** (section 3.1).
3. **Copier `extract_message` et `str_join`** depuis le `main.c` fourni.
4. **`send_all`** (3 lignes).
5. **`main`** : args → socket → `FD_ZERO`/`FD_SET` → adresse → bind → listen.
6. **Boucle `select`** : reconstruire les sets, le `for`, les 3 branches (accept / leave / relay).
7. **Compiler** `cc -Wall -Wextra -Werror mini_serv.c` et corriger les warnings (souvent : variable inutilisée, prototype manquant).

### Auto-test express (sans script)
```sh
# Terminal 1 : lancer
cc -Wall -Wextra -Werror mini_serv.c -o mini_serv && ./mini_serv 8080

# Terminal 2 : client A (reste connecte, ecoute)
nc 127.0.0.1 8080

# Terminal 3 : client B (envoie)
nc 127.0.0.1 8080
# tape une ligne, puis colle un bloc multi-lignes -> verifie le prefixe sur CHAQUE ligne
```
Ce que tu dois voir côté A :
```
server: client 1 just arrived
client 1: <ce que tape B>
server: client 1 just left   # quand tu fais Ctrl-C sur B
```

### Vérif fuites (si valgrind dispo)
```sh
valgrind --leak-check=full --track-fds=yes ./mini_serv 8080
# attendu : "0 bytes in use at exit", "ERROR SUMMARY: 0 errors"
```

---

## 8. Fonctions autorisées utilisées
`write`, `close`, `select`, `socket`, `accept`, `listen`, `send`, `recv`, `bind`, `strstr`, `malloc`, `realloc`, `free`, `calloc`, `bzero`, `atoi`, `sprintf`, `strlen`, `exit`, `strcpy`, `strcat`, `memset`.

> Note : `str_join` utilise `malloc/strcat/strlen/free` ; `extract_message` utilise `calloc/strlen/strcpy`. Tout est dans la liste autorisée.

---

## 9. Conventions
- C, compilé avec `-Wall -Wextra -Werror`, zéro warning.
- Un seul fichier, aucune dépendance externe, **aucun `#define`**.
- Indexation par fd, un seul `select`, bufferisation par ligne.
