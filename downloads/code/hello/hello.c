#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <asm/uaccess.h>

#include <linux/cdev.h>    
#include <linux/semaphore.h>

//drivers/hello/hello.c:303:5: error: implicit declaration of function 'kmalloc' [-Werror=implicit-function-declaration]
#include <linux/slab.h>
#include <linux/seq_file.h>

#include "hello.h"

//主设备和从设备号变量
static int hello_major = 0;
static int hello_minor = 0;

//设备类别和设备变量
static struct class* hello_class = NULL;
static struct hello_android_dev* hello_dev = NULL;

//访问设置属性的方法
static ssize_t hello_val_show(struct device* dev, struct device_attribute* attr, char* buf);
static ssize_t hello_val_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count); 

//定义设备属性
static DEVICE_ATTR(val, S_IRUGO | S_IWUSR, hello_val_show, hello_val_store);

//传统设备文件操作方法
static int hello_open(struct inode* inode, struct file* filp);
static int hello_release(struct inode* inode, struct file* filp);
static ssize_t hello_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos);
static ssize_t hello_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos);

//设备文件操作方法表
static struct file_operations hello_fops = 
{
     .owner = THIS_MODULE,
     .open = hello_open,
     .release = hello_release,
     .read = hello_read,
     .write = hello_write,
};

//打开设备方法
static int hello_open(struct inode* inode, struct file* filp) 
{
    struct hello_android_dev* dev;
    
    //将自定义设备结构体保存在文件指针的私有数据域中，以便访问设备时拿来用
    dev = container_of(inode->i_cdev, struct hello_android_dev, dev);
    filp->private_data = dev;
    return 0;
}

//设备文件释放时调用，空实现
static int hello_release(struct inode* inode, struct file* filp) 
{
  return 0;
}

//读取设备的寄存器val的值
static ssize_t hello_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos)
{
      printk("hello_read()\n");
      
    ssize_t err = 0;
    struct hello_android_dev* dev = filp->private_data;
    
    //同步访问
    if(down_interruptible(&(dev->sem))) 
    {
        return -ERESTARTSYS;
    }

    if(count < sizeof(dev->val)) 
    {
        goto out;
    }

    //将寄存器val的值拷贝到用户提供的缓冲区
    if(copy_to_user(buf, &(dev->val), sizeof(dev->val))) 
    {
         err = -EFAULT;
         goto out;
     }

     err = sizeof(dev->val);

out:
    up(&(dev->sem));
    return err;
}

//写设备寄存器的值val
static ssize_t hello_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos) 
{
    struct hello_android_dev* dev = filp->private_data;
    ssize_t err = 0;
    
    printk("hello_write()\n");

    //同步访问
    if(down_interruptible(&(dev->sem))) {
        return -ERESTARTSYS;
    }

    if(count != sizeof(dev->val)) {
        goto out;
    }

    //将用户提供的缓冲区的值写到设备寄存器去
    if(copy_from_user(&(dev->val), buf, count)) {
        err = -EFAULT;
        goto out;
    }
    err = sizeof(dev->val);
out:
    up(&(dev->sem));
    return err;
}

//读取寄存器val的值到缓冲区的buf中，内部使用
static ssize_t __hello_get_val(struct hello_android_dev* dev, char* buf) 
{
      printk("__hello_get_val()\n");
    
    int val = 0;
    
    //同步访问
    struct semaphore *p_sem = &(dev->sem);
    if(down_interruptible(p_sem))
    {
          printk("sem is used,pls wait to free.\n");
        return -ERESTARTSYS;
    }

    val = dev->val;
    up(p_sem);
    
    printk("get val: %d\n",val);

    return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

//把缓冲区buf的值写到设备寄存器val中去，内部使用
static ssize_t __hello_set_val(struct hello_android_dev* dev, const char* buf, size_t count) 
{
      printk("__hello_set_val()\n");
    
    int val = 0;

    //将字符串转换成数字
    val = simple_strtol(buf, NULL, 10);
    
    printk("set val: %d\n",val);

    //同步访问
    if(down_interruptible(&(dev->sem)))
    {
        return -ERESTARTSYS;
    }
    
    dev->val = val;
    up(&(dev->sem));

    return count;
}

//读取设备属性
static ssize_t hello_val_show(struct device* dev, struct device_attribute* attr, char* buf)
{
      printk("hello_val_show()\n");
    
    struct hello_android_dev* hdev = (struct hello_android_dev*)dev_get_drvdata(dev);

    return __hello_get_val(hdev, buf);
}

//写设备属性
static ssize_t hello_val_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) 
{
      printk("hello_val_store()\n");
    
    struct hello_android_dev* hdev = (struct hello_android_dev*)dev_get_drvdata(dev);

    return __hello_set_val(hdev, buf, count);
}

static int hello_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", hello_dev->val);
 
    return 0;
}
 
 
static int hello_proc_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, hello_proc_show, NULL);
}

//读取设备寄存器val的值，保存在page缓冲区中
static ssize_t hello_proc_read(char* page, char** start, off_t off, int count, int* eof, void* data) 
{    
      printk("hello_proc_read()\n");
    
    if(off > 0) 
    {
        *eof = 1;
        return 0;
    }
    return __hello_get_val(hello_dev, page);
}

//把缓冲区的值buff保存到设备寄存器val中去
static ssize_t hello_proc_write(struct file* filp, const char __user *buff, unsigned long len, void* data) 
{
      printk("hello_proc_write()\n");
     
    int err = 0;
    char* page = NULL;

    if(len > PAGE_SIZE) {
    printk(KERN_ALERT"The buff is too large: %lu./n", len);
        return -EFAULT;
    }

    page = (char*)__get_free_page(GFP_KERNEL);
    if(!page) {
        printk(KERN_ALERT"Failed to alloc page./n");
        return -ENOMEM;
    }

    //先把用户提供的缓冲区值拷贝到内核缓冲区中去
    if(copy_from_user(page, buff, len)) {
        printk(KERN_ALERT"Failed to copy buff from user./n");
    err = -EFAULT;
    goto out;
    }
    err = __hello_set_val(hello_dev, page, len);

out:
    free_page((unsigned long)page);
    return err;
}

//删除/proc/hello文件
static void hello_remove_proc(void) 
{
    remove_proc_entry(HELLO_DEVICE_PROC_NAME, NULL);
}

static const struct file_operations hello_proc_fops = {
	.owner	= THIS_MODULE,
	.open	= hello_proc_open,
	.write	= hello_proc_write,
	.read	= seq_read,
	.llseek	= seq_lseek,
	.release= single_release,
};

//创建/proc/hello文件
static void hello_create_proc(void)
{
//    struct proc_dir_entry* entry;
//
//    entry = create_proc_entry(HELLO_DEVICE_PROC_NAME, 0, NULL);
//    if(entry)
//    {
//        //entry->owner = THIS_MODULE;  entry has no "owner" member
//        entry->read_proc = hello_proc_read;
//        entry->write_proc = hello_proc_write;
//    }
    proc_create(HELLO_DEVICE_PROC_NAME, 0, NULL, &hello_proc_fops);
}


//初始化设备
static int __hello_setup_dev(struct hello_android_dev* dev) 
{
    int err;
    dev_t devno = MKDEV(hello_major, hello_minor);
    
    printk("__hello_setup_dev()\n");

    memset(dev, 0, sizeof(struct hello_android_dev));

    cdev_init(&(dev->dev), &hello_fops);
    dev->dev.owner = THIS_MODULE;
    dev->dev.ops = &hello_fops;

    //注册字符设备
    err = cdev_add(&(dev->dev),devno, 1);
    if(err) 
    {
        return err;
    }

    //初始化信号量和寄存器val的值
    //init_MUTEX(&(dev->sem));  2.6.25及以后的linux内核版本废除了init_MUTEX函数
    sema_init(&(dev->sem),1);
    dev->val = 0;
    return 0; 
}

//模块加载方法
static int __init hello_init(void)
{
    int err = -1;
    dev_t dev = 0;
    struct device* temp = NULL;

    printk(KERN_ALERT"Initializing hello device./n");
    /*动态分配主设备和从设备号*/    
    err = alloc_chrdev_region(&dev, 0, 1, HELLO_DEVICE_NODE_NAME);
    if(err < 0) {
        printk(KERN_ALERT"Failed to alloc char dev region./n");
        goto fail;
    }

    hello_major = MAJOR(dev);
    hello_minor = MINOR(dev);  
    
    //分配hello设备结构体变量
    hello_dev = kmalloc(sizeof(struct hello_android_dev), GFP_KERNEL);
    if(!hello_dev) {
        err = -ENOMEM;
        printk(KERN_ALERT"Failed to alloc hello_dev./n");
        goto unregister;
    }

    //初始化设备
    err = __hello_setup_dev(hello_dev);
    if(err) {
        printk(KERN_ALERT"Failed to setup dev: %d./n", err);
        goto cleanup;
    }

    //在/sys/class/目录下创建设备类别目录hello
    hello_class = class_create(THIS_MODULE, HELLO_DEVICE_CLASS_NAME);
    if(IS_ERR(hello_class)) {
        err = PTR_ERR(hello_class);
        printk(KERN_ALERT"Failed to create hello class./n");
        goto destroy_cdev;
    }

    //在/dev目录和/sys/class/hello目录下分别创建设备文件hello
    temp = device_create(hello_class, NULL, dev, "%s", HELLO_DEVICE_FILE_NAME);
    if(IS_ERR(temp)) {
        err = PTR_ERR(temp);
         printk(KERN_ALERT"Failed to create hello device.");
         goto destroy_class;
    }

    //在/sys/class/hello/hello目录下创建属性文件val
    err = device_create_file(temp, &dev_attr_val);
    if(err < 0) {
        printk(KERN_ALERT"Failed to create attribute val.");
        goto destroy_device;
    }

    dev_set_drvdata(temp, hello_dev);

    //创建/proc/hello文件
    hello_create_proc();
    printk(KERN_ALERT"Succedded to initialize hello device./n");
    return 0;

destroy_device:
    device_destroy(hello_class, dev);
destroy_class:
    class_destroy(hello_class);

destroy_cdev:
    cdev_del(&(hello_dev->dev));
cleanup:
    kfree(hello_dev);

unregister:
    unregister_chrdev_region(MKDEV(hello_major, hello_minor), 1);

fail:
    return err;
}

//模块卸载方法
static void __exit hello_exit(void) 
{
    dev_t devno = MKDEV(hello_major, hello_minor);

    printk(KERN_ALERT"Destroy hello device./n");

    //删除/proc/hello文件
    hello_remove_proc();

    //销毁设备类别和设备
    if(hello_class) {
        device_destroy(hello_class, MKDEV(hello_major, hello_minor));
        class_destroy(hello_class);
    }

    //删除字符设备和释放设备内存
    if(hello_dev) {
        cdev_del(&(hello_dev->dev));
        kfree(hello_dev);
    }

    //释放设备号
    unregister_chrdev_region(devno, 1);
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("First Android Driver");

module_init(hello_init);
module_exit(hello_exit);
