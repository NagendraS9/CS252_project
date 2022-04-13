#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <fstream>
#include <bits/stdc++.h>
#include <dirent.h>
using namespace std;

#define LOOPBACK "127.0.0.1"

int input(ifstream &file, int &id, int &unique_id, int &my_port, int &no_neighbors, int &no_files, vector<string> *file_names,
    vector<vector<int>> *neighbors)
    {
    file >> id;
    // cout<<id<<" ";
    file >> my_port;
    // cout<<my_port<<" ";
    file >> unique_id;
    // cout<<unique_id<<"\n";
    file >> no_neighbors;
    // cout<<no_neighbors<<"\n";

    for (int i=0;i<no_neighbors;i++){
        int nid, nport;
        file >> nid >> nport;
        // cout<<nid<<" "<<nport<<"\n";
        vector<int> details(2);
        details[0] = nid;
        details[1] = nport;
        neighbors->push_back(details);
    }
    file >> no_files;
    // cout<<no_files<<"\n";
    for (int i=0;i<no_files;i++){
        string filename;
        file >> filename;
        // cout<<filename<<"\n";
        file_names->push_back(filename);
    }
    file.close();
    return 0;
}

void listFiles(const char* dirname,vector<string> *my_files){
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (dirname)) != NULL) {
    /* print all the files and directories within directory */
    while ((ent = readdir (dir)) != NULL) {
        string d_name = ent->d_name;
        if (d_name != "." && d_name != ".." && d_name!="Downloaded") { 
            my_files->push_back(ent->d_name);
            printf ("%s\n", ent->d_name);
        }
    }
    closedir (dir);
    } else {
    /* could not open directory */
        perror ("");
        return;
    }
}
// Return a listening socket
int get_listener_socket(const char* IP, const char* PORT)
{
    int listener;     // Listening socket descriptor
    int yes=1;        // For setsockopt() SO_REUSEADDR, below
    int rv;

    struct addrinfo hints, *ai, *p;

    // Get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(IP, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) { 
            continue;
        }
        
        // Lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // If we got here, it means we didn't get bound
    if (p == NULL) {
        return -1;
    }

    freeaddrinfo(ai); // All done with this

    // Listen
    if (listen(listener, 10) == -1) {
        return -1;
    }

    return listener;
}

// Add a new file descriptor to the set
void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
    // If we don't have room, add more space in the pfds array
    if (*fd_count == *fd_size) {
        *fd_size *= 2; // Double it

        *pfds = (pollfd*) realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

    (*fd_count)++;
}

// Remove an index from the set
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count-1];

    (*fd_count)--;
}
// Main
int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: (executable argument1-config-file argument2-directory-path)\n");
        return 1;
    }
    ifstream file;
    file.open(argv[1]);
    if (!file){
        cerr<<"Error: file couldn't be opened\n";
        return 1;
    }

    int id, unique_id, my_port, no_neighbors, no_files;
    vector<vector<int>> neighbors;
    
    vector<string> file_names;
    vector<string> my_files;
    // Take input
    input(file, id, unique_id, my_port, no_neighbors, no_files, &file_names, &neighbors);
    listFiles(argv[2],&my_files);
    // Connection vector
    vector<bool> connected(neighbors.size(), false);

    // Received answer from the neighbor ? vector<bool> is the bitmap for the files (0 if neighbor has it and vice-versa)
    map<int, pair<bool, vector<bool>>> receivedAns;
    for (int i=0;i<neighbors.size();i++){
        receivedAns[neighbors[i][0]] = {false, vector<bool>(no_files, false)};
    }

    // Mapping ids to unique id and socket descriptor
    map<int, pair<int, int>> mapfd;

    // List all the files in the given directory 
    // listFiles(argv[2]);
    int listener;     // Listening socket descriptor

    int newfd;        // Newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;
    int total_msg_sent=0;
    bool allSend=false;
    char buf[256];    // Buffer for client data

    char remoteIP[INET6_ADDRSTRLEN];

    // Start off with room for 5 connections
    // (We'll realloc as necessary)
    int fd_count = 0;
    int fd_size = 5;
    struct pollfd *pfds =(pollfd*) malloc(sizeof *pfds * fd_size);

    // Set up and get a listening socket
    listener = get_listener_socket(LOOPBACK, (to_string(my_port)).c_str());

    if (listener == -1) {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }
    
    // Add the listener to set
    pfds[0].fd = listener;

    pfds[0].events = POLLIN; // Report ready to read on incoming connection

    fd_count = 1; // For the listener

    // conDetails = true if we've printed the connection details 
    // allConnected = true if all the connections have been made
    bool allConnected = false, conDetails = false;
    int confirmations = 0;
    // Main loop
    for(;;) {
        int poll_count = poll(pfds, fd_count, 2500);

        if (poll_count == -1) {
            perror("poll");
            // cout<<"POLL\n";
            exit(1);
        }

        if (confirmations == no_neighbors && conDetails && allConnected){
            exit(1);
        }
        if (!allConnected){
            bool allSuccess = true;
            for (int i=0;i<neighbors.size();i++){
                // If not connected, try to connect       
                if (!connected[i]){   
                    int newfd;
                    struct addrinfo hints, *res;
                    int status;
                    // first, load up address structs with getaddrinfo():

                    memset(&hints, 0, sizeof hints);
                    hints.ai_family = AF_UNSPEC;
                    hints.ai_socktype = SOCK_STREAM;

                    if ((status = getaddrinfo(LOOPBACK, to_string(neighbors[i][1]).c_str(), &hints, &res)) != 0) {
                        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
                        return 2;
                    }
                    // make a socket:

                    newfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

                    if (connect(newfd, res->ai_addr, res->ai_addrlen) != -1){
                        add_to_pfds(&pfds, newfd, &fd_count, &fd_size);
                        connected[i] = true;
                        mapfd[neighbors[i][0]] = {0, newfd};
                    } else{
                        allSuccess = false;
                        // perror("connect");
                    }
                }
            }
            allConnected = allSuccess;
        } else if (!conDetails){
            // Check if all the neighbors have sent their unique ids
            vector<int> neighIDs;
            bool haveAllInfo = true;
            for (int i=0;i<neighbors.size();i++){
                if (neighbors[i].size() != 3){
                    haveAllInfo = false;
                    break;
                }
                neighIDs.push_back(neighbors[i][0]);
            }
            // Print the neighbor details
            if (haveAllInfo){
                sort(neighIDs.begin(), neighIDs.end());
                for (int i=0;i<neighIDs.size();i++){
                    for (int j=0;j<neighbors.size();j++){
                        if (neighbors[j][0] == neighIDs[i]){
                            cout<<"Connected to "<<neighIDs[i]<<" with unique-ID "<<neighbors[j][2]<<" on port "<<neighbors[j][1]<<"\n";
                        }
                    }
                }
                conDetails = true;

                bool receivedAll = true;
                for (auto it : receivedAns){
                    if (it.second.first == false){
                        receivedAll = false;
                        break;
                    }
                }
                conDetails = receivedAll;
                
                if (receivedAll){
                    map<string, pair<bool, set<int>>> ffound;
                    // cout<<"YES\n";
                    for (int i=0;i<no_files;i++){
                        bool found = false;
                        for (auto it : receivedAns){
                            if (it.second.second[i]){
                                found = true;
                                ffound[file_names[i]].first = true;
                                ffound[file_names[i]].second.insert(mapfd[it.first].first);
                            }
                        }
                        if (!found){
                            ffound[file_names[i]].first = false;
                        }
                    }
                    for (auto it : ffound){
                        if (it.second.first){
                            cout<<"Found "<<it.first<<" at "<<*(it.second.second.begin())<<" with MD5 0 at depth 1\n";
                        }
                        else{
                            cout<<"Found "<<it.first<<" at 0 with MD5 0 at depth 0\n";
                        }
                    }
                    conDetails = true;
                    for (auto it : mapfd){
                        string msg = "1";
                        if (send(it.second.second, msg.c_str(), msg.length(), 0) == -1){
                            perror("Error sending confirmation");
                        }
                    }
                }
            }
        }
        // Run through the existing connections looking for data to read
        for(int i = 0; i < fd_count; i++) {

            // Check if someone's ready to read
            if (pfds[i].revents & POLLIN) { // We got one!!

                if (pfds[i].fd == listener) {
                    // If listener is ready to read, handle new connection

                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        add_to_pfds(&pfds, newfd, &fd_count, &fd_size);
                        string msg = "0 ";
                        msg += to_string(id);
                        msg += " ";
                        msg += to_string(unique_id);
                        for(int i=0;i<my_files.size();i++){
                            msg += " ";
                            msg += my_files[i];
                        }
                        char* m = &msg[0];
                        if (send(newfd, m, strlen(m), 0) == -1){
                            perror("send");
                        }

                    }
                } 
                else {
                    // If not the listener, we're just a regular client
                    int nbytes = recv(pfds[i].fd, buf, sizeof buf, 0);
                    buf[nbytes] = '\0';
                    int sender_fd = pfds[i].fd;

                    if (nbytes <= 0) {
                        // Got error or connection closed by client
                        if (nbytes == 0) {
                            // Connection closed
                            // printf("pollserver: socket %d hung up\n", sender_fd);
                        } else {
                            //perror("recv");
                        }

                        close(pfds[i].fd); // Bye!

                        del_from_pfds(pfds, i, &fd_count);

                    } else {
                        // Split the received string with space as delimiter 
                        stringstream str(buf);
                        string segment;
                        vector<string> seglist;

                        while(getline(str, segment, ' '))
                        {
                            seglist.push_back(segment);
                        }
                        int nid, nunique, msg_type;
                        msg_type = stoi(seglist[0]);
                        if (msg_type == 0){
                            nid = stoi(seglist[1]);
                            nunique = stoi(seglist[2]);
                            for (int i=0;i<neighbors.size();i++){
                                // Push unique id of the neighbor
                                if (neighbors[i][0] == nid){
                                    neighbors[i].push_back(nunique);
                                    mapfd[nid].first = nunique;
                                    receivedAns[nid].first = true; //set I have recieved response from this id
                                }
                            }
                            //check the recieved file names are demanded by client?if yes add them
                            for(int i=3;i<seglist.size();i++){
                                for (int j=0;j<no_files;j++){
                                    if(file_names[j]==seglist[i]){
                                        receivedAns[nid].second[j] =true;
                                    }
                                }
                            }
                        }
                        else if (msg_type == 1){
                            confirmations ++;
                        }
                    }
                } 
            }
        } 
    }
    
    return 0;
}







// string exec(const char* cmd) {
//     array<char, 128> buffer;
//     string result;
//     unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
//     if (!pipe) {
//         throw runtime_error("popen() failed!");
//     }
//     while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
//         result += buffer.data();
//     }
//     return result;
// }


// //phase-3 addition
// void send_file(FILE *fp, int sockfd){
//   int n;
//   char data[SIZE] = {0};
 
//   while(fgets(data, SIZE, fp) != NULL) {
//     if (send(sockfd, data, sizeof(data), 0) == -1) {
//       perror("[-]Error in sending file.");
//       exit(1);
//     }
//     bzero(data, SIZE);
//   }
// }
