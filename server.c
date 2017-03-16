#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>
#include <zconf.h>

#define FAIL -1
#define SUCCESS 1
#define ACK "ACK"
#define NACK "NACK"

int server_socket, client_socket;

int check_crc(char *input, char *crc_key)
{
    char temp[30], quotient[100], remainder[30], temp_key[30];
    int i, j, key_length = strlen(crc_key), message_length = strlen(input);
    strcpy(temp_key, crc_key);
    for (i = 0; i < key_length - 1; i++)
    {
        input[message_length + i] = '0';
    }
    for (i = 0; i < key_length; i++)
    {
        temp[i] = input[i];
    }
    for (i = 0; i < message_length; i++)
    {
        quotient[i] = temp[0];
        if (quotient[i] == '0')
        {
            for (j = 0; j < key_length; j++)
            {
                crc_key[j] = '0';
            }
        }
        else
        {
            for (j = 0; j < key_length; j++)
            {
                crc_key[j] = temp_key[j];
            }
        }
        for (j = key_length - 1; j > 0; j--)
        {
            if (temp[j] == crc_key[j])
            {
                remainder[j - 1] = '0';
            }
            else
            {
                remainder[j - 1] = '1';
            }
        }
        remainder[key_length - 1] = input[i + key_length];
        strcpy(temp, remainder);
    }
    strcpy(remainder, temp);
    for (i = 0; i < key_length - 1; i++)
    {
        if (remainder[i] != '0')
        {
            return FAIL;
        }
    }
    return SUCCESS;
}

float pow(float x, int y)
{
    float temp;
    if (y == 0)
    {
        return 1;
    }
    temp = pow(x, y / 2);
    if (y % 2 == 0)
    {
        return temp * temp;
    }
    else
    {
        if (y > 0)
        {
            return x * temp * temp;
        }
        else
        {
            return (temp * temp) / x;
        }
    }
}

void show_message(char *message, int data_len)
{
    printf("Message received: ");
    unsigned int i;
    for (i = 0; i < data_len - 8; i++)
    {
        int j;
        int v = 0;
        for (j = i; j < i + 8; j++)
        {
            if (message[j] == '1')
            {
                v += pow(2, i + 7 - j);
            }
        }
        printf("%c", (char)v);
        i = i + 7;
    }
    printf("\n");
}

void process(int client_socket, float drop_probability)
{
    int data_len;
    do
    {
        char message[1024];
        data_len = read(client_socket, message, 1024);
        message[data_len] = '\0';
        char crc_key[20] = "100000111";
        if (data_len && check_crc(message, crc_key) == SUCCESS)
        {
            show_message(message, data_len);
            printf("Sending ACK....");
            if (((float)rand() / RAND_MAX) < drop_probability)
            {
                printf("Packet dropped!\n");
                continue;
            }
            int sent = send(client_socket, ACK, strlen(ACK), 0);
            printf("Sent successfully!\n");
        }
        else if (data_len)
        {
            show_message(message, data_len);
            printf("Message retrieved had some errors, sending NACK....");
            if (((float)rand() / RAND_MAX) < drop_probability)
            {
                printf("Packet dropped!\n");
                continue;
            }
            int sent = send(client_socket, NACK, strlen(NACK), 0);
            printf("Sent successfully!\n");
        }
        else
        {
            close(client_socket);
        }
    } while (data_len);
}

int setup_connection(int port)
{
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < -1)
    {
        perror("socket: ");
        return FAIL;
    }
    printf("Created socket...\n");

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&server.sin_zero, 8);

    if ((bind(server_socket, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0))
    {
        perror("binding: ");
        return FAIL;
    }
    printf("Done with binding...\n");

    if ((listen(server_socket, 5)) < 0)
    {
        perror("listening: ");
        return FAIL;
    }
    printf("Listening...\n");
    return SUCCESS;
}

void signal_callback(int signum)
{
    printf("Caught signal %d. Releasing resources...\n", signum);
    close(client_socket);
    close(server_socket);
    exit(signum);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: ./server port\n");
        return EXIT_FAILURE;
    }
    signal(SIGINT, signal_callback);
    float drop_probability;
    printf("Enter probability to drop packets: ");
    scanf("%f", &drop_probability);

    if (setup_connection(atoi(argv[1])) == FAIL)
    {
        return EXIT_FAILURE;
    }

    srand(NULL);
    while (1)
    {
        struct sockaddr_in client;
        unsigned int len;
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client, &len)) < 0)
        {
            perror("Accepting: ");
            return EXIT_FAILURE;
        }
        printf("Accepted...\n");

        int pid;
        if ((pid = fork()) < 0)
        {
            perror("forking: ");
            return EXIT_FAILURE;
        }
        if (pid == 0)
        {
            close(server_socket);
            process(client_socket, drop_probability);
            printf("Done...You can terminate the server now!\n");
            return EXIT_SUCCESS;
        }
        else
        {
            close(client_socket);
        }
    }
}
