
#include <netdb.h>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <map>
#include <set>
#include <regex>
#include <limits.h>
#include <stdlib.h>
#include <arpa/inet.h>

/**
 * The vector that will contain the connected fd of the clients.
 */
std::vector<int> connected_fds;

/**
 * A map from a clients name to his fd.
 */
std::map<std::string, int> name_to_fd;

/**
 * A map from a clients fd to his name.
 */
std::map<int, std::string> fd_to_name;

/**
 * A map from a groups name to a set of his members fds.
 */
std::map<std::string, std::set<int>> group_to_clients;

/**
 * A regex that represents a legal name.
 */
std::regex name_format("[a-zA-Z0-9]+");


/**
 * A helper function that checks if a name is legal.
 * @param name The name to check.
 * @return True if the name is legal, False otherwise.
 */
bool legal_name(std::string name)
{
    return ((name_to_fd.find(name) == name_to_fd.end()) &&
            (group_to_clients.find(name) == group_to_clients.end()) &&
            std::regex_match(name, name_format));
}

void client_exit_request(int fd, bool flag);

/**
 * This functions a wrapper for the write sys call that adds to the beginning of a message its
 * length for are protocol and checks the write success.
 * @param fd The fd to write to.
 * @param message The message to send to the client with the given fd.
 * @return on failure -1, if the client is disconnected 0 will be returned and on success the
 *         amount written.
 */
int write_wrapper(int fd, std::string message)
{
    std::string length = std::to_string(message.size());
    while (length.size() != 4)
    {
        length.insert(0,"0");
    }
    message = length + message;
    ssize_t amount = write(fd, message.c_str(), message.size());
    if (amount == 0)
    {
        return 1;
    }
    if (amount == -1)
    {
        std::cerr<<"ERROR: write "<<errno<<"."<<std::endl;
        client_exit_request(fd,false);
    }
    message = message.substr((unsigned long)amount);
    while ((amount = write(fd, message.c_str(), message.size()))>0)
    {
        message = message.substr((unsigned long)amount);
    }
    if (amount == -1)
    {
        std::cerr<<"ERROR: write "<<errno<<"."<<std::endl;
        client_exit_request(fd,false);
    }
    return (int)amount;
}

/**
 * The function to handel a client that requested to exit.
 * @param fd The clients fd.
 * @param flag A flag that represents if to send a message to the client.
 *             This is because we can disconnect from a client if he is no longer connected and
 *             we dont want to write to him in this case.
 */
void client_exit_request(int fd, bool flag)
{
    std::string name = fd_to_name[fd];
    name_to_fd.erase(fd_to_name[fd]);
    fd_to_name.erase(fd);
    for(auto map_it = group_to_clients.begin(); map_it != group_to_clients.end(); ++map_it){
        for(auto set_it = (*map_it).second.begin(); set_it != (*map_it).second.end();){
            if((*set_it) == fd){
                set_it = map_it->second.erase(set_it);
            } else {
                ++set_it;
            }
        }
    }
    if (flag)
    {
        std::string exit_message("Unregistered successfully.");
        std::cout<<name<<": Unregistered successfully."<<std::endl;
        write_wrapper(fd, exit_message);
    }
    connected_fds.erase(std::remove(connected_fds.begin(), connected_fds.end(), fd),
                        connected_fds.end());
    close(fd);
}

/**
 * In are protocol we send the message length as the first 4 bytes so this function reads the
 * beginning ant returns the messages length.
 * @param fd The fd to read from.
 * @return The message length.
 */
size_t get_message_length(int fd)
{
    char buf[4];
    char *temp = buf;
    ssize_t amount;
    size_t count = 4;
    while((amount = read(fd,temp, count)) > 0)
    {
        temp += amount;
        count -= amount;
    }
    if (amount == -1)
    {
        std::cerr<<"ERROR: read "<<errno<<"."<<std::endl;
        client_exit_request(fd,false);
    }
    if (buf == temp)
    {
        return 0;
    }
    return atoi(buf);
}


/**
 * The function that handles a create_group request.
 * @param fd The fd of the client that opened the group.
 * @param group_name The group name to create.
 * @param clients_names The names of the clients that should be members in the group.
 */
void create_group(int fd, std::string group_name, std::deque<std::string> clients_names)
{
    std::string message;
    message.clear();
    if (legal_name(group_name))
    {
        std::set<int> set;
        set.clear();
        set.insert(fd);
        while (clients_names.size() != 0)
        {
            if (name_to_fd.find(clients_names.front()) != name_to_fd.end())
            {
                //FOUND
                set.insert(name_to_fd[clients_names.front()]);
                clients_names.pop_front();
            }
            else
            {
                //NOT FOUND
                set.clear();
                break;
            }
        }
        if (set.size() < 2)
        {
            message += "ERROR: failed to create group \""+group_name+"\".";
            std::cerr<<fd_to_name[fd]<<": ERORR: failed to create group \""<<group_name<<"\"."<<std::endl;
        }
        if (message.size() == 0)
        {
            group_to_clients.insert(std::pair<std::string, std::set<int>>(group_name, set));
            message += "Group \""+group_name+"\" was created successfully.";
            std::cout<<fd_to_name[fd]<<": Group \""<<group_name<<"\" was created successfully."<<std::endl;
        }
    }
    else
    {
        message += "ERROR: failed to create group \""+group_name+"\".";
        std::cerr<<fd_to_name[fd]<<": ERORR: failed to create group \""<<group_name<<"\"."<<std::endl;
    }
    write_wrapper(fd, message);
}

/**
 * This function handles a create_client request.
 * @param fd The fd of the client to create.
 * @param name The name of the client to create.
 */
void create_client(int fd, std::string name)
{
    std::string message;
    message.clear();
    if (legal_name(name))
    {
        fd_to_name.insert(std::pair<int, std::string>(fd, name));
        name_to_fd.insert(std::pair<std::string, int>(name, fd));
        message += "0";
        std::cout<<name<<" connected."<<std::endl;
    }
    else
    {
        message += "1";
    }
    write_wrapper(fd, message);
}

/**
 * The function that handles a who request.
 * @param fd The fd of the client who requested the qho request.
 */
void who_request(int fd)
{
    std::vector<std::string> clients;
    clients.clear();
    for (std::pair<std::string, int> client_info : name_to_fd)
    {
        clients.push_back(client_info.first);
    }
    std::sort(clients.begin(), clients.end());
    std::string message;
    message.clear();
    for (int i=0; i<(int)clients.size(); ++i)
    {
        message.append(clients[i]);
        message.append(",");
    }
    message.pop_back();
    std::cout<<fd_to_name[fd]<<": Requests the currently connected client names."<<std::endl;
    write_wrapper(fd, message);
}

/**
 * This function handles a send a message to a single client.
 * @param sender_fd The senders fd.
 * @param receiver_fd The receivers fd.
 * @param message The message to send.
 * @param sender_message_flag A flag representing if to send the success status of the message to
 *                            the sender.
 * @return 0 on success, -1 otherwise.
 */
int send_message_request(int sender_fd, int receiver_fd, std::string message,
                         bool sender_message_flag)
{
    int return_value;
    std::string message_to_user;
    message_to_user.clear();
    std::string receiver_message;
    receiver_message.clear();
    receiver_message += fd_to_name[sender_fd];
    receiver_message += ": ";
    receiver_message += message;
    switch (write_wrapper(receiver_fd, receiver_message)){
        case 1://CLIENT NOT CONNECTED
            return_value = -1;
            message_to_user += "ERROR: failed to send.";
            break;
        default://SEND SUCCESSES
            return_value = 0;
            message_to_user += "Sent successfully.";
            break;
    }
    if (sender_message_flag)
    {
        if (message_to_user == "ERROR: failed to send.")
        {
            std::cerr<< fd_to_name[sender_fd]<<": ERROR: failed to send \""<<message<<"\" to "
                    ""<<fd_to_name[receiver_fd]<<"."<<std::endl;
        }
        else
        {
            std::cout<<fd_to_name[sender_fd]<<": \""<< message<<"\" was sent successfully "
                    "to "<<fd_to_name[receiver_fd]<<"."<<std::endl;
        }
        write_wrapper(sender_fd,message_to_user);
    }
    return return_value;
}

/**
 * This function handels a request to send a message to a group.
 * @param sender_fd The senders fd.
 * @param group_name The groups name.
 * @param receivers_fds A set of the receivers fds.
 * @param message The message to send.
 */
void send_group_message_request(int sender_fd,std::string group_name, std::set<int> receivers_fds,
                                std::string message)
{
    std::string message_to_user;
    message_to_user.clear();
    for (auto receiver_fd : receivers_fds)
    {
        if (sender_fd != receiver_fd)
        {
            if (send_message_request(sender_fd, receiver_fd, message, false) == -1)
            {
                message_to_user += "ERROR: failed to send.";
                std::cerr<< fd_to_name[sender_fd]<<": ERROR: failed to send \""<<message<<"\" to "
                        ""<<group_name<<"."<<std::endl;
                break;
            }
        }
    }
    if (message_to_user.size() == 0)
    {
        message_to_user += "Sent successfully.";
        std::cout<<fd_to_name[sender_fd]<<": \""<<message<<"\" was sent successfully to "<<group_name<<"."<<std::endl;
    }
    write_wrapper(sender_fd, message_to_user);
}

/**
 * This function handles a request from the servers admin to EXIT.
 */
void server_shutdown(int welcome_socket)
{
    std::cout << "EXIT command is typed: server is shutting down" << std::endl;
    for (int fd: connected_fds)
    {
        write_wrapper(fd, std::string("server_exit"));
    }
    close(welcome_socket);
    exit(0);
}

/**
 * This function handles the booting of the server.
 * @param port_num The welcome sockets port number.
 * @return The fd of the welcome socket.
 */
int server_boot(uint16_t port_num)
{
    char hostname[HOST_NAME_MAX];
    int s;
    struct hostent *hp;
    struct sockaddr_in my_addr;

    gethostname(hostname, HOST_NAME_MAX);
    if ((hp = gethostbyname(hostname)) == NULL)
    {
        std::cerr<<"ERROR: gethostbyname "<<errno<<"."<<std::endl;
        exit(1);
    }

    memset(&my_addr, 0, sizeof(struct sockaddr_in));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = INADDR_ANY;
    my_addr.sin_port = htons(port_num);

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        std::cerr<<"ERROR: socket "<<errno<<"."<<std::endl;
        exit(1);
    }

    if (bind(s, (struct sockaddr *) &my_addr, sizeof(struct sockaddr_in)) < 0)
    {
        std::cerr<<"ERROR: bind "<<errno<<"."<<std::endl;
        close(s);
        exit(1);
    }

    if (listen(s, 10) < 0)
    {
        std::cerr<<"ERROR: listen "<<errno<<"."<<std::endl;
        close(s);
        exit(1);
    }

    return s;
}

/**
 * This function handles the parsing of a message and splits it by delimiter.
 * @param message The whole message.
 * @param delimiter The delimiter to split by.
 * @return A deque of strings of the message after spliting.
 */
std::deque<std::string> split(std::string message, std::string delimiter)
{
    size_t pos = 0;
    std::deque<std::string> deque;
    deque.clear();
    std::string token;
    while ((pos = message.find(delimiter)) != std::string::npos)
    {
        token = message.substr(0, pos);
        message.erase(0, pos + delimiter.length());
        deque.push_back(token);
    }
    deque.push_back(message);
    return deque;
}

/**
 * The main function that boots the program and the loop running as long as the server is up
 * listening
 */
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "USAGE: whatsappServer portNum" << std::endl;
        exit(1);
    }
    name_to_fd.clear();
    fd_to_name.clear();
    group_to_clients.clear();
    connected_fds.clear();

    int s = server_boot((uint16_t) atoi(argv[1]));

    int t;

    fd_set read_fds;

    while (true)
    {
        FD_ZERO(&read_fds);
        FD_SET(s, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        for (int fd: connected_fds)
        {
            FD_SET(fd, &read_fds);
        }
        int max = 0;
        if (connected_fds.size() != 0)
        {
            max = *(std::max_element(connected_fds.begin(),connected_fds.end()));
        }
        max = std::max(std::max(max,s),STDIN_FILENO);
        if (select(max + 1, &read_fds, NULL, NULL, NULL) < 0)
        {
            std::cerr<<"ERROR: select "<<errno<<"."<<std::endl;
            exit(1);
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds))
        {
            std::string message;
            message.clear();
            std::getline(std::cin,message);
            if (message == "EXIT")
            {
                server_shutdown(s);
            }
            else
            {
                std::cerr<<"ERROR: invalid input."<<std::endl;
            }
        }

        if (FD_ISSET(s, &read_fds))
        {
            if ((t = accept(s, NULL, NULL)) < 0)
            {
                std::cerr<<"ERROR: accept "<<errno<<"."<<std::endl;
                exit(1);
            }
            connected_fds.push_back(t);
        }

        for (int fd : connected_fds)
        {
            if (FD_ISSET(fd, &read_fds))
            {
                size_t message_length = get_message_length(fd);
                if (message_length == 0)
                {
                    client_exit_request(fd, false);
                    continue;
                }
                size_t current_message_length = message_length;
                char buf[current_message_length];
                char *temp = buf;
                ssize_t amount = 0;
                while ((amount = read(fd,temp,current_message_length))>0)
                {
                    current_message_length -= amount;
                    temp += amount;
                }
                if (amount == -1)
                {
                    std::cerr<<"ERROR: read "<<errno<<"."<<std::endl;
                    client_exit_request(fd, false);
                }
                std::deque<std::string> message = split(std::string(buf,message_length), " ");
                if (message.front() == "create_client")
                {
                    message.pop_front();
                    create_client(fd,message.front());
                }
                else
                {
                    if (fd_to_name.find(fd) == fd_to_name.end())
                    {
                        std::string message_to_user("2");
                        write_wrapper(fd, message_to_user);
                    }
                    else if (message.front() == "create_group")
                    {
                        message.pop_front();
                        std::string group_name = message.front();
                        message.pop_front();
                        create_group(fd, group_name, split(message.front(), ","));
                    }
                    else if (message.front() == "who")
                    {
                        who_request(fd);
                    }
                    else if (message.front() == "exit")
                    {
                        client_exit_request(fd, true);
                    }
                    else if (message.front() == "send")
                    {
                        message.pop_front();
                        std::string receiver_name = message.front();
                        message.pop_front();
                        std::string the_message("");
                        while (!message.empty())
                        {
                            the_message += message.front();
                            the_message += " ";
                            message.pop_front();
                        }
                        the_message.pop_back();
                        if (name_to_fd.find(receiver_name) != name_to_fd.end())
                        {
                            send_message_request(fd,name_to_fd[receiver_name],
                                                 the_message,true);
                        }
                        else if (group_to_clients.find(receiver_name) !=
                                group_to_clients.end() && group_to_clients[receiver_name].find
                                (fd) != group_to_clients[receiver_name].end())
                        {
                            send_group_message_request(fd,receiver_name,
                                                       group_to_clients[receiver_name], the_message);
                        }
                        else
                        {
                            std::string message_to_user("ERROR: failed to send.");
                            std::cerr<< fd_to_name[fd]<<": ERROR: failed to send "
                                    "\""<<the_message<<"\" to "<<receiver_name<<"."<<std::endl;
                            write_wrapper(fd, message_to_user);
                        }
                    }
                }
            }
        }
    }
}