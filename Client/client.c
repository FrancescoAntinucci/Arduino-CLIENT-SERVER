#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <signal.h>

int DescrittoreClient, fd; /* descrittore del socket e del file */

void grafico(char*filename){
	
	char command[50];
	strcpy(command, "python grafica.py " );
	strcat(command,filename);
	strcat(command," &");
	system(command);
}

void sigint(int a){
	if(fd!=0) 
		close(fd);
	if(DescrittoreClient!=0) 
		close(DescrittoreClient);
	
	exit(1);	
}
	
int main(int argc, char *argv[]){
	/* Controllo 1*/
	if(argc != 4)   {
		printf("How to use---> ./client <hostname> <numero porta> <nomefile> \n");
		exit(1);
	}
	
	struct hostent *hp=gethostbyname(argv[1]); /* con la struttura hostent definisco l'hostname del server */
	int NumPorta = atoi(argv[2]); /* numero di porta */
	char *filename = argv[3]; /*nome file*/
	
	struct sockaddr_in serv_addr; /* indirizzo del server */
	char nread=0, Buffer[100024] = {}; /* contiene i dati di invio e ricezione */
	
	/*controllo 2*/
	if(!(strncmp(filename,"Profondità.txt",strlen(filename))==0 || strncmp(filename,"Umidità.txt",strlen(filename))==0 || strncmp(filename,"Distanza.txt",strlen(filename))==0 ||strncmp(filename,"Temperatura.txt",strlen(filename))==0) ){
		printf(" \n Errore nella scelta del file,i file richiedibili sono solo: \n\n 1) Umidità.txt \n 2) Temperatura.txt \n 3) Distanza.txt \n 4) Profondità.txt \n \n");
		exit(1);}
		
	signal(SIGINT, sigint); //gestione segnali
	
	size_t sec;
	printf("Ogni quanti secondi desideri ricevere aggiornamenti?(-1 se non li vuoi): ");
	scanf("%ld",&sec);
	printf("refrash= %ld secondi\n",sec);
	size_t fsize;
	int tmp=0, bytes_read=0;
	
	
	bzero((char *) &serv_addr, sizeof(serv_addr)); /* bzero scrive dei null bytes dove specificato per la lunghezza specificata */
	serv_addr.sin_family = AF_INET; /* la famiglia dei protocolli */
	serv_addr.sin_port = htons(NumPorta); /* la porta */
	serv_addr.sin_addr.s_addr = ((struct in_addr*)(hp->h_addr)) -> s_addr; /* memorizzo il tutto nella struttura serv_addr */
	
	DescrittoreClient = socket(AF_INET, SOCK_STREAM, 0);
	if(DescrittoreClient < 0){
		perror("Errore nella creazione della socket");
		exit(1);
	}
	connect(DescrittoreClient, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
	if(connect < 0){
		perror("Errore nella connessione");
		close(DescrittoreClient);
		exit(1);
	}
 
	strcpy(Buffer, filename);
	
	send(DescrittoreClient, Buffer, strlen(Buffer), 0); //client invia il nome al server
	printf("inviata richiesta file \n");
	
	sleep(1);
	
	send(DescrittoreClient, &sec, sizeof(sec), 0); //invio ogni quanto desidero avere i refresh
	printf("inviato periodo di refresh \n");
	 
	bytes_read = read(DescrittoreClient, &fsize, sizeof(fsize));
	
	
	if(bytes_read == -1){
		printf("Errore durante ricezione grandezza file\n");
		close(DescrittoreClient);
		exit(1);
	}
	
	if(bytes_read==0){
		printf("Il Server non ha inviato il file \n");
		close(DescrittoreClient);
		exit(1);
	}
	
	fd = open(filename,O_CREAT|O_WRONLY|O_TRUNC,0644); //open file
	if (fd  < 0) {
		perror("open");
		exit(1);
	}
	
	
	/**********LEGGO I DATI PROVENIENTI DAL SERVER**********/
	while(tmp!= fsize){
		
		nread = read(DescrittoreClient, Buffer, 1);
		
		if(nread<0){exit(1);}
		
		if(nread>0){
			write(fd,Buffer,nread);
			tmp+=nread;
			}
	}
	
	printf("Il file ha dimensione: %d  bytes \n", tmp);

	printf("File ricevuto\n\n");
	
	
	
	/**********SE NON VOGLIO GLI AGGIORNAMENTI**********/
	if(sec==-1){	
		close(fd);
		close(DescrittoreClient);
		grafico(filename);
	}
	
	/**********SE VOGLIO GLI AGGIORNAMENTI**********/
	else if(sec!=-1){
		
		grafico(filename);
		while(1){
			tmp=0;
			fsize=0;
			memset(Buffer, 0, sizeof(Buffer));
			
			bytes_read=read(DescrittoreClient, &fsize, sizeof(fsize));
			if(bytes_read == -1){
				printf("Errore durante ricezione grandezza file\n");
				close(DescrittoreClient);
				exit(1);
			}
			if(bytes_read==0){
				printf("Il Server non ha inviato l'aggiornamento, chiudo la comunicazione \n");
				close(DescrittoreClient);
				exit(1);
			}
			
			
			//fd= open(filename,O_WRONLY|O_APPEND,0644); 
			
			/**********LEGGO I DATI PROVENIENTI DAL SERVER**********/
			while(tmp!= fsize){
					nread = read(DescrittoreClient, Buffer+tmp, 1);
					if(nread<0){exit(1);}
					if(nread>0){
						tmp+=nread;
					}
				}
				
			printf("L'aggiornamento ha dimensione : %d bytes \n",tmp);
			write(fd,Buffer,tmp);
			
			printf("Aggiornamento ricevuto\n\n");
			close(fd);
		}
		close(fd);
		close(DescrittoreClient);
	}
		
	return EXIT_SUCCESS;
}
