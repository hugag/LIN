/*
 *  chardev.c: Creates a read-only char device that says how many times
 *  you've read from the dev file
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>  /* for copy_to_user */
#include <linux/cdev.h>
#include <linux/kfifo.h>

MODULE_DESCRIPTION("Chardev2 Kernel Module - FDI-UCM");
MODULE_AUTHOR("Juan Carlos Saez");
MODULE_LICENSE("GPL");

/*
 *  Prototypes
 */
int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

#define SUCCESS 0
#define DEVICE_NAME "prodcons"   /* Dev name as it appears in /proc/devices   */
#define MAX_ITEMS 4     /* Max length of the message from the device */
#define KBUF_SERGIO 128

/*
 * Global variables are declared as static, so are global within the file.
 */

static dev_t start;
static struct cdev* chardev = NULL;
static int Device_Open = 0; /* Is device open?
                 * Used to prevent multiple access to device */
struct kfifo cbuff;
struct semaphore mtx,huecos,elementos;

static struct file_operations fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

/*
 * This function is called when the module is loaded
 */
int init_module(void)
{
    int major;      /* Major number assigned to our device driver */
    int minor;      /* Minor number assigned to the associated character device */
    int ret;

    /* Get available (major,minor) range */
    if ((ret = alloc_chrdev_region (&start, 0, 1, DEVICE_NAME))) {
        printk(KERN_INFO "Can't allocate chrdev_region()");
        return ret;
    }

    /* Create associated cdev */
    if ((chardev = cdev_alloc()) == NULL) {
        printk(KERN_INFO "cdev_alloc() failed ");
        ret = -ENOMEM;
        goto error_alloc;
    }

    cdev_init(chardev, &fops);

    if ((ret = cdev_add(chardev, start, 1))) {
        printk(KERN_INFO "cdev_add() failed ");
        goto error_add;
    }

    major = MAJOR(start);
    minor = MINOR(start);

    sema_init(&mtx,1);
    sem_init(&huecos,0);
    sem_init(&elementos,MAX_ITEMS);
    int ret = kfifo_alloc(&cbuff,MAX_ITEMS*4,GPF_KERNEL);

    if(ret)
        goto error_add;


    return 0;

error_add:
    /* Destroy partially initialized chardev */
    if (chardev)
        kobject_put(&chardev->kobj);

error_alloc:
    unregister_chrdev_region(start, 1);

    return ret;
}

/*
 * This function is called when the module is unloaded
 */
void cleanup_module(void)
{
    /* Destroy chardev */
    if (chardev)
        cdev_del(chardev);

    if(down_interruptible(&mtx))
        return -EINTR;
    
    kfifo_free(&cbuff);

    uo(&mtx);
    /*
     * Unregister the device
     */
    unregister_chrdev_region(start, 1);
}

/*
 * Called when a process tries to open the device file, like
 * "cat /dev/chardev"
 */
static int device_open(struct inode *inode, struct file *file)
{
    if (Device_Open)
        return -EBUSY;

    Device_Open++;

    /* Increment the module's reference counter */
    try_module_get(THIS_MODULE);

    return SUCCESS;
}

/*
 * Called when a process closes the device file.
 */
static int device_release(struct inode *inode, struct file *file)
{
    Device_Open--;      /* We're now ready for our next caller */

    /*
     * Decrement the usage count, or else once you opened the file, you'll
     * never get get rid of the module.
     */
    module_put(THIS_MODULE);

    return 0;
}

/*
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t device_read(struct file *filp,   /* see include/linux/fs.h   */
                           char *buffer,    /* buffer to fill with data */
                           size_t length,   /* length of the buffer     */
                           loff_t * offset)
{
    char kbuf [KBUF_SERGIO];
    int nr_bytes = 0;
    int val;

    if((*offset) > 0)
        return 0;

    if(down_interruptible(&elementos))
        return -EINVAL;

    if(down_interruptible(&mtx)){
        up(&elementos);
        return -EINVAL;
    }
    
    kfifo_out(&cbuff,&val,sizeof(int));

    up(&mtx);
    up(&huecos);

    sprintf(kbuf, "%d\n", val);

    if((nr_bytes = strlen(kbuf)) >= length)
        return -ENOSPC;
    

    /* Transfer data from the kernel to userspace */  
    if (copy_to_user(buffer, kbuf,nr_bytes)){
        printk("prodcons: couldnt copy to user");
        return -EINVAL;
    }

}

/*
 * Called when a process writes to dev file: echo "hi" > /dev/chardev
 */
static ssize_t
device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
    char kbuff[KBUF_SERGIO];
    int digit;

    if(len > KBUF_SERGIO-1)
        return -EINVAL;
    if (copy_from_user( kbuff, buff, len ))  
        return -EFAULT;

    if(sscanf(kbuff,"%d,n",&digit) != 1)
        return -EINVAL;

    if(down_interruptible(&huecos))
        return -EINTR;
    if(down_interruptible(&mtx)){
        up(&huecos);
        return -EINVAL;
    }

    kfifo_in(&cbuf, &digit,sizeof(int));

    up(&mtx);
    up(&elementos);

}