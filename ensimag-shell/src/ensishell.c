/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <wordexp.h>

#include "variante.h"
#include "readcmd.h"

#ifndef VARIANTE
#error "Variante non défini !!"
#endif

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

#if USE_GUILE == 1
#include <libguile.h>


typedef struct bg_children{
    pid_t pid;
    char name[10];
    struct bg_children * next;

} bg_children_t;

bg_children_t *liste_children_bg;


void add_bg(bg_children_t ** head, pid_t pid_a_ajouter, char * name_a_ajouter){
    bg_children_t * new_bg_children;
    new_bg_children=malloc(sizeof(bg_children_t));

    new_bg_children->pid=pid_a_ajouter;
    strcpy(new_bg_children->name, name_a_ajouter);
    new_bg_children->next=*head;
    *head=new_bg_children;
}

void print_bg(bg_children_t *head){
    bg_children_t * tmp=head;
    while(tmp!=NULL){
        printf("%s\n", tmp->name);
        //printf("%d\n", tmp->pid);
        tmp=tmp->next;
    }
}

void remove_bg(bg_children_t **head, pid_t pid){
    bg_children_t *tmp=*head;
    bg_children_t *tmp2=*head;
    if (tmp->pid==pid)
        *head=tmp->next;
    else {
        tmp2=tmp->next;
        while (tmp2!=NULL && tmp2->pid!=pid){
            tmp=tmp2;
            tmp2=tmp2->next;
        }
    }
    if (tmp2!=NULL){
        tmp->next=tmp2->next;
        free(tmp2);
    }
}

void update_bg(bg_children_t **head){
    bg_children_t * tmp=*head;

    while(tmp!=NULL){
        int status;
        pid_t child = waitpid(tmp->pid, &status, WNOHANG);
        if(child == 0){
            //n ne fait rien l'enfant est encore en vie \n
        }else{
            remove_bg(&liste_children_bg, tmp->pid);
        }
        tmp=tmp->next;

    }
}

//return 1 if the last non-space character of the string is equal to car
int last_char(char * str, char car)
{
    int count = 0;
    while(str[count] != '\0' && str[count] != car)
        count++;
    if(str[count] == '\0')
        return 0;
    //we're not at the end of the string and recognized the caracter
    count++;
    while(str[count] != '\0')
    {
        if(str[count] != ' ')
            return 0;
    }
    //we're at the end of the string
    return 1;
}

char * replace_char(char * str, char * delim)
{
    //on trouve la longueur du nouveau string
    int new_length = 0;
    int str_length = 0;
    int bg_check = 0;
    for(int i=0; str[i] != '\0'; i++)
    {
        if(str[i] == delim[0])
            new_length += 2; //on va ajouter deux " si on tombe sur le caractère
        new_length++;
        str_length++;
    }
    new_length++; //pour le caractère de fin '\0'
    //cas spécial pour delim[0] == '&', car à la fin de la commande
    if(delim[0] == '&' && last_char(str, '&') == 1)
    {
        new_length += 2;
        bg_check = 1;
    }
    char * new_str = malloc(sizeof(char)*new_length);
    //initialisation du nouveau string
    for(int i=0; i<new_length; i++)
        new_str[i] = '\0';
    char * token = strtok(str, delim);
    while(token)
    {
        strcat(new_str, token);
        //puts(token);
        token = strtok(NULL, delim);
        if(token != NULL)
        {
            strcat(new_str, "\"");
            strcat(new_str, delim);
            strcat(new_str, "\"");
        }
    }
    if(bg_check == 1)
    {
        strcat(new_str, "\"&\"");
    }
    free(str);
    return new_str;
}

char * word_expansion(char * line)
{
    //on traite les caractères spéciaux
    char * new_line = replace_char(line, "|");
    char * new_line_2 = replace_char(new_line, "<");
    char * new_line_3 = replace_char(new_line_2, ">");
    char * new_line_4 = replace_char(new_line_3, "&");
    wordexp_t result;
    wordexp(new_line_4, &result, 0);
    free(new_line_4);
    int cmd_offset = 0;
    //on calcule la longueur de la nouvelle commande
    for(int i=0; i<result.we_wordc; i++)
    {
        cmd_offset += strlen(result.we_wordv[i]);
        cmd_offset++; //derrière chaque commande, il y a un espace
    }
    //cmd_offset--;

    char * max_cmd = malloc(sizeof(char)*cmd_offset);
    //initialisation à '\0'
    for(int i=0; i<cmd_offset; i++)
        max_cmd[i] = '\0';
    int tmp_cmd_offset = 0;
    //on fait la recopie
    for(int i=0; tmp_cmd_offset<cmd_offset; i++)
    {
        strcat(max_cmd, result.we_wordv[i]);
        tmp_cmd_offset += (int)strlen(result.we_wordv[i]);
        max_cmd[tmp_cmd_offset++] = ' ';
    }
    max_cmd[cmd_offset] = '\0';
    wordfree(&result);
    return max_cmd;
}

int is_in_the_middle(struct cmdline * l, int i)
{
    return (i > 0 && l->seq[i+1] != NULL);
}

void pipe_multiple(struct cmdline * l)
{
    int return_status;
    pid_t child;
    int tube1[2];
    int tube2[2];
    int child_number=0;
    //pour pouvoir attendre tous les fils à la fin du pipe multiple, on 
    //sauvegarde les pid pour les attendre à la fin
    int nb_child = 0;
    while(l->seq[nb_child] != NULL)
        nb_child++;
    pid_t pid_child[nb_child];

    while(l->seq[child_number] != NULL)
    {
        if(child_number%2 == 0)
            pipe(tube1);
        else
            pipe(tube2);
        if((child=fork()) == 0) //il s'agit d'un fils
        {
            if(child_number == 0) //on pourrait avoir une entrée à partir d'un fichier
            {
                if(l->in != NULL)
                {
                    int fd = open(l->in, O_RDONLY);
                    dup2(fd, 0);
                    close(fd);
                }
                dup2(tube1[1], 1);
                close(tube1[0]);
                close(tube1[1]);
            } else if(is_in_the_middle(l, child_number)) {
                //au milieu
                if(child_number%2 == 0)
                {
                    dup2(tube2[0], 0);
                    dup2(tube1[1], 1);
                } else {
                    dup2(tube1[0], 0);
                    dup2(tube2[1], 1);
                }
                close(tube1[0]);
                close(tube1[1]);
                close(tube2[0]);
                close(tube2[1]);
            } else { //il s'agit de la fin des commandes
                if(l->out != NULL)
                {
                    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
                    int fd = open(l->out, O_RDWR|O_APPEND|O_CREAT, mode);
                    dup2(fd, 1);
                    close(fd);
                }
                if(child_number%2 == 0)
                {
                    dup2(tube2[0], 0);
                    close(tube2[0]);
                    close(tube2[1]);
                }
                else
                {
                    dup2(tube1[0], 0);
                    close(tube1[0]);
                    close(tube1[1]);
                }
            }
            execvp(l->seq[child_number][0], l->seq[child_number]);
        }
        if(child_number == 0 || is_in_the_middle(l, child_number))
        {
            if(child_number%2 == 0)
            {
                close(tube1[1]);
                if(child_number > 0)
                    close(tube2[0]);
            } else {
                close(tube2[1]);
                if(child_number > 0)
                    close(tube1[0]);
            }
        }
        pid_child[child_number] = child;
        child_number++;
    }
    if(l->bg)
    {
        for(int i=0; i<nb_child; i++)
            add_bg(&liste_children_bg, pid_child[i], l->seq[i][0]);
    } else {
        for(int i=0; i<nb_child; i++)
            waitpid(pid_child[i], &return_status, 0);
    }
}

int executer(char *line)
{
    /* Insert your code to execute the command line
     * identically to the standard execution scheme:
     * parsecmd, then fork+execvp, for a single command.
     * pipe and i/o redirection are not required.
     */
    char * new_line = word_expansion(line);

    struct cmdline *l = parsecmd( &new_line);
    pid_t child;
    if(l->seq[0] != NULL && l->seq[1] == NULL)
    {
        child = fork();
        if(child == 0) //c'est le fils
        {
            if(l->in != NULL)
            {
                int fd = open(l->in, O_RDONLY);
                dup2(fd, 0);
                close(fd);
            }
            if(l->out != NULL)
            {
                mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
                int fd = open(l->out, O_RDWR|O_APPEND|O_CREAT, mode);
                dup2(fd, 1);
                close(fd);
            }
            execvp(l->seq[0][0], l->seq[0]);
        } 
    } else if(l->seq[0] != NULL && l->seq[1] != NULL) {
        pipe_multiple(l);
    }
    //il s'agit du père
    int return_status;
    if(l->bg){
        add_bg(&liste_children_bg, child, l->seq[0][0]);
    } else {
        waitpid(child, &return_status, 0);
    }
    return 0;
}

SCM executer_wrapper(SCM x)
{
    return scm_from_int(executer(scm_to_locale_stringn(x, 0)));
}
#endif


void terminate(char *line) {
#ifdef USE_GNU_READLINE
    /* rl_clear_history() does not exist yet in centOS 6 */
    clear_history();
#endif
    if (line)
        free(line);
    printf("exit\n");
    exit(0);
}


int main() {
    printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#ifdef USE_GUILE
    scm_init_guile();
    /* register "executer" function in scheme */
    scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif

    while (1) {
        //struct cmdline *l;
        char *line=0;
        //int i, j;
        char *prompt = "ensishell>";

        /* Readline use some internal memory structure that
           can not be cleaned at the end of the program. Thus
           one memory leak per command seems unavoidable yet */
        line = readline(prompt);
        if (line == 0 || ! strncmp(line,"exit", 4)) {
            terminate(line);
        }

        if(!strncmp(line,"jobs",4)) {
            print_bg(liste_children_bg);
        }

#ifdef USE_GNU_READLINE
        add_history(line);
#endif


#ifdef USE_GUILE
        /* The line is a scheme command */
        if (line[0] == '(') {
            char catchligne[strlen(line) + 256];
            sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
            scm_eval_string(scm_from_locale_string(catchligne));
            free(line);
            continue;
        }
#endif
        update_bg(&liste_children_bg);
        executer(line);

        /* parsecmd free line and set it up to 0 */
        //l = parsecmd( & line);

        ///* If input stream closed, normal termination */
        //if (!l) {

        //    terminate(0);
        //}



        //if (l->err) {
        //    /* Syntax error, read another command */
        //    printf("error: %s\n", l->err);
        //    continue;
        //}

        //if (l->in) printf("in: %s\n", l->in);
        //if (l->out) printf("out: %s\n", l->out);
        //if (l->bg) printf("background (&)\n");

        ///* Display each command of the pipe */
        //for (i=0; l->seq[i]!=0; i++) {
        //    char **cmd = l->seq[i];
        //    printf("seq[%d]: ", i);
        //    for (j=0; cmd[j]!=0; j++) {
        //        printf("'%s' ", cmd[j]);
        //    }
        //    printf("\n");
        //}
    }

}
