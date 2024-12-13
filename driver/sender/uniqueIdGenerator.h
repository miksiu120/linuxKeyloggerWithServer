#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

const int UNIQUE_ID_LENGTH = 32;
const char *ID_FILE_PATH = "/etc/unique_id";

static char *uniqueIdGenerator(void)
{
    char *uniqueId = kmalloc(UNIQUE_ID_LENGTH + 1, GFP_KERNEL);
    if (!uniqueId)
    {
        pr_err("Failed to allocate memory for unique ID\n");
        return NULL;
    }

    unsigned char random_bytes[UNIQUE_ID_LENGTH];
    get_random_bytes(random_bytes, UNIQUE_ID_LENGTH);

    for (int i = 0; i < UNIQUE_ID_LENGTH; i++)
    {
        int random = random_bytes[i] % 3;

        if (random == 0)
        {
            uniqueId[i] = '0' + (random_bytes[i] % 10);
        }
        else if (random == 1)
        {
            uniqueId[i] = 'a' + (random_bytes[i] % 26);
        }
        else
        {
            uniqueId[i] = 'A' + (random_bytes[i] % 26);
        }
    }

    uniqueId[UNIQUE_ID_LENGTH] = '\0';

    return uniqueId;
}

static char *getOrCreateUniqueId(void)
{
    struct file *file;
    char *uniqueId = kmalloc(UNIQUE_ID_LENGTH + 1, GFP_KERNEL);
    if (!uniqueId)
    {
        pr_err("Failed to allocate memory for unique ID\n");
        return NULL;
    }

    file = filp_open(ID_FILE_PATH, O_RDONLY, 0);
    if (IS_ERR(file))
    {
        pr_info("ID file not found, generating new ID\n");
        uniqueId = uniqueIdGenerator();
        if (!uniqueId)
        {
            return NULL;
        }

        file = filp_open(ID_FILE_PATH, O_WRONLY | O_CREAT, 0644);
        if (IS_ERR(file))
        {
            pr_err("Failed to create ID file\n");
            kfree(uniqueId);
            return NULL;
        }

        kernel_write(file, uniqueId, UNIQUE_ID_LENGTH, &file->f_pos);
        filp_close(file, NULL);
    }
    else
    {
        kernel_read(file, uniqueId, UNIQUE_ID_LENGTH, &file->f_pos);
        uniqueId[UNIQUE_ID_LENGTH] = '\0';
        filp_close(file, NULL);
    }

    return uniqueId;
}