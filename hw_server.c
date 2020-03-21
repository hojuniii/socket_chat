#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 30
void error_handling(char *message);
void read_childproc(int sig);
void write_routine(int sock, char *buf);

int main(int argc, char **argv)
{
    //two clnt socket
    int serv_sock, clnt_sock[2];
    struct sockaddr_in serv_adr, clnt_adr;
    int status;

    //fd[0] for client1
    //fd[1] for client2
    int fd[2][2];

    pid_t pid;
    struct sigaction act;
    socklen_t adr_sz;
    int str_len, state;
    char buf[BUF_SIZE];

    //signal for zombie
    act.sa_handler = read_childproc;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    state = sigaction(SIGCHLD, &act, 0);

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(9190);

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 2) == -1)
        error_handling("listen() error");

    //use pipe for IPC
    pipe(fd[0]), pipe(fd[1]);

    for (int i = 0; i < 2; i++)
    {
        adr_sz = sizeof(clnt_adr);

        //accept two client socket
        clnt_sock[i] = accept(serv_sock, (struct sockaddr *)&clnt_adr, &adr_sz);

        if (clnt_sock[i] == -1)
            continue;
        else
            printf("* A client connected. Socket id: %d \n", clnt_sock[i]);
    }
    printf("both connected\n");

    for (int i = 0; i < 2; i++)
    {
        pid = fork();        

        //child process
        if (pid == 0)
        {
            printf("* Child process %d (pid = %d) created for client with socket id: %d \n", i + 1, getpid(), clnt_sock[i]);
            //read msg from pipe
            while ((str_len = read(fd[i][0], buf, BUF_SIZE)) != 0)
            {
                //if read 'q' or 'Q' close socket
                if (!strcmp(buf, "q\n") || !strcmp(buf, "Q\n"))
                {
                    printf("A client disconnected\n");
                    close(clnt_sock[i]);
                    return 0;
                }
                else{
                    //write to client socket !
                    write(clnt_sock[i], buf, str_len);
                    printf("Child process (pid = %d) sending the message\n", getpid());
                }
            }
        }
        else
        {
            close(clnt_sock[i]);
        }
    }

    //parent process
    if (pid != 0)
    {
        while (1)
        {
            sleep(1);
            printf("Input message (for sending): ");
            //write message to buffer
            fgets(buf, BUF_SIZE, stdin);

            if (!strcmp(buf, "q\n") || !strcmp(buf, "Q\n"))
            {
                write(fd[0][1], buf, BUF_SIZE);
                write(fd[1][1], buf, BUF_SIZE);
                exit(0);
                return 0;
            }
            
            //piping the message to each child process
            if (write(fd[0][1], buf, BUF_SIZE) != -1)
                printf("* Piping the message to child process  1\n");

            if (write(fd[1][1], buf, BUF_SIZE) != -1)
                printf("* Piping the message to child process  2\n");
        }
    }

    close(serv_sock);
    return 0;
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void read_childproc(int sig)
{
    pid_t pid;
    int status;

    pid = waitpid(-1, &status, WNOHANG);
    if (WIFEXITED(status))
    {
        printf("removed proc id: %d\n", pid);
    }
}