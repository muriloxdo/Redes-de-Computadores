# Trabalho da disciplina de Redes de Computadores da Universidade Federal da Fronteira Sul (UFFS) # 

## Alunos: Murilo Xavier de Oliveira e Rodrigo Vogt ##

## Atenção ##
Os arquivos "enlaces.config" e "roteador.config" precisam estar no
mesmo diretório dos códigos-fonte.

## Compilando ##
Para compilação foi usado:

```bash
 - gcc funcs.h readFiles.c router.c -D_REENTRANT -lpthread -o m -Wall
 ```
 
Também pode ser usado o Makefile usando o comando 

```bash
make
```

## Instanciando um roteador ##
Para instanciar um roteador é preciso passar como argumento na linha de
comandos o seu ID. Exemplo : 

```bash
./m 1
```

Para instanciar um roteador quando é utilizado o comando make é preciso 
passar como argumento na linha de comandos o seu ID. Exemplo : 

```bash
./main 1
```

## OBSERVAÇÃO ##
*Como enviar mensagem? Digite um Id de um roteador. Exemplo: 

```bash
2
```

*Após isso ele irá imprimir "msg:" e então basta digitar a mensagem. Exemplo:

```bash
msg: Exemplo de mensagem
```

Apenas o roteador 1 está enviando mensagem, isso é um problema temporário.

Há um problema visual após um roteador encaminhar um pacote:
O prompt fica esperando um id do scanf, sem a apresentação do
printf anterior na thread de envioMsg. Contudo ao digitar um id válido,
a thread segue sendo executada normalmente.
