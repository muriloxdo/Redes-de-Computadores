
/**
 * * Trabalho de roteamento com Bellman-Ford distribuído
 * 
 * * Alunos: Murilo Xavier de Oliveira e Rodrigo Vogt
 * 
 * * Operating System: Ubuntu 20.04.2 LTS
 * * Kernel: Linux 5.11.0-37-generic
 * 
 * * Compilado com a versão:
 * * gcc version 9.3.0 (GCC) 
 * 
 * * Alunos: Murilo Xavier e Rodrigo Vogt
 * 
 * Programa desenvolvido para em conformidade com as exigências da disciplina de Redes de Computadores da UFFS.
 * Este Programa tem a finalidade de representar a simulação de roteamento em rede, através de processos simulando roteadores,
 * que se comunicam através de pacotes UDP.
 * 
 * Sobre o funcionamento do programa:
 * O programa lê um parâmetro da linha de comando, sendo esse o ID do roteador a ser instanciado.
 * 
 * Primeiramente, lê-se a partir de dois arquivos, “enlaces.config“ e “roteador.config”, as informações sobre os enlaces e
 * sobre os roteadores. Ambos os arquivos devem ser localizados no mesmo diretório que este arquivo fonte. Vale ressaltar que
 * a topologia da rede é estática, ou seja, não sofre alterações durante o seu ciclo de vida;
 * 
 * Os pacotes são processados através de uma fila simples, onde são tratados com a metodologia First In First Out (FIFO),
 * com a exceção das mensagens de confirmação, que chegam na mesma fila, porém são removidas desta assim que recebidas e detectadas.
 * Isto é válido para qualquer mensagem recebida seja ela de escalonamento, seja ela final ou de confirmação.
 * 
 * Para o caso de envio de mensagens do usuário é necessário assegurar um tempo finito de tentativas, deste modo, quando uma mensagem
 * é escalonada para envio, é definido por padrão um tempo de espera(TIMEOUT) de 2 segundos, bem como, também é definido uma quantidade
 * finita de  tentativas,neste caso 3, totalizando o tempo de 6 segundos por pacote.
 *
 * Cada roteador dispõe de uma tabela de roteamento. Nela se encontram as informações sobre todos os outros roteadores,
 * como o próximo salto a ser realizado com um determinado destino como alvo, bem como o custo mínimo para realizar tal ação,
 * custo calculado por uma função de Bellman-Ford distribuído.
 * 
 * Sobre a execução do algoritmo de Bellman-Ford:
 * 
 * Ao iniciar, cada roteador sabe a distância dos saltos somente para seus vizinhos, tomando como base que todos os vizinhos
 * estão ligados, conectados e funcionando normalmente. De tempos em tempos, cada roteador envia seus vetores de distâncias,
 * aqui chamados de Distance Vectors (DV) atualizados para seus vizinhos, e após alguns instantes, as rotas com as distâncias
 * mínimas estão computadas e armazenadas.
 * 
 * Esta ação se repete durante todo o funcionamento, e portanto, se algum roteador tenta enviar um DV para o seu vizinho,
 * mas o vizinho não responde com o DV dele, assume-se que o enlace com aquele roteador "caiu", ou seja, não existe mais, e portanto,
 * assim que essa falha é detectada, as rotas para aquele destino são setadas como tendo distância infinita, para que novos caminhos
 * e saltos possam ser gerados. Quando o roteador não-funcional volta a funcionar, as distâncias são recalculadas, considerando as
 * possíveis rotas que podem ser formadas a partir desse novo nó. Vale ressaltar que o envio dos DV são realizados por uma thread
 * separada.
 *  
 * Com relação à implementação de mensagens de confirmação:
 * 
 * Quando uma mensagem é roteada da origem até o destino, cada roteador do caminho é marcado em um  Parent Vector (PV),
 * ou vetor de parent, assim a confirmação é propagada do destino até a origem, por todos os vetores “pais” pelos quais a
 * mensagem passou.
 * 
 * Como a aplicação permite o envio de mensagens, cada roteador implementa uma interface de interação com o usuário, ou seja,
 * através de uma thread emulada em um terminal, são recebidos um destino e uma mensagem lida do teclado, informadas pelo usuário.
 * Posteriormente esta mensagem é posta na fila para o escalonamento.
 * 
 **/


#include "funcs.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#define GETFILE 105 //constante para mensagem do usuário
#define ALOCACAO_ERRO 0

/** @struct fila_t - Descreve a fila de pacotes do roteador X */
typedef struct fila{
	time_t timestamp;	//Armazena o tempo decorrido na fila do roteador
	int tentativas;	//Quantidade de retransmissões após timeout
	int id;
	msg_t *mesg;	//Estrutura do tipo msg. Representa um pacote
}fila_t;

void remove_f();
int iniciaFila();
void insereFila(msg_t *buf);
void remove_fix(int i);
void insere_fix(msg_t *nova, int save, int saveID, time_t back);

msg_t *copyData(msg_t *new, msg_t *buf);
DistVector_t *copyVector(DistVector_t *new, DistVector_t *buf);

//Variáveis compartilhadas entre as threads
int vertices,vizinhos=0; //Número de vértices do grafo;
int tamanho = 0;  //controle da fila, inicialmente vazia.
tabela_t *myConnect = NULL; //lista de roteamento
tabela_t *myTable = NULL; //copia da lista
router_t *myRouter = NULL;  //informações do roteador
fila_t *filas = NULL;
DistVector_t *vetor = NULL; //vetor de distancia
msg_t mensger[MAX_PARENT];

int *tryVector, flag = 1;

char rout_u[20] = "roteador.config"; //Arquivo de configuração dos roteadores;
char link_u[20] = "enlaces.config"; //Arquivo de conf. dos enlaces;

pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER; //mutex para controlar o acesso as variáveis globais

/** Computa a hora atual */
void printTime(){
	int i;
    time_t ltime; // calendar time 
    ltime=time(NULL); // get current time 
    char *tempo = (char *)malloc(sizeof(char)*30);
    
    tempo = asctime( localtime(&ltime) );
    for(i = 11; i < 20; i++){
		if( i == 11)
			printf(" Time : ");
		printf("%c",tempo[i]);
	}
	printf("\n");	
}

/**
 * Pega as informações da tabela de roteamento e coloca no vetor de distância.
 * @param vector - vetor de distancia 
 */
DistVector_t *atualizaDV(DistVector_t *vector){
	int i;
	for(i = 0; i < vertices; i++){
		vector->dist[i] = myConnect->custo[i];
		vector->router[i] = myConnect->idVizinho[i];
			
	}
		
	myConnect->alterado = 0;
	
	return vector;
}

/**
 * Inicializa o vetor com dados setado como INFINITO
 */
DistVector_t *initDV2(DistVector_t *vetor){
	int i;
	
	for(i = 0; i < vertices; i++){
		vetor->dist[i] = INFINITO;
		vetor->router[i] = INFINITO;
	}
	return vetor;
}

/** Inicializa as estruturas do pacote a ser enviado. Foi utilizado a estrutura das mensagens para o usuário */
msg_t initDV(msg_t me, int who,int id){
	router_t *vis = NULL;
	
	vis = leInfos(rout_u, who);
	
	strcpy(me.ip, vis->ip);
	free(vis);
	me.tipo = 2;
	me.ack = 0;
	me.idMsg = id;
	me.origem = myRouter->id;
	me.destino = who;
	me.nextH = who;
	
	return me;
}

/** Imprime os dados para cada roteador a partir da tabela de roteamento */
void imprimeTabela(){
	int i;
	
	printf("----DV UPDATE---------\n");
	printTime();
  printf(" Roteador : %d\n", myConnect->idVizinho[myRouter->id-1]);
	printf("----------------------\n\n");
	printf(" ROT |   CUSTO\n");
	printf("-----------------------\n");
		for(i = 0 ; i < vertices; i++){
			  
			if(myConnect->idVizinho[i] != myRouter->id){
				if(myConnect->custo[i] == INFINITO)
					printf("   %d |   INF \n", myConnect->idVizinho[i]);
				else printf("   %d |    %d \n", myConnect->idVizinho[i], myConnect->custo[i]);

			  }
			  
		}
	printf("-----------------------\n");
}


/**
  Thread responsável por computar o vetor de distância e encaminhar para a fila do roteador.
  É enviado o DV para os vizinhos este tempo é aproximadamente de 4 segundos. 
  Também é contabilizado a quantidade de vezes de inatividade dos roteadores vizinhos. Se durante 
  um periodo, algum dos roteadores vizinhos não enviar seus respectivos vetores de distâncias,
  é assumido que o enlace caiu. 
**/
void SendDV(void){
  time_t timest;
 
  int i,j,k;
  int *vet;
  int id = 1;
  if(!(vetor = (DistVector_t *)malloc(sizeof(DistVector_t)))){  //inicializa o vetor de distancia
	  printf("Problema para alocar o vetor de distancia\n");
	  return;
  }
  vetor = initDV2(vetor); //inicializa todo mundo com infinito
  vetor->dist[myRouter->id-1] = 0;
  vetor->router[myRouter->id-1] = myRouter->id;//distancia para mim mesmo é zero
  
  
  if(!(vet = (int *)malloc(sizeof(int)*vertices))){
	  printf("Problema com alocacao\n");	
    return;
	}
  
  
  for(i = 0; i < vertices; i++){	  
	  if((myConnect->idVizinho[i] != myRouter->id) && (myConnect->custo[i] != INFINITO) && (myConnect->idImediato[i] == myConnect->idVizinho[i])){
			vet[vizinhos] = myConnect->idVizinho[i];
			vetor->dist[i] = myConnect->custo[i];
			vetor->router[i] = myConnect->idImediato[i];
			vizinhos++;
		}
  }
  
  if(!(tryVector = (int *)malloc(sizeof(int)*vizinhos))){
	  printf("Problema com alocacao dinamica\n");
	  return;
  }

  for(i = 0; i < vizinhos; i++){
    
    for(j = 0; j < vertices; j++){
      mensger[i].DV.dist[j] = vetor->dist[j];
      mensger[i].DV.router[j] = vetor->router[j];
    }
    mensger[i] = initDV(mensger[i], vet[i],id++);
    
    tryVector[i] = 0; 
  }

  free(vet);
  
  timest = time(0);
  
  while(1){
	  usleep(500000); //500ms
	
    pthread_mutex_lock(&count_mutex);
	
    double tempo = difftime(time(0), timest);
	  if(myConnect->alterado == 1){
		  vetor = atualizaDV(vetor);
		  if(!filas)
			if(!iniciaFila())return;
		    
		  for( i = 0; i < vizinhos; i++){
			  if(mensger[i].tipo == 2){
					for(j = 0; j < vertices; j++){
						mensger[i].DV.dist[j] = vetor->dist[j];
						mensger[i].DV.router[j] = vetor->router[j];
					}		
					mensger[i].idMsg = id++;
					
          insereFila(&mensger[i]);
					tryVector[i]++;
					printf("Roteador : %d enviando pacote DV #%d para roteador %d\n", myRouter->id,mensger[i].idMsg, mensger[i].destino);
			  }
		  }
		  timest = time(0);
		 
			imprimeTabela();
			
		  flag = 0;
	  } else if(tempo >= MAX_TIME_DV){  
			if(!filas)
				if(!iniciaFila())return;

			for( i = 0; i < vizinhos; i++){
				if(tryVector[i] >= 5){
					mensger[i].tipo = 0;
					for(j = 0; j < vizinhos; j++){
						mensger[j].DV.dist[mensger[i].destino-1] = INFINITO;
						mensger[j].DV.router[mensger[i].destino-1] = INFINITO; 
					}
					for(k = 0; k < vertices; k++){
						if(mensger[i].destino == myConnect->idImediato[k])
							myConnect->custo[k] = INFINITO;
					}
					printf("v:%d %d \n", mensger[i].destino, tryVector[i]);
				}
				
        if(mensger[i].tipo == 2){
					mensger[i].idMsg = id++;
					insereFila(&mensger[i]);
					tryVector[i]++;
				}	
      }
			timest= time(0);
	  }
	  pthread_mutex_unlock(&count_mutex);
  }
  pthread_exit(NULL);
}

/** 
  Interface de envios de mensagem através do socket para um roteador específico.
 	Lê da teclado um destino válido na topologia da rede, então faz o encaminhamento
  correto até o destino. Esta função executa simultaneamente com as demais funções, controlada
  através de uma thread.
**/
void enviarMsg(void){
	int s, i;
	char message[GETFILE];
	int destino;
	msg_t mensg;
	int id = 1;

	router_t *destRouter = NULL;
	usleep(300000); //espera 300ms
	while(1){
		printf("\n");
		printf("ROTEADOR NUMERO %d\n\n", myRouter->id);
		
    printf("Enviar msg para roteador destino:");
		scanf("%d", &destino);

		fgetc(stdin);//pula \n do scanf
		
    if(destino == myRouter->id){
			printf("\nEnviar para mim mesmo?\n");
			continue;
		}
		
    if(!(destRouter = leInfos(rout_u, destino))){
			printf("Destino invalido\n");
		} else{
		
			for( i = 0; i < vertices; i++){ //inicializa a estrutura do pacote
				if(myConnect[myRouter->id-1].idVizinho[i] == destino){
					mensg.tipo = 1;
					mensg.origem = myRouter->id;
					mensg.destino = destino;
					mensg.nextH = myConnect->idImediato[i];
					mensg.ack = 0;
					mensg.pSize = 0;
					//mensg.DV = NULL;
					strcpy(mensg.ip, destRouter->ip);
					break;
				}
			}

			printf("Msg: ");
			fgets(message, 100,stdin);  //100bytes no máximo
			strcpy(mensg.text, message);
		 
			pthread_mutex_lock(&count_mutex); //usa mutex para iniciar a fila, se necessário
			if(!filas)
				if(!iniciaFila())return;
		
			mensg.idMsg = id++; //identificador da mensagem
			mensg.parent[mensg.pSize] = myRouter->id; //caminho feito é registrado no vetor parent
			mensg.pSize++;
			
			insereFila(&mensg); //insere a mensagem preparada no final da fila
			pthread_mutex_unlock(&count_mutex); //libera para outras thread usarem
		}
	}
  close(s);
  pthread_exit(NULL);
}

/** Responsável por enviar o pacote no socket para algum roteador específico.
 * Envia um pacote para um roteador, controlando se é uma mensagem de confirmação ou não. Esta é uma
 * função auxiliar do controle da fila.
 *
 * @param int s - Socket associado;
 * @param struct sockaddr_in *etc - Estrutura do socket associado;
 * @param msg *buf - A mensagem a ser enviada.
**/
int encaminhaMsg(int s, struct sockaddr_in *etc, msg_t *buf, int flag){ 
	int i;
	int t = sizeof(*etc);
	router_t *destRouter = NULL;
	
	if(buf->ack == 1 && buf->tipo == 1){ //se for mensagem de confirmação (ack)
		for(i = 0; i < vertices; i++){ //procura o roteador vizinho que pertence ao caminho mínimo do destino até a origem
			if(myConnect->idVizinho[i] == buf->parent[buf->pSize]){
				buf->nextH = myConnect->idImediato[i];
			
				if(!(destRouter = leInfos(rout_u, buf->nextH))){
					printf("Destino nao e valido\n");return 0;
				}
				break;
			}	
			
			
		}
	}
	else if(buf->tipo == 2){
		if(!(destRouter = leInfos(rout_u, buf->destino))){
			printf("Destino nao e valido\n");return 0;
		}
	}
	
	else{
		for( i = 0; i < vertices; i++){ //procura o roteador vizinho que pertence ao caminho mínimo da origem até o destino
			if(myConnect->idVizinho[i] == buf->destino){
				buf->nextH = myConnect->idImediato[i];
				if(!(destRouter = leInfos(rout_u, buf->nextH))){
					printf("Destino nao e valido\n");return 0;
				}
				break;
			}
		}
	}

	etc->sin_port = htons(destRouter->port); //especifica a porta do socket
	
	if (inet_aton(destRouter->ip , &etc->sin_addr) == 0) {  //Associa o IP
		fprintf(stderr, "inet_aton() error\n");
		return 0;
	}
  
	if (sendto(s, buf, sizeof(msg_t), 0, (struct sockaddr *)etc, t) == -1) { //tenta enviar
		printf("Nao foi possivel encaminhar a mensagem()...\n");
		return 0;
	}
	if(!buf->ack && buf->tipo == 1) //define os formatos de msg de apresentação para o usuário
		printf("\nRoteador : %d encaminhando msg #%d de %d bytes para roteador : %d\n", myRouter->id,buf->idMsg, (int )strlen(buf->text),destRouter->id); 
	
  free(destRouter);
	return 1;
}

/** 
  Representa o servidor responsável por receber os pacotes enviaos atráves do socket.A função é controlada por
  uma thread particular, onde a mesma é criada por primeiro. Assim, é definido o tipo do socket(neste caso UDP),
  a porta de conexão, variáveis internas de controle, etc. Como a função faz uso da fila (global),
  se faz necessário o uso de mutex para sincronizar os acessos.
  A chamada à função recvfrom é bloqueante, ou seja, neste ponto a thread fica bloqueada enquanto não receber algum
  pacote. Após receber algum pacote, é verificado se o mesmo chegou ao destino ou se ainda é preciso reencaminhar 
  para um próximo roteador. Também é atráves da função recvfrom que as confirmações de pacotes são recebidas.
**/
void server(void){
	int s,temp, recv_len,i,j,flag = 0;
	struct sockaddr_in si_me, si_other;
	socklen_t len;
	msg_t mensg;
	
	if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		printf("Problema com Socket do servidor .\n");
		exit(1);
	}

	memset((char *) &si_me, 0, sizeof(si_me));
	
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(myRouter->port);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if (bind(s , (struct sockaddr*)&si_me, sizeof(si_me)) == -1) {
		printf("bind(). Porta ocupada?.\n");
		exit(1);
	}
	pthread_mutex_lock(&count_mutex); //Acesso a fila, então o uso de mutex
	
	if(!iniciaFila()){
		printf("Impossivel alocar a fila\n");
		exit(1);
	}
	pthread_mutex_unlock(&count_mutex);
	while(1){
		
		fflush(stdout);
		memset((char *)&mensg, 0, sizeof(msg_t));
		
		if ((recv_len = recvfrom(s, &mensg, sizeof(msg_t), 0, (struct sockaddr *)&si_other, &len)) == -1) {
			printf("recvfrom() ");
			exit(1);
		}
		
		pthread_mutex_lock(&count_mutex); //Acesso único também a partir deste ponto
		
    if(mensg.tipo == 2){ //mensagem de dv
      for(j = 0; j < vizinhos; j++){
            if(mensger[j].destino == mensg.origem){
              tryVector[j] = 0;
              break;
            }
      }
      if(j < vizinhos)
        if(mensger[j].tipo == 0)
          mensger[j].tipo = 2;
      
      for(i = 0 ; i < vertices; i++){
        if(mensg.DV.router[i] != INFINITO && mensg.DV.router[i] != myRouter->id) {
          temp = mensg.DV.dist[i] + myTable->custo[mensg.origem-1];
          
          if(temp > INFINITO){
            myConnect->custo[i] = INFINITO;
          }
          else if(myConnect->enlace[i] == 1 && myConnect->idImediato[i] == mensg.origem){
            if(myConnect->custo[i] != temp){
              myConnect->custo[i] = temp;
              flag = 1;
              myConnect->alterado = 1;
            }
          }
          
          else if( (temp  < myConnect->custo[i])){
            myConnect->custo[i] = temp;
            myConnect->idImediato[i] = mensg.origem;
            myConnect->alterado = 1;
          } 
        }
  
      }
      if(myConnect->alterado == 1 )
        printf("Pacote DV #%d recebido do rot. %d\n", mensg.idMsg, mensg.origem);
        
		} else{
			if((mensg.destino != myRouter->id) && (mensg.ack == 0)){ //caso este nao for o destino
				for(i = 0; i < tamanho; i++){ //ignorar pacotes ja recebidos no caso de retransmissoes
					if(mensg.idMsg == filas[i].mesg->idMsg && mensg.nextH == myRouter->id){
						flag = 1;
						break;
					}
				}
				if(!flag){  //se a mensagem nao é repetida
					msg_t *confr = (msg_t *)malloc(sizeof(msg_t));
				
					mensg.parent[mensg.pSize++] = myRouter->id; //coloca no vetor parent este roteador
					confr = copyData(confr,&mensg); //copia a mensagem para uma estrutura dinâmica
					insereFila(confr); //coloca no final da fila
				}
				flag=0; //flag de controle interno
			} else{ //caso este for o destino
				msg_t *conf = (msg_t *)malloc(sizeof(msg_t));
				if(!mensg.ack){ //se nao for msg de confirmacao
					mensg.ack = 1; //flag de confirmacao
					mensg.pSize--; //retira este roteador do caminho em parent
					conf = copyData(conf, &mensg);
					
					printf("\nRoteador : %d recebeu a msg de %d\n", myRouter->id, mensg.origem);
					printf("Msg: %s\n", mensg.text);
						
					insereFila(conf); //coloca na fila para enviar o ack
				} else{ //caso recebe a confirmacao
					for(i = 0; i < tamanho; i++){ //remove da fila o pacote 
						if(mensg.parent[mensg.pSize] == myRouter->id && filas[i].mesg->origem == mensg.origem){
							remove_fix(i);
							break;
						}
					}
					
					if(mensg.origem == myRouter->id){ //se este for o roteador que enviou a mensagem, neste ponto a mesma é confirmada
						printf("\nMensagem confirmada!!\n");
					} else{ //senao apenas propaga a confirmaçao para o vizinho no caminho parent
						mensg.pSize--;
						conf = copyData(conf, &mensg);
						insereFila(conf);
					}
				}
			}
		}
		pthread_mutex_unlock(&count_mutex);
	}
}

/**
 Controla a fila do roteador. A função cria um socket para controlar o envio dos pacotes aos demais roteadores.
 Esta função executa em uma thread separada. Evidentemente usa mutex para o acesso aos elementos da fila. 
 Dentro do laço principal existe um tempo de espera de 500ms, para eventuais sincronizações que possam ocorrer.
 A função basicamente controla quando enviar e reinviar os pacotes, ela controla o timeout. 
 O temporizador está definido como aproximadamente 2 segundos, porém ele faz no máximo 3 tentativas, então aproximadamente
 6 segundos entre todas as possíveis retransmissões.
 A função precisa decidir o estado do pacote, se é pacote de confirmação, se é novo (vindo do usuário) ou se ele está
 sendo retransmitido. A contagem de tempo é feito por uma variável do tipo time_t, onde é comparado a hora do sistema 
 em segundos menos a hora que foi colocado o pacote na fila.
**/
void serverControl(){

	struct sockaddr_in controle;
	time_t back;
	int save,saveId;
	int s,i;

	pthread_mutex_lock(&count_mutex); //precisa exclusividade, pois usa a fila 
	if(!iniciaFila())return;

	pthread_mutex_unlock(&count_mutex);
	if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) { //cria socket UDP
	printf("Socket de controle.\n");
	return;
	}

	controle.sin_family = AF_INET; 
	memset((char *) &controle, 0, sizeof(controle));


	while(1){
		usleep(500000); //espera 500ms

		if(tamanho == 0)continue; //fila vazia
	
		pthread_mutex_lock(&count_mutex);
		for(i = 0; i < tamanho; i++){
			msg_t *conf = (msg_t *)malloc(sizeof(msg_t));
			
      if(filas[i].mesg->ack){ //caso recebeu o pacote
				encaminhaMsg(s,&controle,filas[0].mesg,filas[0].tentativas); //manda ack e remove da fila
				remove_f();
			} else if(filas[0].tentativas == 0){ //se a mensagem nao foi enviada, ou seja, veio do usuario
				int val = encaminhaMsg(s,&controle,filas[0].mesg,filas[0].tentativas);
				
        if(!val || filas[0].mesg->tipo == 2){	
					remove_f(); //caso deu erro, apenas remove
				} else{
					filas[0].tentativas++; //incrementa o numero de tentativas
					filas[0].timestamp = time(0); //conta o tempo a partir de agora
					save = filas[0].tentativas;
					saveId = filas[0].id; //copia em variaveis auxiliares
					back = filas[0].timestamp;
					conf = copyData(conf, filas[0].mesg); //copia para estrutura dinâmica
					
					if(filas[0].mesg->tipo == 1){ //se nao for mensagem do atualizado do DV
						remove_f();
						insere_fix(conf, save, saveId, back); //tem efeito de pegar o primeiro e colocar por último na fila, mantendo os atributos
					}
				}
			} else if(filas[0].tentativas > 0 && filas[0].tentativas < MAX_TENTATIVAS){
				double tempo = difftime(time(0), filas[0].timestamp);
			 
				 if(filas[0].tentativas < MAX_TENTATIVAS && tempo > TIMEOUT){ //max 3 tentativas
					if(filas[0].mesg->tipo == 1)
						printf("\nRetransmissao\n");
					if(encaminhaMsg(s,&controle, filas[0].mesg,filas[0].tentativas)==0){
						remove_f();
					}	else{
						filas[i].tentativas++;
						back = time(0);
						save = filas[0].tentativas;
						saveId = filas[0].id;
						conf = copyData(conf,filas[0].mesg);
						
						if(filas[0].mesg->tipo == 1){ //se nao for mensagem de atualizacao DV
							remove_f();
							insere_fix(conf,save,saveId,back); //coloca no final da fila
						}
					 }
				 } else if(filas[0].tentativas < MAX_TENTATIVAS && tempo <= TIMEOUT){ //Aprox 2s
					 conf = copyData(conf,filas[0].mesg);
					 save = filas[0].tentativas;
					 back = filas[0].timestamp;
					 saveId = filas[0].id;
					 
					 if(filas[0].mesg->tipo == 1){  //se nao for msg de DV
						remove_f();
						insere_fix(conf,save,saveId,back); //coloca no final da fila | total aproximadamente de 6s para tentar enviar
					}
				 }
			} else{
				if(filas[0].mesg->tipo == 1)
					printf("\nNao foi possivel encaminhar a mensagem\n");
					
				remove_f(); //Desiste de enviar
			}
			

		}
		pthread_mutex_unlock(&count_mutex);
	}
	close(s);
	pthread_exit(NULL);
}


/** Faz a copia de dois pacotes, copia a msg A para a msg B */
msg_t *copyData(msg_t *new, msg_t *buf){
	int i;
	new->tipo = buf->tipo;
	new->origem = buf->origem;
	new->destino = buf->destino;
	new->nextH = buf->nextH;
	strcpy(new->ip, buf->ip);
	if(buf->tipo==1)strcpy(new->text, buf->text);
	
	new->ack = buf->ack;
	new->idMsg = buf->idMsg;
	new->pSize = buf->pSize;
	if(buf->tipo == 2){
		for(i = 0; i < vertices; i++){
			new->DV.dist[i] = buf->DV.dist[i];
			new->DV.router[i] = buf->DV.router[i];
		}
	}else
		for(i = 0 ; i < vertices; i++)
			new->parent[i] = buf->parent[i];

	return new;
}
	
//inicializa a fila de pacotes (fila simples)
int iniciaFila(){
	int i;
	filas = (fila_t *)malloc(sizeof(fila_t)*MAXFILA); //fila com tamanho fixo MAXFILA
	
	if(!filas){
		printf("Falha na alocacao\n");
		return 0;
	}
	for(i = 0; i < MAXFILA; i++){
		filas[i].mesg = NULL;
		filas[i].id = 0;
		filas[i].timestamp = 0;
		filas[i].tentativas = 0;
	}
	return 1;
}


//A função realiza a inserção de um pacote no final da fila
void insereFila(msg_t *buf){
	msg_t *nova = (msg_t *)malloc(sizeof(msg_t));

	if(!filas){
		printf("Impossivel inserir. Problema com a fila\n");
		return;
	}
	else if(tamanho >= MAXFILA){
		printf("Fila cheia. Roteador sobrecarregado.\n");
		return;
	}	else{
		nova = copyData(nova,buf);
	
    filas[tamanho].mesg = nova;
    if(tamanho == 0){
      filas[tamanho].id = 1; //primeiro pacote tem id=1 
      filas[tamanho].timestamp = filas[tamanho].tentativas =0;
      
    
    } else{
      filas[tamanho].id = filas[tamanho-1].id+1; //id incremental a partir de 1
      filas[tamanho].timestamp = filas[tamanho].tentativas =0;
    }

		tamanho++;  //tamanho global da fila
	}
}

/** 
  Insere um dado que já existia na fila. Ao remover um dado da fila, é chamada para inseri-lo novamente.
  Mantendo alguns atributos anteriores. Antes de remover o pacote da fila, é necessário salvar estes três atributos,
  e então passados como parâmetros.
    
  @param msg_t *nova - pacote a ser inserido na fila;
  @param int save - quantidade de tentativas;
  @param int saveID - identificador anterior;
  @param time_t back - timestamp anterior;
**/
void insere_fix(msg_t *nova, int save, int saveID, time_t back){
	if(!filas){
		printf("Impossivel inserir. Problema com a fila\n");
		return;
	} else if(tamanho >= MAXFILA){
		printf("Fila cheia. Roteador sobrecarregado\n");
		return;
	} else{
		filas[tamanho].mesg = nova;
		filas[tamanho].id = saveID;
		filas[tamanho].tentativas = save;
		filas[tamanho].timestamp = back;
		
		tamanho++;
	}
}


/** remove o primeiro da fila */
void remove_f(){
	int i;

	free(filas[0].mesg); //libera espaço
	
	filas[0].mesg = NULL; 
	
  for(i = 1; i < tamanho; i++){
		filas[i-1].mesg = filas[i].mesg;
	}

	tamanho--;
}

/**
  Remove um pacote intermediário. Pacotes que já foram confirmados tem prioridade na fila, assim para evitar que
  eles sejam reinviados é necessário removê-los da fila.
**/
void remove_fix(int i){
	int j;
  
	if(i == 0){ //caso for o primeiro, remove normalmente
		remove_f();		
	}	else if( i == tamanho - 1){ //se for o ultimo
		free(filas[i].mesg); 
		filas[i].mesg = NULL;
		tamanho--;
	} else{ //caso for algum intermediario
		for(j = i; j < tamanho-1; j++){
			free(filas[j].mesg);
			filas[j].mesg = filas[j+1].mesg;	
		}
		tamanho--;
	}
	return;
}

int main(int argc, char *arq[]){
  
  pthread_t tids[4];
  
  if(!arq[1]){printf("Problema com o id do roteador\n");
    return 0;
  }

  int routerId = atoi(arq[1]); //Lê o ID do roteador da linha de comando
  
  //Lê a tabela de roteamento com os caminhos mínimos já computados
  if(!(myConnect = leEnlaces(link_u, vertices =countIn(rout_u),routerId)))return 0;
  
  if(!(myTable = leEnlaces(link_u, vertices,routerId)))return 0;
    
  //Lê informações sobre este roteador ( id, porta, ip)   
  if(!(myRouter = leInfos(rout_u, routerId))) return 0;
  
  int i;
  
  for(i = 0; i < vertices; i++){
	  printf("%d %d %d  %d\n", myConnect->idVizinho[i],myConnect->custo[i],myConnect->idImediato[i], myConnect->enlace[i]);  
  }
 
  imprimeTabela();

  pthread_create(&tids[0], NULL, (void *)server, NULL); //server que recebe os pacotes
  pthread_create(&tids[1], NULL, (void *)SendDV, NULL);
  pthread_create(&tids[2], NULL, (void *)enviarMsg, NULL);
  pthread_create(&tids[3], NULL, (void *)serverControl, NULL); //controle da fila e encaminhamentos dos pacotes
  
  pthread_join(tids[0], NULL);
  pthread_cancel(tids[1]);
  pthread_cancel(tids[3]);
  pthread_join(tids[1], NULL);
  
  pthread_cancel(tids[3]);
  
  pthread_join(tids[3], NULL);
  
  return 0;
}
