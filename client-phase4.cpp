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
    int listener;     // Listening socket descriptor

    int newfd;        // Newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;
    int total_msg_sent=0;
    bool allSend=false;
    char buf[1024];    // Buffer for client data

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
    // haveNeighborFiles = true if we've file names of all the neighbors
    bool allConnected = false, conDetails = false, haveNeighborFiles = false;

    // Ask the neighbors for files of their neighbors
    bool askNeighbor = false;
    map<int, vector<string>> neighborFiles;

    // Has the neighbors answered to our search request?
    map<int, pair<bool, vector<int>>> neighborsAnswered;
    // maps to a boolean of reply status and int containing unique id where the file was found corresponding to the files of that index
    for (int i=0;i<neighbors.size();i++){
        neighborsAnswered[neighbors[i][0]] = {false, vector<int>(no_files, 0)};
    }

    // Maps each file to a boolean and the set of ids where it has been found
    map<string, pair<bool, set<int>>> ffound;
    // cout<<"Hi\n";
    // Depths of the files
    map<string, int> file_depths;
    for (int i=0;i<no_files;i++){
        file_depths[file_names[i]] = 0;
    }

    // The neighbors to which our reply is pending to their search request of searching our neighbors
    vector<pair<int, vector<string>>> toSend;
    int totalConfirmations = 0, totalNNConfirmations = 0, totalNN = 0;
    bool notSentNNConfirmation = true;
    bool printedConnectionInfo = false;
    bool haveAllInfo = false;

    // Main loop
    for(;;) {
        int poll_count = poll(pfds, fd_count, 2500);

        if (poll_count == -1) {
            perror("poll");
            // cout<<"POLL\n";
            exit(1);
        }

        if (totalConfirmations == no_neighbors && notSentNNConfirmation){
            string msg = "4 ";
            msg += to_string(id);
            for (auto it : mapfd){
                if (send(it.second.second, msg.c_str(), msg.length(), 0) == -1){
                    perror("NNConfirm send");
                }
            }
            notSentNNConfirmation = false;
        }

        if (totalConfirmations == no_neighbors && totalNNConfirmations == no_neighbors && haveAllInfo){
            // cout<<"HI\n";
            // for (auto it : mapfd){
            //     close(it.second.second);
            // }
            exit(1);
        }


        // Basically the part which tries to send reply to the neighbors if it has its neighbors files
        if (haveNeighborFiles){
            for (int i=0;i<toSend.size();i++){
                vector<string> files_to_search = toSend[i].second;
                string msg = "2 ";
                msg += to_string(id);
                // msg += " ";
                for (int j=0;j<files_to_search.size();j++){
                    set<int> whoHas;
                    // files_to_search.push_back(seglist[j]);
                    for (auto it : neighborFiles){
                        for (int k=0;k<it.second.size();k++){
                            if (files_to_search[j] == it.second[k]){
                                // Store unique id
                                whoHas.insert(mapfd[it.first].first);
                            }
                        }
                    }
                    if (!whoHas.empty()){
                        msg += " ";
                        msg += to_string(*(whoHas.begin()));
                    }
                    else{
                        // Nobody found so 0
                        msg += " 0";
                    }
                }
                if (send(mapfd[toSend[i].first].second, msg.c_str(), msg.length(), 0) == -1){
                    perror("NN search");
                }
            }
            // cout<<"Sent back\n";
            toSend.clear();
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
            vector<int> neighIDs;
            // Check if all the neighbors have sent their unique ids
            bool temp = true;
            for (int i=0;i<neighbors.size();i++){
                if (neighbors[i].size() != 3){
                    temp = false;
                    break;
                }
                neighIDs.push_back(neighbors[i][0]);
            }

            haveAllInfo = temp;
            // Print the neighbor details
            if (haveAllInfo){
                if (!printedConnectionInfo){   
                    sort(neighIDs.begin(), neighIDs.end());
                    for (int i=0;i<neighIDs.size();i++){
                        for (int j=0;j<neighbors.size();j++){
                            if (neighbors[j][0] == neighIDs[i]){
                                cout<<"Connected to "<<neighIDs[i]<<" with unique-ID "<<neighbors[j][2]<<" on port "<<neighbors[j][1]<<endl;
                                break;
                            }
                        }
                    }
                    printedConnectionInfo = true;
                }
                bool receivedAll = true;
                bool receivedNN = true;
                for (auto it : receivedAns){
                    if (!it.second.first){
                        receivedAll = false;
                        break;
                    }
                }
                
                // If all neighbors answered our search message
                for (auto it : neighborsAnswered){
                    if (!it.second.first){
                        receivedNN = false;
                        break;
                    }
                }

                // All the neighbors have answered so we have details about files of their neighbors;
                haveNeighborFiles = receivedAll;
                
                // Initially we check if our neighbors have the files
                if (!askNeighbor && receivedAll){
                    
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
                        // No neighbor has this file so ask if neighbors' neighbors have the files
                        if (!found){
                            ffound[file_names[i]].first = false;
                            // file_depths[file_names[i]] = 0;
                            
                            // Set askNeighbor to true
                            if (no_neighbors)
                                askNeighbor = true;
                        }
                        else{
                            // Set depth to 1 because we found the file at our neighbor
                            file_depths[file_names[i]] = 1;
                        }
                    }

                    if (askNeighbor){
                        string msg = "1 ";
                        msg += to_string(id);
                        for (int j = 0;j<no_files;j++){
                            msg += " ";
                            msg += file_names[j];
                        }
                        // cout<<"Send msg: "<<msg<<"\n";
                        for (auto it : mapfd){
                            // cout<<"Asking "<<it.first<<"\n";
                            if (send(it.second.second, msg.c_str(), msg.length(), 0) == -1){
                                perror("send");
                            }
                        }
                    }
                    // If we found all the files at our neighbors
                    if (!askNeighbor){
                        for (auto it : ffound){
                            cout<<"Found "<<it.first<<" at "<<(it.second.first ? *(ffound[it.first].second.begin()) : 0)<<" with MD5 0 at depth "<<(it.second.first ? 1 : 0)<<endl;
                        }
                        conDetails = true;
                        string msg = "3 ";
                        msg += to_string(id);
                        for (auto it : mapfd){
                            if (send(mapfd[it.first].second, msg.c_str(), msg.length(), 0) == -1){
                                perror("Line 422");
                            }
                        }
                    }

                }

                // We asked for files of neighbors' neighbors
                // Check if we received replies from all the neighbors
                else if (askNeighbor && receivedNN){
                    for (int i=0;i<no_files;i++){
                        
                        if (!ffound[file_names[i]].first){
                            for (auto it : neighborsAnswered){
                                if (it.second.second[i] != 0){
                                    ffound[file_names[i]].first = true;
                                    // neighbors reply with unique id of the client where they find the file else 0
                                    ffound[file_names[i]].second.insert(it.second.second[i]);
                                    file_depths[file_names[i]] = 2;
                                }
                            }
                        }
                    }

                    for (auto it : ffound){
                        if (it.second.first){
                            cout<<"Found "<<it.first<<" at "<<*(ffound[it.first].second.begin())<<" with MD5 0 at depth "<<file_depths[it.first]<<endl;
                        }
                        else{
                            cout<<"Found "<<it.first<<" at 0 with MD5 0 at depth 0"<<endl;
                        }
                    }

                    string msg = "3 ";
                    msg += to_string(id);
                    for (auto it : mapfd){
                        
                        if (send(mapfd[it.first].second, msg.c_str(), msg.length(), 0) == -1){
                            perror("Line 458");
                        }
                    }
                    conDetails = true;
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
                        for(int j=0;j<my_files.size();j++){
                            msg += " ";
                            msg += my_files[j];
                        }
                        
                        // cout<<"Line 465: "<<msg<<"\n";
                        if (send(newfd, msg.c_str(), msg.length(), 0) == -1){
                            perror("send");
                        }
                        total_msg_sent+=1;
                        // if(total_msg_sent==no_neighbors){
                        //     allSend=true;
                        //     if(allConnected && conDetails){exit(1);}
                        // }
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
                        // for (auto it : mapfd){
                        //     if (it.second.second == pfds[i].fd){
                        //         mapfd.erase(it.first);
                        //         break;
                        //     }
                        // }
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
                        // cout<<"Line 505: "<<buf<<"\n";
                        int nid, nunique, msg_type;
                        // cout<<"After this?\n";
                        msg_type = stoi(seglist[0]);
                        // cout<<"Here?\n";
                        nid = stoi(seglist[1]);
                        // cout<<"Or here\n";
                        // First message containing files of immediate neighbor
                        if (msg_type == 0) {    
                            // cout<<"Or this?\n";
                            // if (totalNN == -1) totalNN = 0;
                            // totalNN += stoi(seglist[2]);
                            nunique = stoi(seglist[2]);
                            for (int j=0;j<neighbors.size();j++){
                                // Push unique id of the neighbor
                                if (neighbors[j][0] == nid){
                                    neighbors[j].push_back(nunique);
                                    mapfd[nid].first = nunique;
                                    receivedAns[nid].first = true; //set I have recieved response from this id
                                }
                            }
                            //check the recieved file names are demanded by client?if yes add them
                            for(int j=3;j<seglist.size();j++){
                                neighborFiles[nid].push_back(seglist[j]);
                                for (int k=0;k<no_files;k++){
                                    if(file_names[k] == seglist[j]){
                                        receivedAns[nid].second[k] = true;
                                    }
                                }
                            }    
                        }

                        // Message sent by a sent to ask its neighbor if their neighbor have the files
                        if (msg_type == 1){
                            // Files that are being searched
                            vector<string> files_to_search;
                            // If we've received files names from our neighbors
                            // cout<<"Msg 1: "<<buf<<"\n";
                            if (haveNeighborFiles) {
                                // cout<<"I've files\n";
                                
                                // We send a reply which has 2 as msg_type and as sender id and then the unique ids of the clients where
                                // the neighbor found the file else 0
                                string msg = "2 ";
                                msg += to_string(id);
                                // msg += " ";
                                for (int j=2;j<seglist.size();j++){
                                    set<int> whoHas;
                                    files_to_search.push_back(seglist[j]);
                                    
                                    // Check if any neighbor has this file
                                    for (auto it : neighborFiles){
                                        for (int k=0;k<it.second.size();k++){
                                            if (seglist[j] == it.second[k]){
                                                // Store its unique id 
                                                whoHas.insert(mapfd[it.first].first);
                                            }
                                        }
                                    }

                                    if (!whoHas.empty()){
                                        msg += " ";
                                        // Take the smallest unique id to transmit
                                        msg += to_string(*(whoHas.begin()));
                                    }
                                    else{
                                        msg += " 0";
                                    }
                                }
                                if (send(pfds[i].fd, msg.c_str(), msg.length(), 0) == -1){
                                    perror("519: send");
                                }
                            }
                            // Don't have all the file names currently;
                            else{   
                                // cout<<"I don't have files\n";
                                for (int j=2;j<seglist.size();j++){
                                    files_to_search.push_back(seglist[j]);
                                }     
                                // Push this client and files to search      
                                toSend.push_back({nid, files_to_search});
                            }
                        }
                        // Reply we receive from the neighbor
                        else if (msg_type == 2){
                            // This neighbor has replied
                            // cout<<"Msg 2: "<<buf<<"\n";

                            neighborsAnswered[nid].first = true;
                            for (int j=2;j<seglist.size();j++){
                                // Unique id of client where it found the file
                                // cout<<"hmm "<<seglist[j]<<"\n";
                                neighborsAnswered[nid].second[j-2] = stoi(seglist[j]);
                            }
                        }
                        else if (msg_type == 3){
                            totalConfirmations ++;
                        }
                        else if (msg_type == 4){
                            totalNNConfirmations ++;
                        }
                    }
                } 
            }
        } 
    }
    
    return 0;
}