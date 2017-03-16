#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>

#define FAIL -1
#define SUCCESS 1
#define ACK "ACK"
#define NACK "NACK"

void append_remainder(char *mod_message, char *crc_key)
{
    char temp[30], quotient[100], remainder[30], temp_key[30];
    int i, j, key_length = strlen(crc_key), message_length = strlen(mod_message);
    strcpy(temp_key, crc_key);
    for (i = 0; i < key_length - 1; i++)
    {
        mod_message[message_length + i] = '0';
    }
    for (i = 0; i < key_length; i++)
    {
        temp[i] = mod_message[i];
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
        remainder[key_length - 1] = mod_message[i + key_length];
        strcpy(temp, remainder);
    }
    strcpy(remainder, temp);
    for (i = 0; i < key_length - 1; i++)
    {
        mod_message[message_length + i] = remainder[i];
    }
    mod_message[message_length + key_length - 1] = '\0';
}

void convert_to_binary(const char *message, char *mod_message)
{
    unsigned int i;
    int j;
    for (i = 0; i < strlen(message); i++)
    {
        int c = message[i];
        for (j = 7; j >= 0; j--)
        {
            if (c & 1)
            {
                mod_message[(i * 8) + j] = '1';
            }
            else
            {
                mod_message[(i * 8) + j] = '0';
            }
            c = c >> 1;
        }
    }
    mod_message[i * 8] = '\0';
}

void add_error(char *mod_message, float BER)
{
    unsigned int i;
    for (i = 0; i < strlen(mod_message); i++)
    {
        if ((float)rand() / RAND_MAX < BER)
        {
            if (mod_message[i] == '0')
            {
                mod_message[i] = '1';
            }
            else if (mod_message[i] == '1')
            {
                mod_message[i] = '0';
            }
            else
            {
                fprintf(stderr, "Convert to binary was not successful\n");
            }
        }
    }
}

void message_transform(const char *message, char *mod_message, float BER)
{
    char crc_key[20] = "100000111";
    convert_to_binary(message, mod_message);
    //printf("Binary form: %s\n", mod_message);
    append_remainder(mod_message, crc_key);
    add_error(mod_message, BER);
    //printf("Tampered form: %s\n", mod_message);
}

int communicate(int client_socket, char *mod_message)
{
    if (send(client_socket, mod_message, strlen(mod_message), 0) < 0)
    {
        fprintf(stderr, "Sent error...\n");
        return FAIL;
    }
    printf("Sent message. Waiting for ACK/NACK...\n");

    int reply_length;
    char reply[64];
    if ((reply_length = recv(client_socket, reply, 64, 0)) < 0)
    {
        fprintf(stderr, "Timeout. Re-transmitting...\n");
        return FAIL;
    }
    reply[reply_length] = '\0';
    printf("Reply received: %s\n", reply);
    if (strcmp(reply, NACK) == 0)
    {
        fprintf(stderr, "Previous transmission had some error. Re-transmitting...\n");
        return FAIL;
    }
    else if (strcmp(reply, ACK) == 0)
    {
        return SUCCESS;
    }
    else
    {
        fprintf(stderr, "Error in ACK and NACK\n");
        return FAIL;
    }
}

int setup_connection(int port, char *address)
{
    int client_socket;
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation: ");
        return FAIL;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(address);
    bzero(&server.sin_zero, 8);

    struct timeval time_val;
    time_val.tv_sec = 5;  // 5 Secs timeout
    time_val.tv_usec = 0; // Not initialising this can cause strange errors

    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&time_val, sizeof(struct timeval));

    if (connect(client_socket, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0)
    {
        perror("Connecting: ");
        return FAIL;
    }
    return client_socket;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage ./client ip_address port");
        return -1;
    }
    float BER;
    printf("Enter BER (probability of bit errors): ");
    scanf("%f", &BER);

    int client_socket;
    if ((client_socket = setup_connection(atoi(argv[2]), argv[1])) == FAIL)
    {
        return EXIT_FAILURE;
    }
    srand(NULL);
    while (1)
    {
        char message[64];
        printf("Enter your message: ");
        scanf("%s", message);
        char mod_message[1024];
        do
        {
            message_transform(message, mod_message, BER);
        } while (communicate(client_socket, mod_message) == FAIL);
    }
    close(client_socket);
    return EXIT_SUCCESS;
}