#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <fstream>
using namespace std;
#define MAXDATASIZE 100 // max number of bytes we can get at once

bool wake=true;
int count_recieved=0;
int neighours;
int listdir(const char *path)
{
    struct dirent *entry;
    DIR *dp;
    char *ptr = (char *)".";
    char *ptr1 = (char *)"..";

    dp = opendir(path);
    if (dp == NULL)
    {
        perror("opendir");
        return -1;
    }

    while ((entry = readdir(dp)))
    {
        if (strcmp(ptr, entry->d_name) && strcmp(ptr1, entry->d_name))
        {
            puts(entry->d_name);
        }
    }
    closedir(dp);
    return 0;
}


void send_data(int neighours,int* neighour_ports,int my_id,int my_port,int my_unique_id)
{
    char buffer[256];
    sprintf (buffer, "Connected to %d with unique-ID %d on port %d",my_id,my_unique_id,my_port);

    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return;
    }
    for (int i=0;i<neighours;i++){
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY; //INADDR_ANY always gives an IP of 0.0.0.0
        serv_addr.sin_port = htons(neighour_ports[i]);

        while (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            sleep(2);
        }
        send(sock, buffer, sizeof(buffer), 0);
        // printf("\nMessage sent\n");
        close(sock);
    }
}

void *recieve(void *server_fd){
    int ser_fd = *((int *)server_fd);
    while (1)
    {
        //recieve after gap of 2 sec
        sleep(2);

        //recieve data on our port

        struct sockaddr_in address;
        int valread;
        char buffer[256] ;
        int addrlen = sizeof(address);
        fd_set current_sockets, ready_sockets;

        //Initialize my current set
        FD_ZERO(&current_sockets);
        FD_SET(ser_fd, &current_sockets);
        int k = 0;
        while (1)
        {
            k++;
            ready_sockets = current_sockets;

            if (select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) < 0)
            {
                perror("Error");
                exit(EXIT_FAILURE);
            }

            for (int i = 0; i < FD_SETSIZE; i++)
            {
                if (FD_ISSET(i, &ready_sockets))
                {

                    if (i == ser_fd)
                    {
                        int client_socket;
                        if ((client_socket = accept(ser_fd, (struct sockaddr *)&address,(socklen_t *)&addrlen)) < 0)
                        {
                            perror("accept");
                            exit(EXIT_FAILURE);
                        }
                        FD_SET(client_socket, &current_sockets);
                    }
                    else
                    {
                        valread = recv(i, buffer, sizeof(buffer), 0);
                        count_recieved++;
                        if(count_recieved==neighours){wake =false;}
                        printf("%s\n", buffer);
                        FD_CLR(i, &current_sockets);
                    }
                }
            }

            if (k == (FD_SETSIZE * 2))
                break;
        }
    }
}
int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("example usage:./client-phaseX client1-config.txt files/client1/");
    }

    // read the client config file
    ifstream MyReadFile(argv[1]);
    int client_id, port, my_unique_id;
    MyReadFile >> client_id >> port >> my_unique_id;
    MyReadFile >> neighours;
    int neighours_id[neighours];
    int neighours_port[neighours];

    for (int i = 0; i < neighours; i++)
    {
        MyReadFile >> neighours_id[i] >> neighours_port[i];
    }
    int no_of_files;
    MyReadFile >> no_of_files;
    string files[no_of_files];
    for (int i = 0; i < no_of_files; i++)
    {
        MyReadFile >> files[i];
    }
    MyReadFile.close();
    // done

    // print files
    listdir(argv[2]);

    // read+listen
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int k = 0;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    // Forcefully attaching socket to the port

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    //Printed the server socket addr and port
    // printf("IP address is: %s\n", inet_ntoa(address.sin_addr));
    // printf("port is: %d\n", (int)ntohs(address.sin_port));

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    pthread_t tid;
    pthread_create(&tid, NULL, &recieve, &server_fd);
    send_data(neighours,neighours_port,client_id, port, my_unique_id);
    while(wake){
        sleep(1);
    }
}