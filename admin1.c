/*
Ana Catarina Jesus Gonçalves, 2013167088
Ana Rita Ferreira Alfaro, 2013150362
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define NAMED_PIPE "input_pipe"
#define CONFIG_FILE "config.txt"
#define INPUT_BUFFER_SIZE 50
#define MAX_BUFFER_SIZE 128

void load_configs();
void print_config();

typedef struct {
	int n_threads;//numero de threads
	int n_docts;//numero de processos doutores
	int shift_length;//duracao do turno
	int mq_max;
}config_struct;
config_struct * config;
int fd;


int main(int argc, char const *argv[]) {
	char command[512];
	int value;//valor do comando
	char nome[100];
	int tempoT, tempoA, prioridade;


  	if ((fd=open(NAMED_PIPE, O_WRONLY )) == -1) {
	    perror("Cannot open pipe for writing: ");
	    exit(0);
  	}

  	config = (config_struct *) malloc(sizeof(config_struct));
	load_configs();
	printf("Welcome Admin!\n Change configurations -> configs <COMMAND value>\n Insert patient -> pacient <NAME TIME_T TIME_A PRIORITY>\n Show Statistics -> Stat\n Show configurations -> show\n Exit -> exit\n");

	while(1) {
	    memset(command, 0, strlen(command));
	    fgets(command, MAX_BUFFER_SIZE, stdin);
	    if (command[strlen(command) - 1] == '\n')
	      	command[strlen(command) - 1] = '\0';
	    if(sscanf(command,"configs TRIAGE %d",&value) == 1) {
			config->n_threads=value;
			write(fd, &command, strlen(command) + 2);
	    }
		else if(sscanf(command,"configs DOCTORS %d",&value) == 1){
			config->n_docts=value;
			write(fd, &command, strlen(command) + 2);
		}
		else if(sscanf(command,"configs SHIFT_LENGTH %d",&value) == 1){
			config->shift_length=value;
			write(fd, &command, strlen(command) + 2);
		}
		else if(sscanf(command,"configs MQ_MAX %d",&value) == 1){
			config->mq_max=value;
			write(fd, &command, strlen(command) + 2);
		}          
	    else if(sscanf(command, "pacient %c %d %d %d", nome,&tempoT,&tempoA,&prioridade) == 1) {
	      	write(fd, &command, strlen(command) + 2);
	    }
	    else if(strcmp(command, "Stat") == 0) {
	  		write(fd, &command, strlen(command));
	    }   
	    else if(strcmp(command, "show") == 0) {
	      	print_config();
	    } 
	    else if(strcmp(command, "exit") == 0) {
	      	exit(0);
	    }
	    else{
	    	printf("ERRO\n");
	    }
	}
}

void load_configs(void){
    char *token;
    char line[50];
    FILE *file;
    config = (config_struct *) malloc(sizeof(config_struct));
    if ((file = fopen(CONFIG_FILE, "r")) == NULL) {
	    perror("Config file missing\n");
	    exit(1);
  	}
	if(file){
        while(fgets(line,80,file)!=NULL){
            token=strtok(line,"=");
            if(strcmp(token,"TRIAGE")==0){
                token=strtok(NULL,"=");
                config->n_threads=atoi(token);
            }
            else if(strcmp(token,"DOCTORS")==0){
                token=strtok(NULL,"=");
                config->n_docts=atoi(token);
            }
            else if(strcmp(token,"SHIFT_LENGTH")==0){
                token=strtok(NULL,"=");
                config->shift_length=atoi(token);
            }
            else if(strcmp(token,"MQ_MAX")==0){
                token=strtok(NULL,"=");
                config->mq_max=atoi(token);
            }
        }
        fclose(file);
	}
	else{
		printf("Ficheiro de configuracao nao encontrado\n");
		exit(1);
	}
}

void print_config() {
  printf("\nConfigurações:\n");
  printf("Numero de threads na triagem: %d\n", config->n_threads);
  printf("Numero de processos doutor: %d\n", config->n_docts);
  printf("Duracao do turno em segundos: %d s\n", config->shift_length);
  printf("Tamanho maximo da fila para atendimento: %d\n", config->mq_max);
}