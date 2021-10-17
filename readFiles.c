/**
 * * Alunos: Murilo Xavier de Oliveira e Rodrigo Vogt
 * 
 * * Operating System: Ubuntu 20.04.2 LTS
 * * Kernel: Linux 5.11.0-37-generic
 * 
 * * Compilado com a versão:
 * * gcc version 9.3.0 (GCC)
 **/

#include <math.h>
#include "funcs.h"

#define CONST 40
#define MAX 200


//vetor de custos e adjacencias
int *cost;


//Conta a quantidade de vértices 
int countIn(char rot[CONST]){
  FILE *arq;
  int i,adj;
  int count = 0;
  char ip[CONST];
  
  if(!(arq = fopen(rot,"r"))){
    printf("O arquivo de configuracao roteador.config nao foi encontrado\n");
    return 0;
  }
  while(fscanf(arq,"%d %d %s", &i, &adj, ip)!=EOF)count++;
	
  fclose(arq);
  return count;

}

/** Cria a tabela de roteamento | computa as distancias iniciais para seus vizinhos e ao demais seta como INFINITO */ 
tabela_t *leEnlaces( char enl[CONST], int count, int myId){

  tabela_t *myConnect;
  FILE *arq;
  int i,adj,custo;

  if(!(myConnect = (tabela_t *)malloc(sizeof(tabela_t))))
		return NULL;

  
  if(!(myConnect->idVizinho = (int *)malloc(sizeof(int)*(count))) ||
     (!(myConnect->custo = (int *)malloc(sizeof(int)*(count)))) ||
     (!(myConnect->idImediato = (int *)malloc(sizeof(int)*(count))))||
     (!(myConnect->enlace = (int *)malloc(sizeof(int)*(count)))))
      return NULL;
  
  if(!(arq = fopen(enl,"r"))) return NULL;

   //inicializa a tabela de roteamento
  for(i=0; i< count;i++){
	  myConnect->idVizinho[i] = i+1;
	  myConnect->custo[i] = INFINITO;
	  myConnect->idImediato[i] = i+1;
	  myConnect->enlace[i] = 0;
	  
  }
  //custo para si mesmo é zero    
  myConnect->idVizinho[myId-1] = myId; 
  myConnect->custo[myId-1] = 0;
  myConnect->idImediato[myId-1] = myId;
  myConnect->enlace[myId-1] = 2;
  myConnect->alterado = 0;

  //lê arestas com respectivos custos
  while(fscanf(arq,"%d %d %d", &i, &adj, &custo) != EOF){
		if(i == myId && adj != myId){
			myConnect->idVizinho[adj-1] = adj;
			myConnect->custo[adj-1] = custo;
			myConnect->idImediato[adj-1] = adj;
			myConnect->enlace[adj-1] = 1;
		}
		else if( i != myId && adj == myId){
			myConnect->idVizinho[i-1] = i;
			myConnect->custo[i-1] = custo;
			myConnect->idImediato[i-1] = i;
			myConnect->enlace[i-1] = 1;
		}
  }
  fclose(arq);
  return myConnect;
}


//Devolve as informações sobre o roteador X (id, porta, ip)
router_t *leInfos(char rout[CONST], int id){

	FILE *arq = fopen(rout, "r");
	router_t *myRouter = (router_t *)malloc(sizeof(router_t)*1);

	if(!myRouter)return NULL;

	if(!arq)return NULL;

	while(fscanf(arq,"%d %d %s", &(myRouter->id), &(myRouter->port), myRouter->ip)!=EOF){
		if(id == myRouter->id)break;
	}

	if(id != myRouter->id){return NULL;}

	fclose(arq);
	return myRouter;
}
