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
    struct bg_children * next;

} bg_children_t;

bg_children_t *liste_children_bg;


void add_bg(bg_children_t ** head, pid_t pid_a_ajouter){
    bg_children_t * new_bg_children;
    new_bg_children=malloc(sizeof(bg_children_t));

    new_bg_children->pid=pid_a_ajouter;
    new_bg_children->next=*head;
    *head=new_bg_children;

}

void print_bg(bg_children_t *head){
    bg_children_t * tmp=head;
    while(tmp!=NULL){
        printf("%d\n", tmp->pid);
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


int executer(char *line)
{
    /* Insert your code to execute the command line
     * identically to the standard execution scheme:
     * parsecmd, then fork+execvp, for a single command.
     * pipe and i/o redirection are not required.
     */
    struct cmdline *l = parsecmd(& line);
    pid_t child1, child2;
    if(l->seq[0] != NULL && l->seq[1] == NULL)
    {
        child1 = fork();
        if(child1 == 0) //c'est le fils
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
        
    } else if(l->seq[0] != NULL && l->seq[1] != NULL)
    {
        int tube[2];
        pipe(tube);
        if((child1=fork()) == 0)
        {
            //if(l->out != NULL)
            //{
            //    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
            //    int fd = open(l->out, O_RDWR|O_APPEND|O_CREAT, mode);
            //    dup2(fd, 1);
            //    close(fd);
            //}
            dup2(tube[0], 0);
            close(tube[1]);
            close(tube[0]);
            execvp(l->seq[1][0], l->seq[1]);
        }
        if((child2=fork()) == 0)
        {
            //if(l->in != NULL)
            //{
            //    int fd = open(l->in, O_RDONLY);
            //    dup2(fd, 0);
            //    close(fd);
            //}
            dup2(tube[1], 1);
            close(tube[1]);
            close(tube[0]);
            execvp(l->seq[0][0], l->seq[0]);
        }
    }
    //il s'agit du père
    int return_status;
    if(l->bg){
        add_bg(&liste_children_bg,child1);
        add_bg(&liste_children_bg,child2);
    }else{
        if(l->seq[1] == NULL)
            waitpid(child1, &return_status, 0);
        else
            waitpid(child1, &return_status, 0);
    }
    /* Remove this line when using parsecmd as it will free it */
    free(line);

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
