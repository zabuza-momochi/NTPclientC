// Include declaration ----------------------------------------------------
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
// ------------------------------------------------------------------------

// Struct declaration -----------------------------------------------------
typedef struct ntp_packet
{
    unsigned char leap_version_mode;                // Leap indicator (LI) | Version number (VN) | Mode
    unsigned char stratum;                          // Stratum level of the local clock
    unsigned char poll;                             // Maximum interval between successive messages
    unsigned char precision;                        // Clock precision (log2 to the base 2 of the clock precision)
    unsigned int root_delay;                        // Total round-trip delay to the reference clock
    unsigned int root_dispersion;                   // Maximum error relative to the reference clock
    char ref_id[4];                                 // Reference identifier (ASCII characters or code)
    unsigned int ref_timestamp_decimal;             // Reference timestamp (seconds since 1900-01-01 00:00:00)
    unsigned int ref_timestamp_fraction;            // Reference timestamp fraction (fractional part)
    unsigned int original_timestamp_decimal;        // Originate timestamp (time request sent by client)
    unsigned int original_timestamp_fraction;       // Originate timestamp fraction (fractional part)
    unsigned int receive_timestamp_decimal;         // Receive timestamp (time request received by server)
    unsigned int receive_timestamp_fraction;        // Receive timestamp fraction (fractional part)
    unsigned int transmit_timestamp_decimal;        // Transmit timestamp (time reply sent by server)
    unsigned int transmit_timestamp_fraction;       // Transmit timestamp fraction (fractional part)
} ntp_packet;
// BIG ENDIAN, pay attention!
// ------------------------------------------------------------------------

// Convert 32-bit integer from big-endian to little-endian
unsigned int convertBigEndianToLittle(unsigned int value)
{
    return ((value & 0xFF) << 24) |
           (((value >> 8) & 0xFF) << 16) |
           (((value >> 16) & 0xFF) << 8) |
           ((value >> 24) & 0xFF);
}

// Convert 32-bit integer from little-endian to big-endian
unsigned int convertLittleEndianToBig(unsigned int value)
{
    return ((value & 0xFF) << 24) |
           (((value >> 8) & 0xFF) << 16) |
           (((value >> 16) & 0xFF) << 8) |
           ((value >> 24) & 0xFF);
}

// Entry point ------------------------------------------------------------
int main(int argc, char *args[])
{
#ifdef _WIN32
    // This part is only required on windows, init the winsock2 dll
    WSADATA wsa_data;

    // Init winsock library on windows
    if (WSAStartup(0x0202, &wsa_data))
    {
        printf("unable to initialize winsock2 \n");
        return -1;
    }
#endif

    // Init packet struct
    ntp_packet packet;

    // Reset data
    memset(&packet, 0, sizeof(packet));

    // Set vars
    int leap = 0;
    int version = 4;
    int mode = 3;

    // Get current time in seconds since the epoch (1970)
    time_t current_time;
    time(&current_time);

    // Convert to unsigned int
    unsigned int now = (unsigned int)current_time;

    // Set leap, version and mode, shift bit
    packet.leap_version_mode = (leap << 6) | (version << 3) | mode;

    // Set current time to original timestamp
    packet.original_timestamp_decimal = convertLittleEndianToBig(now);

    // Create a UDP socket
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // Check if the socket creation is successful
    if (s < 0)
    {
        printf("Unable to initialize the UDP socket \n");
        return -1;
    }

    printf("---------------------------\n");
    printf("Socket %d created \n", s);

    // Set up the server address structure
    struct sockaddr_in sin;

    // Convert the IP address string to a binary representation
    inet_pton(AF_INET, "216.239.35.12", &sin.sin_addr); // this will create a big endian 32 bit address

    // Set the address family of the sin structure to AF_INET, which indicates that the address is an IPv4 address.
    sin.sin_family = AF_INET;

    // Set the port number to 9999 and convert it to big-endian format
    sin.sin_port = htons(123);

    // Send the packet to the server
    int sent_bytes = sendto(s, (const char *)&packet, sizeof(packet), 0, (struct sockaddr *)&sin, sizeof(sin));

    // Print the number of bytes sent
    printf("---------------------------\n");
    printf("Sent %d bytes via UDP \n", sent_bytes);

    // Reuse same struct, reset data
    memset(&packet, 0, sizeof(packet));

    // Set struct size
    socklen_t addr_len = sizeof(sin);

    // Wait for response from the server
    int received_bytes = recvfrom(s, (char *)&packet, sizeof(packet), 0, (struct sockaddr *)&sin, &addr_len);

    // Check response from the server
    if (received_bytes < 0)
    {
        printf("---------------------------\n");
        printf("Error receiving response from the server\n");
    }
    else
    {
        // Process response
        printf("---------------------------\n");
        printf("Received %d bytes via UDP\n", received_bytes);

        // Print response stats
        printf("---------------------------\n");
        printf("NTP DATA: \n");
        printf("--------------\n");
        printf("leap: %d\n", packet.leap_version_mode >> 6);
        printf("version: %d\n", packet.leap_version_mode >> 3 & 0b111);
        printf("mode: %d\n", packet.leap_version_mode & 0b111);
        printf("stratum: %u\n", packet.stratum);
        printf("poll: %u\n", packet.poll);
        printf("precision: %u\n", packet.precision);
        printf("root_delay: %u\n", convertBigEndianToLittle(packet.root_delay));
        printf("root_dispersion: %u\n", convertBigEndianToLittle(packet.root_dispersion));
        printf("reference id: %c%c%c%c\n", packet.ref_id[0], packet.ref_id[1], packet.ref_id[2], packet.ref_id[3]);
        printf("ref_timestamp: %u\n", convertBigEndianToLittle(packet.ref_timestamp_decimal));
        printf("original_timestamp: %u\n", convertBigEndianToLittle(packet.original_timestamp_decimal));
        printf("receive_timestamp: %u\n", convertBigEndianToLittle(packet.receive_timestamp_decimal));
        printf("transmit_timestamp: %u\n", convertBigEndianToLittle(packet.transmit_timestamp_decimal));

        // Convert NTP timestamp to Unix timestamp (seconds since 1970-01-01 00:00:00 UTC)
        time_t unix_timestamp = (time_t)(convertBigEndianToLittle(packet.transmit_timestamp_decimal) - 2208988800);

        // Convert Unix timestamp to struct tm for formatting
        struct tm tm_info;

        if (gmtime_s(&tm_info, &unix_timestamp) == 0)
        {
            // Format the date and time
            char formatted_time[20];

            // Add GMT+1:00
            tm_info.tm_hour +=1;

            // Set string format
            strftime(formatted_time, sizeof(formatted_time), "%Y-%m-%d %H:%M:%S", &tm_info);

            // Print formatted string
            printf("---------------------------\n");
            printf("Time from NTP Response: %s\n", formatted_time);
            printf("---------------------------\n");
            printf("END -----------------------\n");
        }
        else
        {
            printf("---------------------------\n");
            printf("Error in converting timestamp.\n");
        }
    }

    // Close the socket
#ifdef _WIN32
    closesocket(s);
    WSACleanup();  // Cleanup Winsock on Windows
#else
    close(s);
#endif

    return 0;
}