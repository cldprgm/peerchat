#include "cli_args.h"
#include "network.h"

#include <stdio.h>
#include <winsock2.h>

#define DEFAULT_PORT 8082

int main(int argc, char *argv[])
{
    WSADATA winsock_data;
    int startup_result = WSAStartup(MAKEWORD(2, 2), &winsock_data);
    if (startup_result != 0)
    {
        fprintf(stderr, "WSAStartup failed: %d\n", startup_result);
        return 1;
    }

    // validate cli args
    int user_mode_choice;
    if ((user_mode_choice = parse_valid_mode(argc, argv)) == 0)
    {
        WSACleanup();
        return 1;
    }

    struct in_addr user_addr_choice;
    if (user_mode_choice == MODE_CLIENT)
    {
        if (parse_valid_ip4(argc, argv, &user_addr_choice) == 0)
        {
            WSACleanup();
            return 1;
        }
    }

    int user_port_choice;
    if ((user_port_choice = parse_valid_port(argc, argv)) == 0)
        user_port_choice = DEFAULT_PORT;

    // mode choice
    switch (user_mode_choice)
    {
    case 1:
        printf("\n");
        start_server(user_port_choice);
        break;
    case 2:
        printf("\n");
        start_client(&user_addr_choice, user_port_choice);
        break;

    default:
        printf("\nIncorrect mode (use: 'host' or 'client')\n");
        break;
    }

    WSACleanup();
    return 0;
}