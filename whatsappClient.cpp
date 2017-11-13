#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <regex>
#include <set>
#include <stdlib.h>


std::regex name_format("[a-zA-Z0-9]+");
std::regex names_format("[a-zA-Z0-9,]+");
std::regex starts_comma(",.*");
std::regex ends_comma(".*,");
std::regex double_comma (",,");

/**
 * A helper function that checks if a name is legal.
 * @param name. The given name that needs to be checked.
 * @return True if the name is legal, False otherwise.
 */
bool legal_name(std::string name)
{
    return (std::regex_match(name, name_format));
}

/**
 * A function that does all the actions we would like to do once there is a problem.
 * @param fd - the file descripter that needs to be closed
 * @param message - the message that needs to be printed
 * @param loc - where to print the error (cout or cerr)
 * @param error_num - the current errno
 * @param exit_num - determines is do exit with 1 or 0
 */
void problem( int fd, std::string message, bool loc, int error_num, int exit_num)
{
    if(loc)
    {
        std::cout << message << std::endl;
    }
    else
    {
        std::cerr << message << " " << error_num << std::endl;
    }
    close(fd);
    exit(exit_num);

}

/**
 * The function in charge of checking if a message is valid or not, and prints
 * the error if there is any.
 * @param message - the message that needs to be checked
 * @return true if the message is valid and false otherwise
 */
bool check_message(std::string message)
{
    std::string space = " ";
    size_t pos = 0;
    std::string word;
    pos = message.find(space);
    word = message.substr(0,pos);
    if(word == "who")
    {
        if(pos == std::string::npos)
        {
            return true;
        }
        else
        {
            std::cerr << "ERROR: failed to receive list of connected clients." << std::endl;
            return false;
        }
    }
    else if(word == "exit")
    {
        if(pos == std::string::npos)
        {
            return true;
        }
    }
    else if(word == "create_group")
    {
        message.erase(0, pos + 1);
        pos = message.find(space);
        word = message.substr(0, pos);
        if(!legal_name(word))
        {
            std::cerr << "ERROR: failed to create group \"" << word << "\""<< std::endl;
            return false;
        }
        message.erase(0, pos + 1);
        if(std::regex_match(message, names_format) &&
                !std::regex_match(message, double_comma) &&
                !std::regex_match(message, starts_comma) &&
                !std::regex_match(message, ends_comma) )
        {
            return true;
        }
        else
        {
            std::cerr << "ERROR: failed to create group \"" << word << "\""<< std::endl;
            return false;
        }
    }
    if(word == "send")
    {
        message.erase(0, pos + 1);
        pos = message.find(space);
        if(!legal_name(message.substr(0,pos)))
        {
            std::cerr << "ERROR: failed to send" << std::endl;
            return false;
        }
        else
        {
            return true;
        }
    }
    std::cout << "ERROR: Invalid input." << std::endl;
    return false;
}


/**
 * As part of the client-server agrement every message first of all will contain in its
 * 4 first bytes the length of the rest of the message. this message adds the length of the
 * message in the first 4 bytes.
 * @param message - the message that needs to be cushioned
 * @return - the cushioned message
 */
std::string cushion(std::string message)
{
    int count = (int)message.length();
    std::string count_char = std::to_string(count);
    int len = (int)count_char.length();
    for(int i=0; i < 4 - len; i++)
    {
        count_char = "0" + count_char;
    }
    return count_char + message;
}


/**
 * A wrapper function to write
 * @param fd - the file descripter that we want  to write into
 * @param message - the message that needs to be written to the fd
 */
void writer(int fd, std::string message)
{
    message = cushion(message);
    int count = (int)message.length();
    int amount_writen;
    amount_writen = (int)write(fd,message.c_str(),(size_t)count);
    if(amount_writen < 0)
    {
        problem(fd,"ERROR: write ", false,errno,1);
    }
    if(amount_writen == 0)
    {
        problem(fd,"Connection failed" , true, 0,0);
    }
    count -= amount_writen;
    while ((amount_writen = (int)write(fd,message.c_str(),(size_t)count)))
    {
        if(amount_writen < 0)
        {
            problem(fd,"ERROR: write ", false,errno,1);
        }
        count -= amount_writen;
    }
}

/**
 * A wrapper function to the read function
 * @param fd - the file descripter that needs to be read from.
 * @return - the message read from the fd
 */
std::string reader(int fd)
{
    ssize_t amount_read;
    size_t message_length;
    char message_length_in_string[4];
    char *temp = message_length_in_string;
    size_t digits = 4;
    amount_read = read(fd,temp,digits);
    digits -=amount_read;
    temp += amount_read;
    if(amount_read == 0)
    {
        close(fd);
        exit(1);
    }
    while((amount_read = read(fd,temp,digits)) > 0)
    {
        digits -=amount_read;
        temp += amount_read;
    }
    if (amount_read < 0)
    {
        problem(fd,"ERROR: read ", false,errno,1);
    }
    message_length = atoi(message_length_in_string);
    char message[message_length];
    temp = message;
    while((amount_read = read(fd,temp,message_length)) > 0)
    {
        message_length -=amount_read;
        temp += amount_read;
    }
    if (amount_read < 0)
    {
        problem(fd,"ERROR: read ", false,errno,1);
    }
    return std::string(message, atoi(message_length_in_string));
}


/**
 * The main function. first tries to create a socket and then connect to a server.
 * if everything went well, it will be able to recieve and send messages through the server
 * to the other connected clients.
 * @param argc - should be 4, otherwise error will be printed
 * @param argv - agruments that contain the name of the client and to what ip and port
 * it wishes to atemept to connect to.
 * @return
 */
int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        std::cout<<"Usage: whatsappClient clientName serverAddress serverPort"<<std::endl;
        exit(1);
    }
    int socket_fd;

    std::string name = (std::string)argv[1];
    struct sockaddr_in my_addr;
    memset(&my_addr,0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons((uint16_t)atoi(argv[3]));
    inet_aton(argv[2],&(my_addr.sin_addr));

    if ((socket_fd = socket(AF_INET, SOCK_STREAM,0))<0)
    {
        std::cerr << "ERROR: socket " << errno << std::endl;
        exit(1);
    }

    if(!legal_name(name))
    {
        problem(socket_fd,"Failed to connect the server",true,0, 0);
    }


    if (connect(socket_fd, (struct sockaddr *)&my_addr , sizeof(my_addr)) < 0 ) {
        problem(socket_fd,"ERROR: connect", false, errno,1);
    }

    name = cushion("create_client " + name);
    unsigned int count = (unsigned int) name.length();
    ssize_t amount_read;
    while((amount_read = write(socket_fd,name.c_str(),count)))
    {
        if(amount_read < 0 )
        {
            problem(socket_fd,"ERROR: connect ", false, errno,1);
        }
        count-=amount_read;
    }
    std::string ans = reader(socket_fd);
    if(ans == "1")
    {
        problem(socket_fd,"Client name is already in use.",true,0,0);
    }
    std::cout<<"Connected Successfully."<<std::endl;

    std::string message;
    fd_set read_fds;
    while (true)
    {
        FD_ZERO(&read_fds);
        FD_SET(socket_fd, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        if (select(std::max(socket_fd,STDIN_FILENO) + 1, &read_fds,NULL,NULL,NULL) < 0)
        {
            problem(socket_fd,"ERROR: select ", false, errno, 1);
        }
        if(FD_ISSET(STDIN_FILENO,&read_fds))
        {
            getline(std::cin,message);
            if(check_message(message))
            {
                writer(socket_fd,message);
                message = reader(socket_fd);
                if(message == "Unregistered successfully.")
                {
                    problem(socket_fd,"Unregistered Successfully.",true, 0,0);
                }
                std::cout << message << std::endl;
            }

        }
        if(FD_ISSET(socket_fd,&read_fds))
        {
            message = reader(socket_fd);
            if(message == "server_exit")
            {
                close(socket_fd);
                exit(0);

            }
            std::cout << message<< std::endl;

        }
    }
}