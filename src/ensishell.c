/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>




#include "variante.h"
#include "readcmd.h"

#ifndef VARIANTE
#error "Variante non défini !!"
#endif

#define N_PROCESSUS_MAX 255
#define READ_END 0
#define WRITE_END 1

// structure de processus
typedef struct processus_t{
	pid_t pid;
	char commande[50]; // hypothèse que les noms de commande ne dépassent pas 50 caractères
  long int elapsed_time;
} processus;

processus proc_table[N_PROCESSUS_MAX];

void interruption_handler(int sig)
{
  int k;
  pid_t endID;
  struct timeval tp;
  int status;
  endID = waitpid(-1, &status, WNOHANG);
  if (endID == -1 || endID == 0) printf("erreur pid handler\n");
  else{
    for(k = 0; proc_table[k].pid != endID; k++){}
    gettimeofday(&tp, NULL);
    printf("ARRET DE %s qui était à pid %d. Durée d'execution : %ld s\n",
            proc_table[k].commande, proc_table[k].pid, tp.tv_sec - proc_table[k].elapsed_time);
  }
}

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

#if USE_GUILE == 1
#include <libguile.h>

int question6_executer(char *line)
{
	/* Question 6: Insert your code to execute the command line
	 * identically to the standard execution scheme:
	 * parsecmd, then fork+execvp, for a single command.
	 * pipe and i/o redirection are not required.
	 */

	if (! strncmp(line,"mkdir", 5)){
		printf("c'est un mkdir\n" );
	}


	printf("Not implemented yet: can not execute %s\n", line);





	/* Remove this line when using parsecmd as it will free it */
	free(line);

	return 0;
}

SCM executer_wrapper(SCM x)
{
        return scm_from_int(question6_executer(scm_to_locale_stringn(x, 0)));
}
#endif


void terminate(char *line) {
#if USE_GNU_READLINE == 1
	/* rl_clear_history() does not exist yet in centOS 6 */
	clear_history();
#endif
	if (line)
	  free(line);
	printf("exit\n");
	exit(0);
}
// début du terminal
int main() {
	int new_processus = 0;
	struct sigaction traitant;
  struct timeval tp = {0};
        printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#if USE_GUILE == 1
        scm_init_guile();
        /* register "executer" function in scheme */
        scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif
	int run = 1, k;
	while (run) {
		struct cmdline *l;
		char *line=0;
		int i, j;
		char *prompt = "ensishell>";

		/* Readline use some internal memory structure that
		   can not be cleaned at the end of the program. Thus
		   one memory leak per command seems unavoidable yet */
		line = readline(prompt);
		if (line == 0 || ! strncmp(line,"exit", 4)) {
			terminate(line);
		}

#if USE_GNU_READLINE == 1
		add_history(line);
#endif


#if USE_GUILE == 1
		/* The line is a scheme command */
		if (line[0] == '(') {
			char catchligne[strlen(line) + 256];
			sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
			scm_eval_string(scm_from_locale_string(catchligne));
			free(line);
                        continue;
                }
#endif

		/* parsecmd free line and set it up to 0 */
		l = parsecmd( & line);

		/* If input stream closed, normal termination */
		if (!l) {

			terminate(0);
		}



		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		if (l->in) printf("in: %s\n", l->in);
		if (l->out) printf("out: %s\n", l->out);
		if (l->bg) printf("background (&)\n");
/*


UR !
CERTAINS TESTS NE PASSENT PAS, ET POURTANT DANS LE SHELL LA SORTIE EST BONNE !!
"ls -l" et "cat a.txt a.txt" par exemple...


*/
		pid_t pid;
		/* Display each command of the pipe */
		int p[2];
		pipe(p); // un seul pipe !
		for (i=0; l->seq[i]!=0; i++) {
			char **cmd = l->seq[i];
			(void)j;
			printf("seq[%d]: ", i);
                        for (j=0; cmd[j]!=0; j++) {
                                printf("'%s' ", cmd[j]);
                        }
			printf("\n");
			// Commandes internes
			if (!strcmp(cmd[0], "jobs")){ /* version à base de tableau, mais qui est sécurisée */
				printf("jobs:\n");
				for(k = 0; k < new_processus; k++){
					int stat_loc, endID;
					close(p[WRITE_END]);
					endID = waitpid(proc_table[k].pid, &stat_loc, WNOHANG);
					if (endID == 0){
						printf("processus %s pid %d ", proc_table[k].commande, proc_table[k].pid);
						printf("is still running\n");
					}
					/* decommenter si l'on veut afficher l'historique des processus terminés
					else if (endID == pid){ // La première fois on signal l'arrêt
						if (WIFEXITED(stat_loc)) printf("ended normally\n");
						else if (WIFSIGNALED(stat_loc)) printf("ended because of uncaught signal\n");
						else if (WIFSTOPPED(stat_loc)) printf("has stopped\n");
						else printf("status not treated\n"); // ensuite c'est du passé
					}
					else if (endID == -1) printf("n'existe plus\n");*/
				}
			}
			else{
				// On fork pour lancer un processus
				switch( pid = fork() ) {
				case -1:
				  perror("fork error");
				  break;
				case 0: // le fils
					/* les pipes */
					if (l->seq[i + 1]!=0){ // un pipe est après
						printf("%s pipe sur stdout\n", cmd[0]);
						dup2(p[WRITE_END], STDOUT_FILENO);
						close(p[READ_END]);
						close(p[WRITE_END]);
					}
					if (i > 0){ // un pipe est avant
						printf("stdin pipe %s\n", cmd[0]);
						dup2(p[READ_END], STDIN_FILENO);
						close(p[WRITE_END]);
						close(p[READ_END]);
					}
					/* les redirections */
					if (i == 0 && (l->in)){ // première commande, elle reçoit input
						int fdi = open(l->in, O_RDONLY);
						dup2(fdi , STDIN_FILENO);
						close(fdi);
					}
					if (l->seq[i + 1]==0 && (l->out)){ //derniere commande, écrit output
						int fdo = open(l->out, O_WRONLY | O_TRUNC | O_CREAT);
						dup2(fdo, STDOUT_FILENO);
						close(fdo);
					}
					/* la commande */
					if (execvp(cmd[0], cmd) == -1){
						printf("fils : Commande non reconnue\n");
						run = 0; // on arrête le processus !
					};
				  break;
				default: //le pere
				  {
						/* gestion du backgroud ? */
						int status;
				    printf("pere: commande %s lancée sur au pid %d", cmd[0], pid);
						if (!l->bg){ // attendre la fin du fils
							printf("\n");
							close(p[WRITE_END]);
							waitpid(pid, &status, 0);
						}
						else if (new_processus < N_PROCESSUS_MAX - 1){ // fils en arrière plan
							printf(" en backgroud\n");
							/* table des processus pour jobs */
							proc_table[new_processus].pid = pid;
							strcpy(proc_table[new_processus].commande, cmd[0]);
              gettimeofday(&tp, NULL);
              proc_table[new_processus].elapsed_time = (long int)tp.tv_sec;
							new_processus ++;

							/* le traitant de terminaison */
							traitant.sa_handler = interruption_handler;
							sigemptyset ( & traitant.sa_mask );
							traitant.sa_flags = 0 ;
							if ( sigaction(SIGCHLD, &traitant, NULL) == -1 )
								perror("erreur sigaction");
						}
				    break;
				  }
				}
			}
		}
	}

}
