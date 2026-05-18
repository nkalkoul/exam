#ifndef LIFE
#define LIFE

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct s_g{

    int w;
    int h;
    int it;
    int i;
    int j;
    int draw;
    char alive;
    char dead;
    char **b;

}t_g;

// int init_g(t_g *g, char **av);
// void fill_b(t_g *g);
// int play(t_g *g);
// void free_b(t_g *g);
// int count_n(t_g *g, int i, int j);
// void print_b(t_g *g);

#endif