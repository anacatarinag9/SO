/*
Ana Catarina Jesus Gonçalves, 2013167088
Ana Rita Ferreira Alfaro, 2013150362
*/
typedef struct Paciente *pac;
typedef struct Paciente{ 
	char nome[30];
	int tempoT;//tempo necessario para a triagem
	int tempoA;//tempo necessario para o atendimento
	int prioridade;
	char * instante_inicial;
	char * instante_triagem;
	char * instante_atend;
	char * instante_saida;
	pac prox;

}Pacientes;

pac criaLista(){
	pac aux;
	aux=(pac)malloc(sizeof(Pacientes));

	if(aux!=NULL)
	{		
		memset(aux->nome,0,sizeof(aux->nome));
		aux->tempoT=0;
		aux->tempoA=0;
		aux->instante_inicial=0;
		aux->instante_triagem=0;
		aux->instante_atend=0;
		aux->instante_saida=0;
		aux->prioridade=0;
		aux->prox=NULL;
	}
	return aux;
}

void inserePaciente(pac lista, char nome[],int tempoT, int tempoA,int prioridade,char* instante_inicial){

	pac node=(pac)malloc(sizeof(Pacientes));
	pac node2=lista;

	if(node !=NULL)
	{
		strcpy(node->nome,nome);
		node->tempoT=tempoT;
		node->tempoA=tempoA;
		node->prioridade=prioridade;
		node->instante_inicial=instante_inicial;
		node->prox=NULL;
	}
	while(node2->prox!=NULL){
		node2=node2->prox;
	}
	node2->prox=node;

	 #if DEBUG
        printf("Paciente inserido\n");
    #endif 

}
pac retiraPaciente(pac lista){
    if(lista->prox==NULL)
    {
        printf("Lista vazia");
    }
    else{
        pac aux=lista->prox;
        pac deleted=(pac)malloc(sizeof(Pacientes));
        if(deleted!=NULL){
            strcpy(deleted->nome, aux->nome);
            deleted->tempoT=aux->tempoT;
            deleted->tempoA=aux->tempoA;
            deleted->prioridade=aux->prioridade;
            deleted->instante_inicial= aux->instante_inicial;
            deleted->prox = aux->prox;
        }
        
        lista->prox=aux->prox;  
        free(aux);
        printf("\nPaciente %s retirado da fila\n",deleted->nome);
        return deleted;
        
    }
}

int vazia(pac lista){
    return (lista->prox == NULL ? 1 : 0);
}
pac proximo(pac paci){
    if(vazia(paci)){
        return NULL;
    }
    else{
        return paci->prox;
    }
}
