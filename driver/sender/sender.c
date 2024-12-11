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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Kernel Module Sending HTTP Request");

static int __init send_http_post_init(void)
{
    struct socket *sock;
    struct sockaddr_in server_addr;
    struct msghdr msg = {0};
    struct kvec vec;
    char *post_data = "{\"Keys\": \"JESTEM KERNEL\"}"; // JSON data to send
    char *http_request;
    char *response;
    int ret;

    // Przygotowanie żądania HTTP POST
    http_request = kzalloc(1024, GFP_KERNEL);
    if (!http_request)
    {
        pr_err("Memory allocation for HTTP request failed\n");
        return -ENOMEM;
    }
    // Konstruowanie zapytania HTTP POST do endpointu /weatherforecast
    ret = snprintf(http_request, 1024,
                   "POST /weatherforecast HTTP/1.1\r\n"
                   "Host: localhost:5247\r\n"
                   "User-Agent: kernel\r\n"
                   "Accept: */*\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: %zu\r\n"
                   "\r\n"
                   "%s",
                   strlen(post_data), post_data);

    if (ret < 0)
    {
        pr_err("Failed to construct HTTP request\n");
        kfree(http_request);
        return -EINVAL;
    }

    // Tworzenie gniazda
    ret = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
    if (ret < 0)
    {
        pr_err("Failed to create socket\n");
        kfree(http_request);
        return ret;
    }

    // Przygotowanie adresu serwera
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT); // Port serwera .NET (5247)
    ret = in4_pton("127.0.0.1", strlen("127.0.0.1"), (u8 *)&server_addr.sin_addr.s_addr, '\0', NULL);
    if (ret != 1)
    {
        pr_err("Invalid IP address\n");
        sock_release(sock);
        kfree(http_request);
        return -EINVAL;
    }

    // Połączenie z serwerem
    ret = sock->ops->connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr), 0);
    if (ret < 0)
    {
        pr_err("Failed to connect to server\n");
        sock_release(sock);
        kfree(http_request);
        return ret;
    }

    // Przygotowanie struktury kvec
    vec.iov_base = http_request;
    vec.iov_len = strlen(http_request);

    // Przygotme = &server_addr;
    msg.msg_namelen = sizeof(server_addr);
    msg.msg_flags = 0;

    // Wysyłanie żądania HTTP POST
    ret = kernel_sendmsg(sock, &msg, &vec, 1, strlen(http_request)); // Use struct kvec here
    if (ret < 0)
    {
        pr_err("Failed to send HTTP request\n");
    }
    else
    {
        pr_info("HTTP POST request sent successfully\n");
    }

    // Odbieranie odpowiedzi z serwera
    response = kzalloc(1024, GFP_KERNEL);
    if (!response)
    {
        pr_err("Memory allocation for response failed\n");
        sock_release(sock);
        kfree(http_request);
        return -ENOMEM;
    }

    vec.iov_base = response;
    vec.iov_len = 1024;

    ret = kernel_recvmsg(sock, &msg, &vec, 1, 1024, msg.msg_flags);
    if (ret < 0)
    {
        pr_err("Failed to receive response\n");
    }
    else
    {
        pr_info("Received response: %s\n", response);
    }

    // Zwolnienie zasobów
    sock_release(sock);
    kfree(http_request);
    kfree(response);

    return 0;
}

static void __exit send_http_post_exit(void)
{
    pr_info("HTTP POST kernel module unloaded\n");
}

module_init(send_http_post_init);
module_exit(send_http_post_exit);