#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>  /* for copy_to_user */
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/semaphore.h>
#include <linux/device.h>   /* for class_create and device_create */
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,2,1)
#define __cconst__ const
#else
#define __cconst__ 
#endif


MODULE_DESCRIPTION("Chardev2 Kernel Module - FDI-UCM");
MODULE_AUTHOR("Juan Carlos Saez");
MODULE_LICENSE("GPL");

#define SUCCESS 0
#define DEVICE_NAME "prodcons"   /* Dev name as it appears in /proc/devices */
#define MAX_ITEMS 4     /* Max length of the message from the device */
#define KBUF_SERGIO 128


int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

/* Global variables */
static dev_t start;
static struct cdev *chardev = NULL;
static struct class *prodcons_class = NULL;  /* Class for /dev entry */
static struct device *prodcons_device = NULL;
static int Device_Open = 0;
struct kfifo cbuff;
struct semaphore sem, huecos, elementos;

static struct file_operations fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

static char *custom_devnode(__cconst__ struct device *dev, umode_t *mode)
{
  if (!mode)
    return NULL;
  if (MAJOR(dev->devt) == MAJOR(start))
    *mode = 0666;
  return NULL;
}
int init_module(void)
{
    int ret;

    /* Allocate a character device region */
    if ((ret = alloc_chrdev_region(&start, 0, 1, DEVICE_NAME))) {
        printk(KERN_ERR "prodcons: Cannot allocate chrdev_region\n");
        return ret;
    }

    /* Allocate and initialize cdev */
    if ((chardev = cdev_alloc()) == NULL) {
        printk(KERN_ERR "prodcons: cdev_alloc() failed\n");
        ret = -ENOMEM;
        goto error_alloc;
    }

    cdev_init(chardev, &fops);

    if ((ret = cdev_add(chardev, start, 1))) {
        printk(KERN_ERR "prodcons: cdev_add() failed\n");
        goto error_add;
    }

    
    prodcons_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(prodcons_class)) {
        printk(KERN_ERR "prodcons: class_create() failed\n");
        ret = PTR_ERR(prodcons_class);
        goto error_class;
    }

    prodcons_class->devnode = custom_devnode;

    /* Create device node in /dev */
    prodcons_device = device_create(prodcons_class, NULL, start, NULL, DEVICE_NAME);
    if (IS_ERR(prodcons_device)) {
        printk(KERN_ERR "prodcons: device_create() failed\n");
        ret = PTR_ERR(prodcons_device);
        goto error_device;
    }

    /* Initialize semaphores and buffer */
    sema_init(&sem, 1);
    sema_init(&huecos, MAX_ITEMS);
    sema_init(&elementos, 0);

    ret = kfifo_alloc(&cbuff, MAX_ITEMS * sizeof(int), GFP_KERNEL);
    if (ret) {
        printk(KERN_ERR "prodcons: kfifo_alloc() failed\n");
        goto error_kfifo;
    }

    printk(KERN_INFO "prodcons: Module loaded successfully\n");
    return 0;

error_kfifo:
    device_destroy(prodcons_class, start);
error_device:
    class_destroy(prodcons_class);
error_class:
    cdev_del(chardev);
error_add:
    kobject_put(&chardev->kobj);
error_alloc:
    unregister_chrdev_region(start, 1);
    return ret;
}

void cleanup_module(void)
{
    /* Clean up resources */
    kfifo_free(&cbuff);

    if (prodcons_device)
        device_destroy(prodcons_class, start);

    if (prodcons_class)
        class_destroy(prodcons_class);

    if (chardev)
        cdev_del(chardev);

    unregister_chrdev_region(start, 1);

    printk(KERN_INFO "prodcons: Module unloaded successfully\n");
}

static int device_open(struct inode *inode, struct file *file)
{
    if (!try_module_get(THIS_MODULE))
        return -EINVAL;

    return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file)
{
    module_put(THIS_MODULE);
    return 0;
}

static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
    char kbuf[KBUF_SERGIO];
    int nr_bytes = 0;
    int val;

    if ((*offset) > 0)
        return 0;

    if (down_interruptible(&elementos))
        return -EINTR;

    if (down_interruptible(&sem)) {
        up(&elementos);
        return -EINTR;
    }

    kfifo_out(&cbuff, &val, sizeof(int));

    up(&sem);
    up(&huecos);

    sprintf(kbuf, "%d\n", val);

    nr_bytes = strlen(kbuf);
    if (nr_bytes > length)
        return -ENOSPC;

    if (copy_to_user(buffer, kbuf, nr_bytes))
        return -EFAULT;

    (*offset) += nr_bytes;
    return nr_bytes;
}

static ssize_t device_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
    char kbuff[KBUF_SERGIO];
    int digit;

    if (len > KBUF_SERGIO - 1)
        return -EINVAL;

    if (copy_from_user(kbuff, buff, len))
        return -EFAULT;

    kbuff[len] = '\0';

    if (sscanf(kbuff, "%d", &digit) != 1)
        return -EINVAL;

    if (down_interruptible(&huecos))
        return -EINTR;

    if (down_interruptible(&sem)) {
        up(&huecos);
        return -EINTR;
    }

    kfifo_in(&cbuff, &digit, sizeof(int));

    up(&sem);
    up(&elementos);

    return len;
}
