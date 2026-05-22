# exam05

Solutions des deux exercices de l'exam rank 05 de 42 : **bsq** et **life**.

```
.
├── bsq/        # Biggest Square : recherche du plus grand carre vide sur une carte
└── life/       # Game of Life : simulateur du jeu de la vie de Conway
```

---

## 1. bsq — Biggest Square

### Sujet (resume)
Trouver le plus grand carre possible sur une carte en evitant les obstacles, et le tracer avec un caractere "plein". La premiere ligne du fichier decrit la carte :

```
<n_lignes> <empty> <obstacle> <full>
```

Le programme accepte un ou plusieurs fichiers en argument ; sans argument il lit sur `stdin`. En cas de carte invalide, il affiche `map error` sur `stderr` et passe a la suivante. Les solutions sont separees par un saut de ligne.

Tie-break : si plusieurs carres de meme taille existent, on choisit le plus haut puis le plus a gauche.

### Compilation et utilisation
```sh
cc -Wall -Wextra -Werror bsq/bsq.c -o bsq/bsq
./bsq/bsq bsq/map.txt
./bsq/bsq map1.txt map2.txt map3.txt
cat bsq/map.txt | ./bsq/bsq
```

### Exemple
Avec `bsq/map.txt` :
```
9.oB
...........................
....o......................
............o..............
...........................
....o......................
...............o...........
...........................
......o..............o.....
..o.......o................
```
Sortie :
```
.....BBBBBBB...............
....oBBBBBBB...............
.....BBBBBBBo..............
.....BBBBBBB...............
....oBBBBBBB...............
.....BBBBBBB...o...........
.....BBBBBBB...............
......o..............o.....
..o.......o................
```

### Architecture du code (`bsq/bsq.c`, `bsq/bsq.h`)

Trois structures :
- `t_elements` : entete de la carte (`n_lines`, `empty`, `obstacle`, `full`).
- `t_map`      : la grille (`char** grid`, `width`, `height`), terminee par `NULL`.
- `t_square`   : resultat (`size`, coordonnees `i`, `j` du coin haut-gauche).

Flux principal — `main` -> `convert_file_pointer` -> `execute_bsq` :

1. **`loadElements`** lit la premiere ligne via `fscanf("%d%c%c%c", ...)`. Verifie :
   - exactement 4 valeurs lues,
   - `n_lines > 0`,
   - les 3 caracteres sont distincts deux a deux,
   - les 3 caracteres sont imprimables (32-126).
2. **`loadMap`** lit `n_lines` lignes via `getline`, retire le `\n` final, copie via `ft_substr`, verifie que toutes les lignes ont la meme largeur, puis appelle `element_control` pour s'assurer qu'on n'a que `empty` ou `obstacle` dans la grille.
3. **`find_big_square`** applique la **programmation dynamique** classique du plus grand carre. Pour chaque case non-obstacle :
   ```
   dp[i][j] = 1 + min(dp[i-1][j], dp[i-1][j-1], dp[i][j-1])
   ```
   Les bords (`i == 0` ou `j == 0`) valent 1. On garde le maximum ; un `>` strict (pas `>=`) preserve naturellement la priorite haut-gauche.
4. **`print_filled_square`** ecrit `full` dans la zone calculee, puis affiche la map via `fputs` + `\n`.
5. **`free_map`** libere chaque ligne et le tableau ; `execute_bsq` retourne -1 en cas de carte invalide, ce qui declenche le `map error` dans `main`.

### Fonctions autorisees utilisees
`malloc`, `free`, `fopen`, `fclose`, `getline`, `fscanf`, `fputs`, `fputc`, `fprintf`, `stderr`, `stdout`, `stdin`.

---

## 2. life — Game of Life

### Sujet (resume)
```
./life largeur hauteur iterations
```
La configuration initiale est dessinee par une suite de commandes lues sur `stdin` :

| Commande | Effet |
|----------|-------|
| `w` `a` `s` `d` | deplace le stylo (haut, gauche, bas, droite) |
| `x` | leve / abaisse le stylo (toggle dessin) |
| autre | ignore |

Le stylo demarre en `(0, 0)`. Une cellule peut etre redessinee plusieurs fois sans probleme. Toute cellule en dehors du plateau est consideree comme morte.

Apres lecture, on applique `iterations` etapes du jeu de la vie de Conway, puis on affiche le plateau (`O` = vivante, espace = morte), une ligne par `\n`.

### Compilation et utilisation
```sh
cc -Wall -Wextra -Werror life/life.c -o life/life
echo 'sdxddssaaww' | ./life/life 5 5 0
echo 'dxss'        | ./life/life 3 3 2
```

### Exemple
```sh
$> echo 'sdxddssaaww' | ./life/life 5 5 0 | cat -e
 $
 OOO $
 O O $
 OOO $
 $
```

### Architecture du code (`life/life.c`, `life/life.h`)

Une structure unique `t_g` contient tout l'etat :
- `w`, `h`, `it` : largeur, hauteur, nombre d'iterations.
- `i`, `j`       : position du stylo.
- `draw`         : etat du stylo (0 = leve, 1 = baisse).
- `alive`, `dead` : caracteres affiches (`'O'` et `' '`).
- `b`            : `char**`, plateau `h x w`.

Flux principal — `main` :

1. Verifie `ac == 4`, sinon retourne 1.
2. **`init_g`** parse les 3 arguments via `atoi`, initialise les compteurs et alloue le plateau rempli de `dead`.
3. **`fill_b`** boucle sur `read(STDIN_FILENO, &buffer, 1)`, un caractere a la fois. Pour chaque commande :
   - `w/a/s/d` : deplace le stylo en restant dans les bornes,
   - `x`       : toggle `draw`,
   - sinon     : ignore via un `flag = 1`.
   Si `draw` est actif apres un mouvement valide, on ecrit `alive` a la position courante. Le sujet precise que le stylo doit se deplacer meme quand il est leve : c'est ce que fait le code (le toggle est independant du dessin).
4. **`play`** applique une iteration de Conway sur un buffer temporaire `temp` :
   - vivante : reste vivante si 2 ou 3 voisins vivants, sinon meurt,
   - morte   : naissance si exactement 3 voisins vivants.
   - `count_n` parcourt les 8 voisins (`-1..1` x `-1..1`, en sautant uniquement le centre via `di == 0 && dj == 0`).
   On libere l'ancien plateau et `g->b = temp`.
5. **`print_b`** affiche le plateau, suivi de `'\n'` par ligne.
6. **`free_b`** libere ligne par ligne puis le tableau.

### Fonctions autorisees utilisees
`atoi`, `read`, `putchar`, `malloc`, `free`.

---

## Conventions

- C99, compile avec `-Wall -Wextra -Werror`.
- Pas de dependance externe.
- Les `.h` sont volontairement minimaux (structures + includes systeme).
