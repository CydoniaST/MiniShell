#include "parser.h"
#include <stdio.h>

#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define READ_END 0
#define WRITE_END 1

struct proceso{
	pid_t pid;
	char comando[1024];
};

struct proceso * procesos;
int num_proceso = 0;

bool comandoPATH(char * comando){
	char *path_env = getenv("PATH"); // obtener el valor de la variable de entorno PATH
    char *dir = strtok(path_env, ":"); // dividir la cadena PATH en los diferentes directorios
	bool contiene = false;
    char path[256];

    while (dir != NULL) {

        snprintf(path, sizeof(path), "%s/%s", dir, comando); // construir la ruta completa al archivo ejecutable

        if (access(path, X_OK) == 0) { // verificar si el archivo existe y se puede acceder a él
            contiene = true;
        }

        dir = strtok(NULL, ":");
    }

	return contiene;
}

void anadirProceso(pid_t pid, char * buf, int num_proceso){
	
	procesos = (struct proceso *) realloc(procesos, (num_proceso + 1) * sizeof(struct proceso));
	procesos[num_proceso].pid = pid;
	strncpy(procesos[num_proceso].comando, buf, 1024);
}

int unicoMandato(tline * line, int num_proceso, int status){
	char buffDirectorio[512];
	char *simb;

	if(!(strcmp(line->commands[0].argv[0], "cd"))){
		
		if(line->commands[0].argc < 2){
			if(line->commands[0].argc == 1){ //Si solo hay 1 argumento, el de "cd", irá a la carpeta HOME
				chdir(getenv("HOME"));//Sin utilizar la funcion "getenv()" no funcia el cambio al directorio HOME. 
					if(getcwd(buffDirectorio, sizeof(buffDirectorio)) != NULL){
						printf("El directorio actual es: %s", buffDirectorio);
					}else{//Si getcwd devuelve NULL quiere decir que ha ocurrido un error al cambiar de directorio
						printf("Error al obtener el directorio actual.");
					}
				return 0;
			}
			fprintf(stderr, "Error: no se especificó un directorio como segundo argumento al que cambiar.");
			return 1;
		}else{
			if(line->commands[0].argc == 2){ //Si  hay 2 argumentos, el de "cd" y el directorio, cambiará a ese directorio
				if(getcwd(buffDirectorio, sizeof(buffDirectorio)) == NULL){
					perror("Error al cambiar de directorio.");
					return 0;
				}else{
					chdir(line->commands[0].argv[1]);
					if(getcwd(buffDirectorio, sizeof(buffDirectorio)) != NULL){
						printf("El directorio actual es: %s", buffDirectorio);
					}else{//Si getcwd devuelve NULL quiere decir que ha ocurrido un error al cambiar de directorio
						printf("Error al obtener el directorio actual.");
					}
				}
				return 0;
			}
		}

	}else if(!(strcmp(line->commands[0].argv[0], "jobs"))){

		for(int i = 0; i < num_proceso ; i++){
			simb = " ";

			if(i == num_proceso - 2){
				simb = "-";
			}else if(i == num_proceso - 1){
				simb = "+";
			}
			
			printf("[%d]%s Running                  %s\n", procesos[i].pid, simb, procesos[i].comando);
			
		}

	}else if(!(strcmp(line->commands[0].argv[0], "fg"))){
		bool contiene;
		int pos;
		pid_t pidAux;

		if(line->commands[0].argc < 2){

			waitpid(procesos[num_proceso - 1].pid, &status, 0);
			procesos = (struct proceso *) realloc(procesos, num_proceso * sizeof(struct proceso));
			num_proceso--;
		}else{
			contiene = false;
			pidAux = atoi(line->commands[0].argv[1]);
			
			for(int i = 0; i < num_proceso; i++){
				if(procesos[i].pid == pidAux){
					contiene = true;
					pos = i;
				}
			}
			if(contiene){
				waitpid(pidAux, &status, 0);
				for(int i = pos; i < num_proceso - 1; i++){
					procesos[i] = procesos[i + 1];
				}
				procesos = (struct proceso *) realloc(procesos, num_proceso * sizeof(struct proceso));
				num_proceso--;
			}else{
				printf("Ninguno de los procesos ejecutados en segundo plano contiene el pid introducido.");
			}
		}

	}

	return num_proceso;

} 

void manejador(int sig){
	int status;
	int pid;
	int pos;
	bool contiene = false;

	if(sig == SIGCHLD){

		while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
		{
			if (WIFEXITED(status)){
				contiene = false;
				
				for(int i = 0; i < num_proceso; i++){
					if(procesos[i].pid == pid){
						contiene = true;
						pos = i;
					}
				}
				if(contiene){
					waitpid(pid, &status, 0);
					for(int i = pos; i < num_proceso - 1; i++){
						procesos[i] = procesos[i + 1];
					}
					procesos = (struct proceso *) realloc(procesos, num_proceso * sizeof(struct proceso));
					num_proceso--;
				}
			}
		}
	}
}

int
main(void) {

	char buf[1024];
	tline * line;
	int i;
	pid_t pid;
	int fdinput, fdoutput;
	int status;
	procesos = (struct proceso *)malloc(num_proceso * sizeof(struct proceso));

	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGCHLD, manejador);

	printf("msh > ");	
	while (fgets(buf, 1024, stdin)) {
		int only_spaces = 1;
		for (long unsigned int i = 0; i < strlen(buf); i++) {
	        	if (!isspace(buf[i])) {
		            only_spaces = 0;
		            break;
		        }
		}
		if ((strcmp(buf, "\n") != 0) && !only_spaces) {
			line = tokenize(buf);
		
			int fds[line->ncommands - 1][2];
			if (!(strcmp(line->commands[0].argv[0], "exit"))){
				break;
			}
			if (line==NULL) {
				continue;
			}

			for (i=0; i<line->ncommands - 1; i++) {

				pipe(fds[i]);

			}

			if (!(strcmp(line->commands[0].argv[0], "cd")) || !(strcmp(line->commands[0].argv[0], "jobs")) || (num_proceso > 0 && !(strcmp(line->commands[0].argv[0], "fg")))){

				num_proceso = unicoMandato(line, num_proceso, status);

			}else{

				for (i=0; i<line->ncommands; i++) {

					pid = fork();
					if (pid < 0) { /* Error */
						fprintf(stderr, "Falló el fork()");
						exit(-1);
					}
					else if (pid == 0) { /* Proceso Hijo */
						if(!line->background){
							signal(SIGINT, SIG_DFL);
							signal(SIGQUIT, SIG_DFL);
						}
						if(line->ncommands > 1){
							if(i == 0){

								if (line->redirect_input != NULL) {
									fdinput = open(line->redirect_input, O_RDONLY);
									dup2(fdinput, STDIN_FILENO);
									close(fdinput);
								}

								close(fds[i][READ_END]);
								dup2(fds[i][WRITE_END], STDOUT_FILENO);
								close(fds[i][WRITE_END]);
								execvp(line->commands[i].filename, line->commands[i].argv);
							}else if(i > 0 && i < (line->ncommands - 1)){

								close(fds[i][READ_END]);
								//Tiene que cerrar la escritura del pipe anterior para asi leer de el
								dup2(fds[i-1][READ_END], STDIN_FILENO);
								close(fds[i-1][READ_END]);
								
								//Tiene que cerrar la lectura del pipe actual para asi escribir en el
								dup2(fds[i][WRITE_END], STDOUT_FILENO);
								close(fds[i][WRITE_END]);

								execvp(line->commands[i].filename, line->commands[i].argv);
							}else{

								dup2(fds[i-1][READ_END], STDIN_FILENO);
								close(fds[i-1][READ_END]);
								
								if (line->redirect_output != NULL) {
									//printf("redirección de salida: %s\n", line->redirect_output);
									fdoutput = open(line->redirect_output, O_CREAT | O_WRONLY, 0777);
									
									dup2(fdoutput, STDOUT_FILENO);
									//close(fdoutput);
								}
								execvp(line->commands[i].filename, line->commands[i].argv);
							}
						}else {
								if (line->redirect_output != NULL) {
									fdoutput = open(line->redirect_output, O_CREAT | O_WRONLY, 0777);

									dup2(fdoutput, STDOUT_FILENO);
									//close(fdoutput);
								}
								execvp(line->commands[i].filename, line->commands[i].argv);
						}
						fprintf(stderr,"Se ha producido un error.\n");
						exit(1);
					}
					if(line->ncommands > 1){
						if (i == 0){
							close(fds[i][WRITE_END]);
						} else if(i > 0 && i < (line->ncommands - 1)){
							close(fds[i-1][READ_END]);
							close(fds[i][WRITE_END]);
						}
					}
				}

				if (!line->background) {
					for (i=0; i<line->ncommands - 1; i++) {
						close(fds[i][WRITE_END]);
						close(fds[i][READ_END]);
						wait (&status);
					}
					wait (&status);
				}else{
					anadirProceso(pid, buf, num_proceso);
					num_proceso++;
				}
			}
		}
		printf("msh > ");	
	}
	return 0;
}
