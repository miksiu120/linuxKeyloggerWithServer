#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <linux/inet.h>
#include <linux/uio.h> // Include for kvec

#define PORT 5247

static struct socket *sock = NULL;

static int init_socket(void)
{
    struct sockaddr_in server_addr = {0};
    int ret = 0;

    // Tworzenie gniazda
    ret = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
    if (ret < 0)
    {
        pr_err("Failed to create socket: %d\n", ret);
        return ret;
    }

    // Przygotowanie adresu serwera
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    ret = in4_pton("127.0.0.1", -1, (u8 *)&server_addr.sin_addr.s_addr, '\0', NULL);
    if (ret != 1)
    {
        pr_err("Invalid IP address\n");
        sock_release(sock);
        return -EINVAL;
    }

    // Połączenie z serwerem
    ret = kernel_connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr), 0);
    if (ret < 0)
    {
        pr_err("Failed to connect to server: %d\n", ret);
        sock_release(sock);
        return ret;
    }

    pr_info("Socket initialized and connected to server\n");
    return 0;
}

static void cleanup_socket(void)
{
    if (sock)
    {
        sock_release(sock);
        sock = NULL;
    }
    pr_info("Socket cleaned up\n");
}

static int send_http_post(const char *post_data)
{
    struct msghdr msg = {0};
    struct kvec vec = {0};
    char *http_request = NULL;
    char *response = NULL;
    int ret = 0;

    // Przygotowanie żądania HTTP POST
    http_request = kzalloc(1024, GFP_KERNEL);
    if (!http_request)
    {
        pr_err("Memory allocation for HTTP request failed\n");
        return -ENOMEM;
    }

    ret = snprintf(http_request, 1024,
                   "POST /weatherforecast HTTP/1.1\r\n"
                   "Host: localhost:%d\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: %zu\r\n"
                   "\r\n"
                   "%s",
                   PORT, strlen(post_data), post_data);

    if (ret < 0 || ret >= 1024)
    {
        pr_err("Failed to construct HTTP request\n");
        kfree(http_request);
        return -EINVAL;
    }

    // Przygotowanie danych do wysłania
    vec.iov_base = http_request;
    vec.iov_len = strlen(http_request);
    msg.msg_flags = 0;

    // Wysyłanie żądania HTTP POST
    ret = kernel_sendmsg(sock, &msg, &vec, 1, strlen(http_request));
    if (ret < 0)
    {
        pr_err("Failed to send HTTP request: %d\n", ret);
    }
    else
    {
        pr_info("HTTP POST request sent successfully\n");
    }

    // Odbieranie odpowiedzi
    response = kzalloc(1024, GFP_KERNEL);
    if (!response)
    {
        pr_err("Memory allocation for response failed\n");
        kfree(http_request);
        return -ENOMEM;
    }

    vec.iov_base = response;
    vec.iov_len = 1024;

    ret = kernel_recvmsg(sock, &msg, &vec, 1, 1024, msg.msg_flags);
    if (ret < 0)
    {
        pr_err("Failed to receive response: %d\n", ret);
    }
    else
    {
        pr_info("Received response: %s\n", response);
    }

    // Zwolnienie zasobów
    kfree(http_request);
    kfree(response);

    return ret < 0 ? ret : 0;
}