#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/rwlock.h>


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("modList Kernel Module - FDI-UCM");
MODULE_AUTHOR("Sergio & Hugo");


#define BUFFER_LENGTH 128
#define LITTLE_BUFFER_LENGTH 10
DEFINE_RWLOCK(pablo);
/* Estructura que representa los nodos de la lista */
struct list_item {
	  int data;
    struct list_head links;
};

struct list_head mylist; /* Nodo fantasma (cabecera) de la lista enlazada */



static struct proc_dir_entry *proc_entry;

static int modlist_open(struct inode *, struct file *){
  try_module_get(THIS_MODULE);
  return 0;
}

static int modlist_release(struct inode *, struct file *){
  module_put(THIS_MODULE);
  return 0;
}

static ssize_t modlist_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
  int n;
  char kbuf[BUFFER_LENGTH];
  
  if(len >= BUFFER_LENGTH){
    printk("modList: not enough space");
    return -ENOSPC;
  }
    
  /* Transfer data from user to kernel space */
  if (copy_from_user( &kbuf, buf, len ))  
    return -EFAULT;

  kbuf[len] = '\0'; /* Add the `\0' */
  *off+=len;            /* Update the file pointer */
  
  if(sscanf(kbuf,"add %i",&n) == 1){
  
    struct list_item* newItem;
    
    if((newItem = (struct list_item *)kmalloc(sizeof(struct list_item), GFP_KERNEL)) == 0){//ADD
      printk("modList: not enough memory");
      return -ENOMEM;
    }
    
    newItem->data = n;
    
    write_lock(&pablo);
      list_add_tail(&newItem->links, &mylist);
    write_unlock(&pablo);
    

    printk("modlist: added new element %i", n);
  }
  else if(sscanf(kbuf,"remove %i", &n)){//REMOVE
    
    struct list_item* item = NULL;
    struct list_head* cur_node = 0;
    struct list_head* tmp = 0;


    write_lock(&pablo);
      list_for_each_safe(cur_node,tmp,&mylist){
        item = list_entry(cur_node, struct list_item, links);
        if(item->data == n){
          list_del(cur_node);
          kfree(item);
        }
      }
    write_unlock(&pablo);
    
  }
  else if (strcmp(kbuf,"cleanup\n") == 0){//CLEAN
  
    struct list_item* item = NULL;
    struct list_head* cur_node = 0;
    struct list_head* tmp = 0;
    
    write_lock(&pablo);
      list_for_each_safe(cur_node,tmp,&mylist){
        item = list_entry(cur_node, struct list_item, links);
        list_del(cur_node);
        kfree(item);
      }
    write_unlock(&pablo);
  }
  else{
    printk("modlist: unknown command");
    return -EINVAL;
  }
  
  
  return len;
}

static ssize_t modlist_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {

  char kbuf[BUFFER_LENGTH] = "";
  char minBuf[LITTLE_BUFFER_LENGTH] = "";
  char* dst = kbuf;
  int nChar = 0;
  
  
  struct list_item* item ;
  struct list_head* cur_node = 0;

  if (*off >= len) {
      return 0; /* end of file */
  }

  try_module_get(THIS_MODULE);
  
  read_lock(&pablo);
    list_for_each(cur_node, &mylist){
      int nBytes;
        
      item = list_entry(cur_node,struct list_item, links);
      
      nBytes = sprintf(minBuf, "%i\n", item->data);
      
      if(nChar + nBytes > BUFFER_LENGTH - 1 || nChar + nBytes > len){
        printk(KERN_INFO"modList: not enough space");
        return -ENOSPC;
      }
      
      strcpy(dst, minBuf);
      
      dst += nBytes;
      nChar = dst - kbuf;
    }
  read_unlock(&pablo);

    /* Transfer data from the kernel to userspace */  
  if (copy_to_user(buf, kbuf,nChar)){
    printk("modList: couldnt copy to user");
    return -EINVAL;
    
  }
    
  (*off)+=len;  /* Update the file pointer */

  return nChar;
   
}

static const struct proc_ops proc_entry_fops = {
    .proc_open = modlist_open,
    .proc_release = modlist_release,
    .proc_read = modlist_read,
    .proc_write = modlist_write,    
};



int init_modlist_module( void )
{
  int ret = 0;
  
  INIT_LIST_HEAD(&mylist);

  proc_entry = proc_create( "modList", 0666, NULL, &proc_entry_fops);
  if (proc_entry == NULL) {
    ret = -ENOMEM;
    printk(KERN_INFO "modList: Can't create /proc entry\n");
  } else {
    printk(KERN_INFO "modList: Module loaded\n");
  }

  return ret;

}


void exit_modlist_module( void )
{

  struct list_item* item = NULL;
  struct list_head* cur_node = 0;
  struct list_head* tmp = 0;
  
  
  remove_proc_entry("modList", NULL);
  
  list_for_each_safe(cur_node,tmp,&mylist){
    item = list_entry(cur_node, struct list_item, links);
    list_del(cur_node);
    kfree(item);
  }
  
  printk(KERN_INFO "modList: Module unloaded.\n");
}


module_init( init_modlist_module );
module_exit( exit_modlist_module );
