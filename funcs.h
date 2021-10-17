/*
 * Murilo Xavier de Oliveira
 * Rodrigo Vogt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONST 40 //constante para tamanho de arquivo
#define IP 15 //constante do ip
#define MAX_PARENT 20 //vetor de pais tamanho fixo
#define MAXFILA 100//tamanho máximo da fila 
#define MAX_TENTATIVAS 3 //número máximo de tentativas após timeout
#define TIMEOUT 2 //temporizador
#define INFINITO 999 //representacao do infinito
#define MAX_TIME_DV 4 //tempo de envio do DV
#define CONSTANTE_DV 20 //numero de roteadores no vetor de distancia 


/** Tabela de roteamento
 * @struct tabela_t - representa a tabela de roteamento.*/
typedef struct tab{  
  int alterado; //flag para o caso de tiver atualizações na tabela
  int *enlace;  //flag para cada roteador ( identificar as adjacencias após inicializar)
  int *idVizinho; //vetor contendo o id de cada roteador do grafo
  int *custo;   //vetor de custo mínimo para cada roteador
  int *idImediato; //o próximo roteador no caminho até o destino | proximo vertice no caminho até idVizinho
}tabela_t;


/** @struct DistVector_t - Estrutura do vetor de distancia */
typedef struct dv{
	int router[MAX_PARENT]; //vetor que com id's de cada roteador
	int dist[MAX_PARENT]; //a distância estimada 
}DistVector_t;

/** @struct msg_t - representa o pacote a ser enviado, juntamente com a mensagem do usuário. */
typedef struct mensagem{
  int tipo; //tipo da msg ( 1. Usuário; 2. Vetor de distancia)
  int idMsg; //identificador da mensagem
  int origem; //roteador que enviou a mensagem
  int destino; //roteador destino para a msg
  int nextH; //próximo roteador no caminho até o destino
  char ip[IP]; //ip do rot. origem | que enviou
  char text[105]; //a mensagem em si (Max. 100 bytes). (nao usado para msg do tipo 2)
  int pSize;  //contador do vetor de parent. (nao usado para msg do tipo 2)
  int ack; //flag de confirmação de pacote. (nao usado para msg do tipo 2)
  int parent[MAX_PARENT]; //vetor estático que conta os roteador pelo caminho. (nao usado para msg do tipo 2)
  DistVector_t DV;  //vetor de distancia para ser enviado ( nao usado para msg do tipo 1)
}msg_t;


/** @struct router_t - Representa as informações sobre um roteador. */
typedef struct rt{
  
  int id;  //id do roteador
  int port; //porta associada ao socket | que este roteador escuta
  char ip[IP]; //endereço de ip
}router_t;


tabela_t *leEnlaces( char enl[CONST], int count, int myId); //lê tabela de roteamento
router_t *leInfos(char rout[CONST], int id); //lê as info sobre roteador
int countIn(char rot[CONST]); //conta vértices do grafo
void enviarMsg(void); //Envia msg do usuário
void server(void); //server que recebe as mensagens
void serverControl(void); //controle das filas
msg_t initDV(msg_t me, int who, int id);//inicia o DV
void SendDV(void); //thread que controla o DV
