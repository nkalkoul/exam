#include "life.h"

void free_b(t_g *g){
    if (g->b){
        for (int i = 0; i < g->h; i++){
            if (g->b[i])
                free(g->b[i]);
        }
        free(g->b);
    }
}

void print_b(t_g *g){
    for (int i = 0; i < g->h; i++){
        for (int j = 0; j < g->w; j++){
            putchar(g->b[i][j]);
        }
        putchar('\n');
    }
}

int init_g(t_g *g, char **av){
    g->w = atoi(av[1]);
    g->h = atoi(av[2]);
    g->it = atoi(av[3]);
    g->i = 0;
    g->j = 0;
    g->draw = 0;
    g->dead = ' ';
    g->alive = 'O';
    g->b = malloc(sizeof(char *) * g->h);
    if (!g->b)
        return (-1);
    for (int i = 0; i < g->h; i++){
        g->b[i] = malloc(sizeof(char) * g->w);
        if (!g->b[i]){
            free_b(g);
            return(-1);
        }
        for (int j = 0; j < g->w; j++){
            g->b[i][j] = g->dead;
        }
    }
    return (0);
}

void fill_b(t_g *g){
    char buffer;
    int flag;

    while (read(STDIN_FILENO, &buffer, 1) == 1){
        flag = 0;
        switch (buffer){
            case 'w' : 
                if (g->i > 0)
                    g->i--;
                break;
            case 's' : 
                if (g->i < (g->h - 1))
                    g->i++;
                break;
            case 'a' : 
                if (g->j > 0)
                    g->j--;
                break;
            case 'd' : 
                if (g->j < (g->w - 1))
                    g->j++;
                break;
            case 'x' :
                g->draw = !(g->draw);
                break;
            default :
                flag = 1;
        }
        if (flag == 0 && g->draw == 1){
            if (g->i >= 0 && g->i < g->h && g->j >= 0 && g->j < g->w)
                g->b[g->i][g->j] = g->alive;
        }
    }
}

int count_n(t_g *g, int i, int j){
    int count = 0;
    for (int di = -1; di < 2; di++){
        for (int dj = -1; dj < 2; dj++){
            if (di == 0 && dj == 0)
                continue;
            int ni = i + di;
            int nj = j + dj;
            if (ni >= 0 && ni < g->h && nj >= 0 && nj < g->w)
                if (g->b[ni][nj] == g->alive)
                    count++;
        }
    }
    return (count);
}

int play(t_g *g){
    char **temp;

    temp = malloc(sizeof(char * ) * g->h);
    if (!temp)
        return (-1);
    for (int i = 0; i < g->h; i++){
        temp[i] = malloc(sizeof(char ) * g->w);
        if (!temp[i])
            return (-1);
        for (int j = 0; j < g->w; j++){
            int count = count_n(g, i, j);
            if (g->b[i][j] == g->alive){
                if (count == 2 || count == 3)
                    temp[i][j] = g->alive;
                else
                    temp[i][j] = g->dead;
            }
            else
                if (count == 3)
                    temp[i][j] = g->alive;
                else
                    temp [i][j] = g->dead;
        }
    }
    free_b(g);
    g->b = temp;
    return (0);
}

int main(int ac, char **av){
    if (ac != 4)
        return (1);

    t_g g;

    if (init_g(&g, av) == -1)
        return (1);
    fill_b(&g);
    for (int i = 0; i < g.it; i++){
        if (play(&g) == -1){
            free_b(&g);
            return (1);
        }
    }
    print_b(&g);
    free_b(&g);
    return (0);
}