/*
Ana Catarina Jesus Gonçalves, 2013167088
Ana Rita Ferreira Alfaro, 2013150362
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <pthread.h>
#include "paciente.h"

#define LOG_FILE "server.log"
#define NAMED_PIPE "input_pipe"
#define CONFIG_FILE "config.txt"
#define INPUT_SIZE 128

void load_configs(void);
void print_config();
void listen_named_pipe();
void send_pipe_info(char*command);
void atendimento();
void init();
void run_stats();
void init_pipe();
void init_mq();
void init_mem();
void init_semaforos();
void init_configs();
void main_process();
void* atribuiPrioridade(void *arg);
void threads_pool();
void processos();
void clean_threads();
char * get_current_time();
char* straddcharacter_2(const char* a, const char* b);
int calcule_time_diff(char * arrival, char * handled);
void cleanup();

///////CONFIGURACOES//////
typedef struct {
	int n_threads;//numero de threads
	int n_docts;//numero de processos doutores
	int shift_length;//duracao do turno
	int mq_max;
}config_struct;
config_struct *runner;

///////MEMORIA PARTILHADA////////
typedef struct{ 
	int pac_triados;
	int pac_atendidos;
	int pre_triagem; //tempo de espera antes do inicio da triagem
	int pre_atendimento; //tempo de espera entre o fim e o injicio do atendimento
	int tempoTotal;
}dados;
dados* mem;
int shmid;
/////////////////////////
//pool de threads
pthread_t * threads;
int* threadsID;
int*busy;

//processos doutores
pid_t doutor;
//queue pré triagem
pac preTriagem;
//fila de mensagens
int msqid;
//semaforos
sem_t* mutex;
sem_t*mutex2;
sem_t*empty;
sem_t*full;
//named pipe
int fd;

int main(){
	int i,j;
	char * inst;
	FILE *file;

	printf("Processo principal %ld iniciou!\n",(long) getpid());
	char* arranque=get_current_time();
	printf("Started at %s\n",arranque);
	file=fopen(LOG_FILE,"a");
	fprintf(file, "O sistema iniciou: %s\n",arranque);
	fclose(file);

	preTriagem=criaLista();
	inst=get_current_time();
	inserePaciente(preTriagem,"Catarina",5,2,1,inst);
	inst=get_current_time();
	inserePaciente(preTriagem,"Joana",5,5,2,inst);
	inst=get_current_time();
	inserePaciente(preTriagem,"Rita",5,2,3,inst);
	inst=get_current_time();
	inserePaciente(preTriagem,"José",5,2,1,inst);

	signal(SIGUSR1, run_stats);
	signal(SIGINT, cleanup);

	init();

	kill(getpid(),SIGUSR1);

	return 0;
}

void init(){
	init_pipe();
	load_configs();
	init_mem();
	init_mq();
	init_semaforos();
	main_process();
}

void init_pipe(){

	if((mkfifo(NAMED_PIPE,O_CREAT|O_EXCL|0600)<0)&&(errno!=EEXIST)){  //CRIAÇÃO DO NAMED PIPE
		perror("Cannot creat pipe: ");
		exit(0);
	}
	printf("CRIOU\n");

	if((fd=open(NAMED_PIPE,O_RDONLY))<0){
		perror("Cannot open named pipe for reading");
		exit(0);
	}
	printf("Abriu Named Pipe\n");
	#if DEBUG
		printf("Named pipe criado e aberto para leitura!\n");
	#endif

}

void load_configs(void){
    char *token;
    char line[50];
    FILE *file;
    runner = (config_struct *) malloc(sizeof(config_struct));
    if ((file = fopen(CONFIG_FILE, "r")) == NULL) {
	    perror("Config file missing\n");
	    exit(1);
  	}
 
	if(file){
        while(fgets(line,80,file)!=NULL){
            token=strtok(line,"=");
            if(strcmp(token,"TRIAGE")==0){
                token=strtok(NULL,"=");
                runner->n_threads=atoi(token);
            }
            else if(strcmp(token,"DOCTORS")==0){
                token=strtok(NULL,"=");
                runner->n_docts=atoi(token);
            }
            else if(strcmp(token,"SHIFT_LENGTH")==0){
                token=strtok(NULL,"=");
                runner->shift_length=atoi(token);
            }
            else if(strcmp(token,"MQ_MAX")==0){
                token=strtok(NULL,"=");
                runner->mq_max=atoi(token);
            }
        }
        fclose(file);
	}
	else{
		printf("Ficheiro de configuracao nao encontrado\n");
		exit(1);
	}
	#if DEBUG
		printf("Configuracoes atualizadas apos a leitura do ficheiro config.txt!\n");
	#endif
}

void init_mem(){
	shmid=shmget(IPC_PRIVATE,sizeof(dados),IPC_CREAT|0700); //CRIAÇÃO DA MEMORIA PARTILHADA
	mem=(dados*)shmat(shmid,NULL,0);
	mem->pac_triados=0;
	mem->pac_atendidos=0;
	mem->pre_triagem=0;
	mem->pre_atendimento=0;
	mem->tempoTotal=0;
	shmdt(mem);
	 #if DEBUG
		printf("Memoria partilhada criada!\n");
	#endif
}

void init_mq(){
	if((msqid=msgget(IPC_PRIVATE, IPC_CREAT|0666))<0)
	{
		printf("erro\n");
		exit(1);
	}
	/*else{
		//printf("msgget: msgget succeeded: msqid= %d\n",msqid);
	}*/
	#if DEBUG
		printf("Fila de mensagens criada!!\n");
	#endif

}

void init_semaforos(){
	int i;
	sem_unlink("mutex");
	mutex=sem_open("mutex",O_CREAT|O_EXCL,0700,1);

	sem_unlink("mutex_2");
	mutex2=sem_open("mutex_2",O_CREAT|O_EXCL,0700,1);

	sem_unlink("sem_thread");
	empty=sem_open("sem_thread",O_CREAT|O_EXCL,0700,runner->n_threads);

	sem_unlink("full");
	full=sem_open("full",O_CREAT|O_EXCL,0700,0);

    #if DEBUG
		printf("Semaforos criados!\n");
	#endif
}

void main_process(){
	threads_pool();
	processos();
	listen_named_pipe();	
}

void listen_named_pipe(){
	printf("\n\n ESTAMOS A ESCUTA \n\n");
	char command[INPUT_SIZE];
	while(1){
		read(fd,&command,1256967);
		printf("Servidor recebeu %s.\n",command);
		send_pipe_info(command);
	}

	#if DEBUG
		printf("A ler named pipe!\n");
	#endif
}

void send_pipe_info(char*command){
	int threads, docts, shift, mq;
	char nome[30];
	int tempoT, tempoA, prioridade;
	int i;
	char *inst;
	char *nome1;
	runner = (config_struct *) malloc(sizeof(config_struct));

	if(sscanf(command,"configs TRIAGE %d",&threads) == 1){
	 	runner->n_threads=threads;
	 	threads_pool();
	 	printf("passou estatisticas!");
	 }
	 else if(sscanf(command,"configs DOCTORS %d",&docts) == 1){
	 	runner->n_docts=docts;	
	 	processos();
	 } 	
	 else if(sscanf(command,"configs SHIFT_LENGTH %d",&shift) == 1){
	 	runner->shift_length=shift;
	 }
	 else if(sscanf(command,"configs MQ_MAX %d",&mq)== 1){
	 	runner->mq_max=mq;
	 }
	 else if(sscanf(command, "pacient %c %d %d %d",nome,&tempoT,&tempoA,&prioridade) == 1){
	 		printf("tempo t: %d\n", tempoT/1000000);
	 		pac paci=(pac)malloc(sizeof(Pacientes));;
	 		strcpy(paci->nome,nome);
	 		paci->tempoT=tempoT;
	 		strcpy(nome1,nome);
	 		printf("tempo tt %d\n",paci->tempoT);

	 		inst=get_current_time();
	 		inserePaciente(preTriagem,nome1,(tempoT/1000000),tempoA,prioridade,inst);
	 		printf("Paciente %s inserido com sucesso na queue\n",nome );
	 		pac paciente=proximo(preTriagem);
	 		printf("Proximo %s\n",paciente->nome);
	 		i=0;
	 		while(busy[i]!=0){
	 			if(i>=runner->n_threads){
	 				i=0;
	 			}
	 			else{
	 				i++;
	 			}
	 		}
 				pthread_t p;
 				printf("econtrei thread livre!\n");
 				threadsID[i] = i;
 				sem_wait(empty);
				pthread_create(&p, NULL,atribuiPrioridade,&threadsID[i]);
			
	 }
	else if(strcmp(command,"Stat")== 0)
	{
		run_stats(1);
	}

	#if DEBUG
		printf("A receber atualizaoes do named pipe!\n");
	#endif
}

void threads_pool(){
	int i,j;
	busy = malloc(sizeof(runner->n_threads*(sizeof(int))));
	threads = malloc(sizeof(runner->n_threads*(sizeof(pthread_t))));
	threadsID = malloc(sizeof(runner->n_threads*(sizeof(int))));
	for(i=0;i<runner->n_threads;i++){
		threadsID[i] = i;
		pthread_create(&threads[i], NULL,atribuiPrioridade,&threadsID[i]);  
	}
	for(j=0;j<runner->n_threads;j++)
	{
		pthread_join(threads[j],NULL);
	}
	
	#if DEBUG
		printf("Criação da pool de threads!\n");
	#endif
}

void* atribuiPrioridade(void *arg){
	int i = *((int *)arg);

	busy[i]=1;//thread ocupada
	dados* stat;
	FILE *file;
	file=fopen(LOG_FILE,"a");
	int buffer_write=0;
	int buffer_size=runner->n_threads*2;

	//printf("Atribuindo prioridade...\n");

			sem_wait(empty);
			sem_wait(mutex2);
			buffer_write++;
			pac prox=retiraPaciente(preTriagem);
			fprintf(file,"Paciente %s retirado da queue para realizar triagem\n",prox->nome);
			char * instante_preT=get_current_time();
			printf("%s\n", instante_preT);
			sleep(prox->tempoT);
			prox->instante_triagem=get_current_time();
			int dif=calcule_time_diff((prox->instante_inicial),instante_preT);
			sem_wait(mutex);
			stat=(dados*)shmat(shmid,NULL,0);
			stat->pac_triados++;
			stat->pre_triagem=dif;
			shmdt(mem);
			sem_post(mutex);
			printf("Prioridade atribuida!\n\n");
			fprintf(file, "Paciente %s foi triado/a com sucesso!\n",prox->nome);
			fclose(file);
			if((msgsnd(msqid,&prox,sizeof(prox),0))<0){
				printf("msgsnd error: %s\n",strerror(errno));
				exit(1);
			}
			else{
				printf("Enviando o paciente... %s\n",prox->nome);
			}
			sem_post(mutex2);
			sem_post(full);
			busy[i]=0;//libertou

	#if DEBUG
		printf("Atribuir prioridade aos pacientes e envia-los para a fila de mensagens!\n");
	#endif
}
	
void processos(){
	int i,j;
	int shift=runner->shift_length;
	FILE*fi;
	fi=fopen(LOG_FILE,"a");
	for(i=0;i<runner->n_docts;i++){
		doutor=fork();	
		if(doutor==0){
			fprintf(fi,"Doctor %d terminou o turno.\n",getpid());
			printf("Doctor %d vai iniciar o seu turno!\n",getpid());
			printf("Aguardo o proximo paciente...\n");
			atendimento(getpid());		
			exit(1);		
		}
	}
	for(j=0;j<runner->n_docts;j++){
		wait(NULL);
	}
	fclose(fi);	
	
	#if DEBUG
		printf("Criação dos processos Doutor!\n");
	#endif	
}

void atendimento(int pidD){
	pac atend;
	FILE*fi;
	dados* stat;

		sem_wait(full);
		sem_wait(mutex2);

		if(msgrcv(msqid,&atend,sizeof(pac),0,0)<0){
			printf("Erro a receber\n");
			exit(1);
		}
		else{
			fi=fopen(LOG_FILE,"a");
			fprintf(fi,"%s\n",atend->nome);
			char* inicioAtend=get_current_time();
			printf("Recebi o paciente %s\n",atend->nome);
			fprintf(fi,"Doctor %d vai iniciar o seu turno!\n",pidD);
			fprintf(fi, "Atendimento recebeu o/a paciente %s\n", atend->nome);	
			sleep(atend->tempoA);
			printf("Terminou o atendimento do paciente %s!\n",atend->nome);
			
			fclose(fi);	
			char *saida=get_current_time();
			int tri_atend=calcule_time_diff((atend->instante_triagem),inicioAtend);
			int entr_saida=calcule_time_diff((atend->instante_inicial),saida);
			
			sem_wait(mutex);
			stat=(dados*)shmat(shmid,NULL,0);
			stat->pac_atendidos++;	
			stat->pre_atendimento=tri_atend;
			stat->tempoTotal=entr_saida;
			shmdt(mem);
			sem_post(mutex);	
			
		}
		sem_post(mutex2);
		sem_post(empty);

	#if DEBUG
		printf("Atendimento de pacientes após a chegada através da fila de mensagens!\n");
	#endif	
}

int calcule_time_diff(char * arrival, char * handled) {
    char * tmp_arrival = straddcharacter_2("", arrival);
    char * tmp_handled = straddcharacter_2("", handled);
    int hour_arrival, hour_handled;
    int minute_arrival, minute_handled;
    int second_arrival, second_handled;
    int milli_arrival, milli_handled;
    int diff = 0;
 
    char * time_arrival = strtok(tmp_arrival, " ");
    time_arrival = strtok(NULL, " ");
    hour_arrival = atoi(strtok(time_arrival, ":"));
    minute_arrival = atoi(strtok(NULL, ":"));
    second_arrival = atoi(strtok(NULL, ":"));
    milli_arrival = atoi(strtok(NULL, ":"));
 
    char * time_handled = strtok(tmp_handled, " ");
    time_handled = strtok(NULL, " ");
    hour_handled = atoi(strtok(time_handled, ":"));
    minute_handled = atoi(strtok(NULL, ":"));
    second_handled = atoi(strtok(NULL, ":"));
    milli_handled = atoi(strtok(NULL, ":"));
 
    if (second_arrival != second_handled) {
        diff += (second_handled - second_arrival) * 1000;
    }
 
    int diff_tmp = milli_handled - milli_arrival;
    if(diff_tmp < 0) diff_tmp = -diff_tmp;
 
    diff += diff_tmp;
 
    return diff;
    #if DEBUG
		printf("Cálculo da diferença do tempo de chegada e de saida!\n");
	#endif
}

char* straddcharacter_2(const char* a, const char* b) {
    size_t len = strlen(a) + strlen(b);
    char *ret = (char*)malloc(len * sizeof(char) + 1);
    *ret = '\0';
    return strcat(strcat(ret, a) ,b);
}

char * get_current_time() {
  struct timeval curTime;
  gettimeofday(&curTime, NULL);
  int milli = curTime.tv_usec / 1000;
  time_t rawtime;
  struct tm * timeinfo;
  char buffer [128];
  char currentTime[128] = "";

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  strftime(buffer, 128, "%Y-%m-%d %H:%M:%S", timeinfo);
  sprintf(currentTime, "%s:%d", buffer, milli);

  return straddcharacter_2("", currentTime);

#if DEBUG
	printf("Dia e hora do instante presente\n");
#endif	
}

void run_stats(int signum){
	printf("\nSIGNAL\n");
	dados *stat;
	sem_wait(mutex);
	stat=(dados*)shmat(shmid,NULL,0);
	int tempo1=(stat->pre_triagem)/(stat->pac_triados);
	int tempo2=(stat->pre_atendimento)/(stat->pac_atendidos);
	int tempo3=(stat->tempoTotal)/(stat->pac_atendidos);
	printf("pre: %d\n",stat->pre_triagem);
	printf("preA %d\n", stat->pre_atendimento);
	printf("total %d\n", stat->tempoTotal);
	printf("Numero total de pacientes triados: %d\n",stat->pac_triados);
	printf("NUmero total de pacientes atendidos: %d\n",stat->pac_atendidos);
	printf("Tempo médio de espera antes do inicio da triagem: %dseg\n",tempo1);
	printf("Tempo médio de espera entre o fim da triagem e o inicio do atendimento: %dseg\n",tempo2);
	printf("Media do tempo total desde a entrada no serviço ate a saida: %dseg\n",tempo3);
	shmdt(mem);
	sem_post(mutex);
	#if DEBUG
		printf("Impressão de estatísticas!\n");
	#endif
}

void print_config() {
  printf("\nConfigurações:\n");
  printf("Numero de threads na triagem: %d\n", runner->n_threads);
  printf("Numero de processos doutor: %d\n", runner->n_docts);
  printf("Duracao do turno em segundos: %d s\n", runner->shift_length);
  printf("Tamanho maximo da fila para atendimento: %d\n", runner->mq_max);

	#if DEBUG
		printf("Impressão dos dados de configuração!\n");
	#endif
}

void clean_threads(){
	int i;
	for(i=0;i<runner->n_threads;i++){
		pthread_cancel(threads[i]);
	}
}

void cleanup(){
	int i;

	clean_threads();

	kill(doutor,SIGINT);
	while(wait(NULL)!=-1);

    clean_threads();

	//memoria partilhada
	shmdt(&shmid);
	shmctl(shmid, IPC_RMID, NULL);
	free(preTriagem);

     //semaforos
     sem_close(mutex);
     sem_close(mutex2);
     sem_close(full);
     sem_close(empty);
     sem_unlink("mutex");
     sem_unlink("mutex_2");
     sem_unlink("full");
     sem_unlink("sem_thread");

    unlink(NAMED_PIPE);
    free(runner);
    printf("Shared memory clean!\n");
	printf("Threads clean!\n");
	printf("Terminaçao de todos os processos!\n");
    exit(0);


	 #if DEBUG
		printf("Servidor limpo!\n");
	#endif

}