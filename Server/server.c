#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

int DescrittoreServer, DescrittoreClient;

sem_t* scrivere;


void sigint(int a){
	
	if(DescrittoreClient!=0) 
		close(DescrittoreClient);
	if(DescrittoreServer!=0) 
		close(DescrittoreServer);
	
	sem_close(scrivere);
	
	exit(1);	
}

void scriviFile(FILE*f,FILE*f2,char*filename){
	f=fopen(filename,"w");
	ftruncate(fileno(f),0);
	
	char * line = NULL;
    size_t len = 0;
    ssize_t read;
    char str1[20];
    char nome[20]; //nome sensore che ci interessa
    
    strncpy(nome,filename,strlen(filename)-4);
    
    
    while ((read = getline(&line, &len, f2)) != -1) {
        
        strncpy(str1,line,5);
        if(strncmp(str1,"tempo",5)==0){
			fprintf(f,"%s",line);}
		
		strncpy(str1,line,strlen(nome));
		if(strncmp(str1,nome,strlen(nome))==0){
			fprintf(f,"%s\n",line);}
	}
			
	fclose(f);
}

int main(int argc, char *argv[]){

    	if(argc != 2){
    		printf("Correct form: ./server <port number>\n");
    		exit(1);
    	}
    	
    	signal(SIGINT, sigint); 
    	
		//sem_t* scrivere;
		
    	//int DescrittoreServer, DescrittoreClient;
			
    	socklen_t LunghezzaClient;

    	int NumPorta = atoi(argv[1]);

    	struct sockaddr_in serv_addr, cli_addr; /* indirizzo del server e del client */

    	int rc, fd, bytes_sent;

    	off_t offset = 0;

    	struct stat stat_buf;

    	char filename[1024] = {};
    	
    	char azzera[1024];

    	size_t fsize;
		size_t refresh;
		size_t confronto=-1;
		
		FILE* f;
		FILE* f2;
		
    	DescrittoreServer = socket(AF_INET, SOCK_STREAM, 0);

    	if(DescrittoreServer < 0){
    		perror("Errore creazione socket\n");
    		exit(1);
    	}

    	bzero((char *) &serv_addr, sizeof(serv_addr)); /* bzero scrive dei null bytes dove specificato per la lunghezza specificata */
    	serv_addr.sin_family = AF_INET; /* la famiglia dei protocolli */
    	serv_addr.sin_port = htons(NumPorta); /* porta htons converte nell'ordine dei byte di rete */
    	serv_addr.sin_addr.s_addr = INADDR_ANY; /* dato che è un server bisogna associargli l'indirizzo della macchina su cui sta girando */
    	

    	/* int bind(int descrittore_socket, struct sockaddr* indirizzo, int lunghezza_record_indirizzo) */

    	if(bind(DescrittoreServer, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
    		perror("Errore di bind\n");
    		close(DescrittoreServer);
    		exit(1);
    	}

    	/* int listen (int descrittore_socket, int dimensione_coda) */

    	listen(DescrittoreServer, 3);

    	LunghezzaClient = sizeof(cli_addr);
		
		scrivere=sem_open("mysemaphore7",O_CREAT);
			if(scrivere==SEM_FAILED){
				fprintf(stderr,"errore nell'apertura del semaforo");
				exit(1);}
		
    	while(1){

    		/* int accept(int descrittore_socket, struct sockaddr* indirizzo, int* lunghezza_record_indirizzo) */
			fprintf(stderr,"In attesa di un client \n");
    		DescrittoreClient = accept(DescrittoreServer, (struct sockaddr *) &cli_addr, &LunghezzaClient);

    		if(DescrittoreClient < 0){
    			perror("Errore: non è possibile stabilire la connessione con il Client \n");
    			close(DescrittoreServer);
    			close(DescrittoreClient);
    			exit(1);

    		}
			
    		/* get the file name from the client */

        	rc = recv(DescrittoreClient, filename, sizeof(filename), 0);
			
        	if (rc == -1) {
          		fprintf(stderr, "recv failed: %s\n", strerror(errno));
          		exit(1);

        	}
        	
        	printf("Ricevuta richiesta di inviare il file: '%s'\n \n", filename);
			
			/* get the file name from the client */
			
			rc=recv(DescrittoreClient,&refresh,sizeof(refresh),0);
			printf("ricevuto refresh period %ld \n",refresh);
			
			sem_wait(scrivere); //entro in zona critica
			printf("entrato in zona critica\n");
			
			f2=fopen("dati.txt","r");
			
			scriviFile(f,f2,filename);
			
			/* open the file to be sent */
        	fd = open(filename, O_RDONLY);

       	 	if (fd == -1) {
        		fprintf(stderr, "Impossibile aprire '%s': %s \nIl client è stato disconnesso \n", filename, strerror(errno));
        		close(DescrittoreClient);
        		continue;
			}

        	/* get the size of the file to be sent */

        	fstat(fd, &stat_buf);

        	fsize = stat_buf.st_size;

        	bytes_sent = send(DescrittoreClient, &fsize, sizeof(fsize), 0);

        	if(bytes_sent == -1){

        		printf("Errore durante l'invio della grandezza del file \n");
        		close(DescrittoreClient);
    			close(fd);
    			close(DescrittoreServer);
        		exit(1);

        	}

        	/* copy file using sendfile */

        	offset = 0;

        	rc = sendfile(DescrittoreClient, fd, &offset, stat_buf.st_size);
			
			printf("Il file inviato ha dimensione= %d bytes \n",rc);
			
        	if (rc == -1) {
          		fprintf(stderr, "Errore durante l'invio di: '%s'\n", strerror(errno));
          		exit(1);

        	}

        	if (rc != stat_buf.st_size) {
          		fprintf(stderr, "Trasferimento incompleto: %d di %d bytes\n", rc, (int)stat_buf.st_size);
          		exit(1);
        	}

    		char *ip_address = inet_ntoa(cli_addr.sin_addr); /* inet_ntoa converte un hostname in un ip */

    		printf("Il file è stato correttamente inviato al IP----> %s\n\n", ip_address);		
			
			
			sem_post(scrivere);
			close(fd);
    		sleep(5.0001);
    		printf("mi preparo...");
			if(refresh==confronto){
				close(DescrittoreClient);
				printf("Client servito ! \n \n");
				fclose(f2);
				char command[50]="rm ";
				strcat(command,filename);
				system(command);
				strncpy(filename,azzera,sizeof(azzera));
				printf("Client servito ! \n \n");
				continue;}
			
			else if(refresh!=confronto){
				
				printf("inizio a inviare gli aggiornamenti... \n\n");
				
				while(1){
					sleep(refresh);
					sem_wait(scrivere);
					
					scriviFile(f,f2,filename);
						/* open the file to be sent */
					fd = open(filename, O_RDONLY);

					if (fd == -1) {
						fprintf(stderr, "Impossibile aprire '%s': %s \nIl client è stato disconnesso \n", filename, strerror(errno));
						close(DescrittoreClient);
						continue;
					}
					
					fstat(fd, &stat_buf);

					fsize = stat_buf.st_size;
					
					bytes_sent = send(DescrittoreClient, &fsize, sizeof(fsize), 0);

					if(bytes_sent == -1){

						printf("Errore durante l'invio della grandezza del file \n");
						close(DescrittoreClient);
						close(fd);
						close(DescrittoreServer);
						exit(1);

					}
					/* copy file using sendfile */

					offset = 0;

					rc = sendfile(DescrittoreClient, fd, &offset, stat_buf.st_size);
					
					printf("Il file inviato ha dimensione= %d bytes \n",rc);
					
					if (rc == -1) {
						fprintf(stderr, "Errore durante l'invio di: '%s'\n", strerror(errno));
						exit(1);

					}
					
					close(fd);
					sem_post(scrivere);
					sleep(0.0001);
					printf("aggiornamento inviato\n\n");
			}}
			
			fclose(f2);
    		char command[50]="rm ";
    		strcat(command,filename);
    		system(command);
    	}
	
    	close(DescrittoreServer);
    	sem_close(scrivere);
    	return EXIT_SUCCESS;
    }



   /* Come funziona ?

    1) client manda nome file al server

    2) server manda grandezza file al client

    3) server manda il file

    4) il client riceve il file */


    
