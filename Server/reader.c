#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <termios.h>
#include <stdlib.h>
#include "reader.h"
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>

// Ritorna il file descriptor della porta seriale e setta le impostazioni della comunicazione

int open_serial_port(const char * device, uint32_t baud_rate){
  
  int fd = open(device, O_RDWR | O_NOCTTY);
  if (fd == -1){
	perror("Impossibile collegarsi alla porta seriale");
    return -1;
  }
 
  //Flush dei byte letti o scritti precedentemente
  int result = tcflush(fd, TCIOFLUSH);
  if (result){
    perror("tcflush fail");  
  }
 
  // // Ottengo la configurazione corrente della porta seriale
  struct termios options;
  result = tcgetattr(fd, &options);
  if (result)
  {
    perror("tcgetattr failed");
    close(fd);
    return -1;
  }
 
  // Turn off delle opzioni che possono interferire con l'invio
  options.c_iflag &= ~(INLCR | IGNCR | ICRNL | IXON | IXOFF);
  options.c_oflag &= ~(ONLCR | OCRNL);
  options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
 
  // Set up timeouts
  options.c_cc[VTIME] = 1;
  options.c_cc[VMIN] = 0;
 
  //Setto il Baud_rate

  cfsetospeed(&options, B115200);
 
  cfsetispeed(&options, cfgetospeed(&options));
 
  result = tcsetattr(fd, TCSANOW, &options);
  
  if (result){
    perror("tcsetattr fail");
    close(fd);
    return -1;
  }
 
  return fd;
}
 
// Legge i dati dalla porta seriale

ssize_t read_port(int fd,char* file, uint8_t * buffer, size_t size){

  time_t now;
  struct tm *tm;
  
  FILE* f;
  
  /* SEMAFORO*/
  sem_t* scrivere;
  sem_unlink("mysemaphore7");
  scrivere=sem_open("mysemaphore7",O_CREAT,0644,1);
  if(scrivere==SEM_FAILED){
	  fprintf(stderr,"errore nell'apertura del semaforo");
	  exit(1);}
	  
  size_t received = 0;
  
  int pausa;
  printf("Ogni quanti secondi acquisisco i dati?");
  scanf("%d",&pausa);
  printf("tempo di acquisizione= %d secondi \n\n",pausa);
  
  char c=pausa;
  
  write(fd,&c,sizeof(char));
  
		
  while (1){
	
	sem_wait(scrivere);
	
    f=fopen(file,"a");
  
	if(f==NULL) {
		perror("Errore in apertura del file");
		exit(1);}
	
	
    ssize_t r1 = read(fd, &buffer[0], 1);
    
    if (r1 < 0){	
      perror("failed to read from port");
      return -1;}
	
    else if(buffer[0]=='['){
		
		/**********SCRIVO TIMESTAMP SUL FILE**********/
		now = time(0);
		
		if ((tm = localtime (&now)) == NULL) {
			printf ("Error extracting time stuff\n");
			return 1;
		}
		
		fprintf(f,"tempo %02d.%02d\n\n",tm->tm_hour, tm->tm_min);
		
		/**********LEGGO I DATI PROVENIENTI DA ARDUINO**********/
		
		while(buffer[0]!=']'){
			
			r1=read(fd,&buffer[0],1);
			
			if(r1<0){
				perror("errore");
				return -1;}
			
			if(r1==0){
				continue;}
			
			if(buffer[0]==']'){
				fprintf(f,"%s","\n");
				break;}
			
			else if(buffer[0]!='['){
				fprintf(f,"%c",buffer[0]);}
		}
		printf("acquisite informazioni \n");
	}
	
	fclose(f);
	sem_post(scrivere);
	sleep(0.0001);
	
	}
	sem_close(scrivere);
	fclose(f);
	return received;
}


int set(){
 
  const char * device = "/dev/ttyACM0";
  
  //Definisco Baud_rate (=bits per second)
  
  uint32_t baud_rate = 115200;
 
  int fd = open_serial_port(device, baud_rate);
 
  
  if (fd < 0) {return 1;}
  
  printf("\naperta porta seriale \n");
  return fd;
 
}

int main(){
	int serial=set();  
	      	
	if(serial!=1){
		printf("connesso alla seriale: %d \n \n",serial);}
	
	else{
		printf("Non è stato possibile connettersi alla seriale \n \n");
		exit(1);}
		
	system("rm -f dati.txt");	//elimino il file vecchio se esiste
	
	uint8_t buffer[1024];
	
	sleep(2.5);
	
	read_port(serial,"dati.txt",buffer,sizeof(buffer));
	
	return 0;
	
}
