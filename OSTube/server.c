//INCLUDES
#include <stdio.h>//Standard input-output
#include <stdlib.h>//Standard library
#include <unistd.h>//Standard symbolic constants and types

#include <string.h>
#include <time.h>

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>//Handle signals during program's execution

#include <dirent.h>//Format of directory entries
#include <sys/stat.h>//Information about files attributes

#include <sys/socket.h>//Internet protocol family
#include <arpa/inet.h>//Definitions for internet operations

#include <errno.h>

#include "server.h"

//DEFINITIONS
#define BUFFER_SIZE_INDEX 60000
#define BUFFER_SIZE_VIDEO 1048576
#define BUFFER_SIZE_IMAGES 10240
#define IPSERVER "192.168.1.3"
#define LOCALHOST "127.0.0.1"
#define PUERTO 8080

/*
INFORMACION IMPORTANTE

HTTP/0.9 Y 1.0 Cierran la conexion despues de un request
HTTP/1.1 Puede reutilizar la conexion mas de una vez para requests nuevos

*/


int main(int argc , char *argv[]) {
  //signal(SIGPIPE,SIG_IGN);//Ignore de broken pipe signal

  //Socket initialization
  int socket_desc , client_sock , c , *new_sock;
	struct sockaddr_in server , client;
  int opt = 1;
	//Create socket file descriptor
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1) {
		printf("ERROR: No se ha creado el socket\n");
	}

  // Forcefully attaching socket to the port 8080
	if (setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
												&opt, sizeof(opt)))
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

  server.sin_family = AF_INET;
	//server.sin_addr.s_addr = inet_addr(IPSERVER); //Ip del servidor
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(PUERTO); //Puerto del servidor

  //Bind the socket to the corresponding port
	if(bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0) {
		printf("ERROR: No se realizo el bind correspondiente\n");
		return 1;
	}
  if (listen(socket_desc, 3) < 0) {
    printf("ERROR: No se puede escuchar\n");
		return 1;
  }
  printf("El servidor ha iniciado correctamente en el puerto 8080\n");

  while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) ) {
    struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&client;
		struct in_addr ipAddr = pV4Addr->sin_addr;
		char *str_ip = malloc(INET_ADDRSTRLEN);
		inet_ntop( AF_INET, &ipAddr, str_ip, INET_ADDRSTRLEN );

    time_t tiempo_conexion;
		struct tm tiempo_inicio_conexion; //Tiempo de inicio del  servidor

		tiempo_conexion = time(NULL);
 		tiempo_inicio_conexion = *localtime(&tiempo_conexion);

 		char datos_fecha_conexion[200];
 		memset(datos_fecha_conexion,0,200);

 		sprintf(datos_fecha_conexion,"Hora de inicio: %d-%d-%d %d:%d:%d", tiempo_inicio_conexion.tm_mday, tiempo_inicio_conexion.tm_mon + 1, tiempo_inicio_conexion.tm_year + 1900, tiempo_inicio_conexion.tm_hour, tiempo_inicio_conexion.tm_min, tiempo_inicio_conexion.tm_sec);

		char peticion_realizada[200];
		memset(peticion_realizada,0,200);
		strcat(peticion_realizada,"Se conectó el usuario ");
		strcat(peticion_realizada,str_ip);
		strcat(peticion_realizada," - ");
		strcat(peticion_realizada,datos_fecha_conexion);

		printf("%s \n", peticion_realizada);

    pthread_t sniffer_thread;
		new_sock = malloc(1);
		*new_sock = client_sock;

		if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0) {

			printf("ERROR: No se pudo crear el hilo\n");

			return 1;
		}

  }
  return 0;
}

void *connection_handler (void *socket_desc){
  int sock = *(int*)socket_desc;
	char client_message[2000];

  int numRead = recv(sock , client_message , 2000 , 0);
    if (numRead < 1){
	    if (numRead == 0){
	    	printf("ERROR: El cliente se desconectó.\n");
	    } else {
	    	printf("ERROR: El cliente no está leyendo. errno: %d\n", errno);
	        //perror("The client was not read from");
	    }
	    close(sock);
      return NULL;
    }

  FILE *fileptr;
  FILE *videofileptr;//Se podria manejar con semaforos pero es para que cargue el html junto con el video

	char *buffer;
	long filelen;

	//Aqui se obtiene lo que se pide en el request
	int largo_copia = 0;
	for(int i = 4; i < numRead; i++) {
		if(client_message[i] == ' ') {
			break;
		} else {
			largo_copia++;
		}
	}
	char salida[2000];

	strncpy(salida,client_message, largo_copia+4);

	salida[largo_copia+4] = '\0';

  printf("%s \n", salida);

	if( !strncmp(salida, "GET /favicon.ico\n", 16) ) {
		printf("Petición get de /favicon.ico");
		fileptr = fopen("Web/Files_icons/favicon.ico", "rb");  // Open the file in binary mode
		char* fstr = "image/ico";
		sprintf(salida, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nConnection: close\r\n\r\n",fstr);
		send(sock, salida, strlen(salida), 0);


    while ((numRead = fread(salida, 1, 2000, fileptr)) > 0) {
        send(sock, salida, numRead, 0);
    }

    fclose(fileptr);
  }else if ( !strncmp(salida, "GET /\0", 6) ){
    char peticion_realizada[80];
		memset(peticion_realizada,0,80);
		strcat(peticion_realizada,"Peticion get de /index.html");

		printf("%s", peticion_realizada);

		//free(peticion_realizada);

		fileptr = fopen("Web/Files_HTML/watchVideo.html", "rb");
		char* fstr = "text/html";
		sprintf(salida, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nConnection: close\r\n\r\n",fstr);
		send(sock, salida, strlen(salida), 0);


    while ((numRead = fread(salida, 1, 2000, fileptr)) > 0){
        send(sock, salida, numRead, 0);
    }
    fclose(fileptr);
  } else if ( !strncmp(salida, "GET /Gallery/Files_Video/", 25) ){

    char peticion_realizada[100];
		memset(peticion_realizada,0,100);
		strcat(peticion_realizada,"Peticion get videos");

		printf("%s", peticion_realizada);

		//free(peticion_realizada);
    char *fileName= strtok(salida, " ");
    fileName = strtok(NULL, " ");

    printf("%s", fileName);

    videofileptr = fopen(fileName+1, "rb");
    char* fstr = "video/mp4";
		sprintf(salida, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nConnection: close\r\n\r\n",fstr);
		send(sock, salida, strlen(salida), 0);


    while ((numRead = fread(salida, 1, 2000, videofileptr)) > 0){
        send(sock, salida, numRead, 0);
    }
    fclose(videofileptr);

  }


  free(socket_desc);
	close(sock);

	printf("Se desconectó el cliente\n");

	return 0;
}
