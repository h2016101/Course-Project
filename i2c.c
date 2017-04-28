/*This is Device Driver to interface VCNL 4010 with Raspberry pi and read the
 *measurements from VCNL 4010 Proximity sensor using I2C Bus.
 *Design Project for Device drivers EEE G547 ,BITS PILANI
 *Sahil Chandna 2016H140101P   
 *The driver is written on lines with i2c_dev.c */  

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/notifier.h> 
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/spinlock.h>
 
static unsigned int gpioLed =17;//Pin 11 of Raspberry pi
bool ledOn;     
bool detectBit;
//sensor enable register
#define REG 0x80
#define DATA 0xFF
//proximity rate set
#define REG1 0x82
#define DATA1 0x00
#define REG2 0x84
#define DATA2 0x9D
#define DATA3 0x0A //Threshold value for Interrupt status Check MSB
#define DATA4 0xFF //Threshold value for Interrupt status Check LSB
static dev_t first;//device number
 static int NUM; //major number

#define I2C_MINORS	MINORMASK

DEFINE_RWLOCK(my_lock);//read write spinlock created

/*this is structure attached to bus*/
struct i2c_dev {
	struct list_head list;
	struct i2c_adapter *adap;
	struct device *dev;
	struct cdev cdev;
};


static LIST_HEAD(i2c_dev_list);

//check for existing i2c device on bus using minor number
static struct i2c_dev *i2c_dev_get_by_minor(unsigned index)
{
	struct i2c_dev *i2c_dev;

	
	list_for_each_entry(i2c_dev, &i2c_dev_list, list) 
	{
		if (index==1)
		{printk(KERN_INFO"the slave was found");
			goto found;
}
	}
	i2c_dev = NULL;
found:
	
	return i2c_dev;
}

//add a device 
static struct i2c_dev *get_free_i2c_dev(struct i2c_adapter *adap)
{
	struct i2c_dev *i2c_dev;

	if (adap->nr >= I2C_MINORS) {
		printk(KERN_ERR "i2c-dev: Out of device minors (%d)\n",
		       adap->nr);
		return ERR_PTR(-ENODEV);
	}

	i2c_dev = kzalloc(sizeof(*i2c_dev), GFP_KERNEL);
	if (!i2c_dev)
		return ERR_PTR(-ENOMEM);
	i2c_dev->adap = adap;

	list_add_tail(&i2c_dev->list, &i2c_dev_list);
	return i2c_dev;
}

//delete device
static void put_i2c_dev(struct i2c_dev *i2c_dev)
{
	list_del(&i2c_dev->list);
	kfree(i2c_dev);
}


/*read function*/
static ssize_t myread(struct file *file, char __user *buf, size_t count,
		loff_t *offset)
{
	char *tmp;
	int ret;
	int i=256;
	char x;
	int k;
	int w;
	/*the client handle points to file Descriptor data field*/
	
	struct i2c_client *client = file->private_data;
	
	tmp = (char *)kmalloc(count, GFP_KERNEL);
	
	if (tmp == NULL)
		return -ENOMEM;
	read_lock(&my_lock);//aquire read lock
	printk(KERN_INFO"INSIDE THE READ SPINLOCK"); 
	ret = i2c_master_recv(client, tmp, count);
	//read the interrupt status register
	w=i2c_smbus_read_byte_data(client,0x8E);
	printk("Interrupt status is %d",w);	
	read_unlock(&my_lock);//release read lock
	printk(KERN_INFO"RELEASED THE READ SPINLOCK"); 
	
	if (ret >= 0)
		ret = copy_to_user(buf, tmp, count) ? -EFAULT : ret;
	x = *(buf+2);	
	k= (x*i)+ *(buf+3);
	printk(KERN_INFO"the Proximity reading is %d",k);
	//indication led logic	
	if (k>5000)
		{
		 	gpio_set_value(gpioLed, 1);//set led on
	
		}
	else
		{
			gpio_set_value(gpioLed,0);
		}
		

	
	// Generate and Read Interrupt Status
	/*interrupt set to High THreshhold VALUE
	 * using (DATA3 *256)+DATA4
	 * current threshold 2815*/
	 write_lock(&my_lock); 
	i2c_smbus_write_byte_data(client,0x89,0x02); 
	i2c_smbus_write_byte_data(client,0x8C,DATA3); 
	i2c_smbus_write_byte_data(client,0x8D,DATA4); 
	write_unlock(&my_lock);		
	kfree(tmp);
	return ret;
}


/*IOCTL Function call used here to set the
 *slave address and for write functionality*/	
static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = file->private_data;
	
	switch (cmd) 
	{
		case I2C_SLAVE:

			client->addr = arg;
			write_lock(&my_lock);
			i2c_smbus_write_byte_data(client,REG,DATA); 
			i2c_smbus_write_byte_data(client,REG1,DATA1); 
			i2c_smbus_write_byte_data(client,REG2,DATA2);
			printk(KERN_INFO"INSIDE THE write SPINLOCK");
			write_unlock(&my_lock);
			printk(KERN_INFO"Released write SPINLOCK"); 
		

			break;

		default:
		
			 printk(KERN_INFO"Check ioctl user space");
			return -ENOTTY;
	}

	return 0;
}



static int device_open(struct inode *inode, struct file *file)
{
	unsigned int minor = iminor(inode);
	struct i2c_client *client;
	struct i2c_adapter *adap;

	adap = i2c_get_adapter(minor);
	if (!adap)
		return -ENODEV;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client) {
		i2c_put_adapter(adap);
		return -ENOMEM;
	}
	snprintf(client->name, I2C_NAME_SIZE, "i2c-dev %d", adap->nr);

	client->adapter = adap;
	file->private_data = client;
	

	return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
	struct i2c_client *client = file->private_data;

	i2c_put_adapter(client->adapter);
	kfree(client);
	file->private_data = NULL;

	return 0;
}

static const struct file_operations i2cdev_fops = {
	.owner		= THIS_MODULE,
	.read		= myread,
	.unlocked_ioctl	= my_ioctl,
	.open		= device_open,
	.release	= device_release,
};

/* ------------------------------------------------------------------------- */

static struct class *i2c_dev_class;

// this function adds a device to existing list and also initiate the device

static int i2cdev_attach_adapter(struct device *dev, void *dummy)
{
	struct i2c_adapter *adap;
	struct i2c_dev *i2c_dev;
	int res;

	if (dev->type != &i2c_adapter_type)
		return 0;
	adap = to_i2c_adapter(dev);

	i2c_dev = get_free_i2c_dev(adap);
	if (IS_ERR(i2c_dev))
		return PTR_ERR(i2c_dev);

	cdev_init(&i2c_dev->cdev, &i2cdev_fops);
	i2c_dev->cdev.owner = THIS_MODULE;
	res = cdev_add(&i2c_dev->cdev, MKDEV(NUM, adap->nr), 1);
	if (res)
		goto error_cdev;

	/* register this i2c device with the driver core */
	i2c_dev->dev = device_create(i2c_dev_class, &adap->dev,
				     MKDEV(NUM, adap->nr), NULL,
				     "vcnl4010");
	if (IS_ERR(i2c_dev->dev)) {
		res = PTR_ERR(i2c_dev->dev);
		goto error;
	}


		 
	return 0;
error:
	cdev_del(&i2c_dev->cdev);
error_cdev:
	put_i2c_dev(i2c_dev);
	return res;
}

//remove the device on bus
static int i2cdev_detach_adapter(struct device *dev, void *dummy)
{
	struct i2c_adapter *adap;
	struct i2c_dev *i2c_dev;

	if (dev->type != &i2c_adapter_type)
		return 0;
	adap = to_i2c_adapter(dev);

	i2c_dev = i2c_dev_get_by_minor(adap->nr);
	if (!i2c_dev) /* attach_adapter must have failed */
		return 0;

	cdev_del(&i2c_dev->cdev);
	put_i2c_dev(i2c_dev);
	device_destroy(i2c_dev_class, MKDEV(NUM, adap->nr));

	
	return 0;
}

static int i2cdev_notifier_call(struct notifier_block *nb, unsigned long action,
			 void *data)
{
	struct device *dev = data;

	switch (action) {
	case BUS_NOTIFY_ADD_DEVICE:
		return i2cdev_attach_adapter(dev, NULL);
	case BUS_NOTIFY_DEL_DEVICE:
		return i2cdev_detach_adapter(dev, NULL);
	}

	return 0;
}

static struct notifier_block i2cdev_notifier = {
	.notifier_call = i2cdev_notifier_call,
};



static int __init prox_init(void)
{
	int res;
    

	printk(KERN_INFO "i2cdevice driver init function\n");
	res=alloc_chrdev_region(&first,0,I2C_MINORS,"PROX");
	NUM=MAJOR(first);

	if (res)
		{
		printk(KERN_INFO"FAILING");
		goto out;
		}

	i2c_dev_class = class_create(THIS_MODULE, "i2c-class");
	if (IS_ERR(i2c_dev_class))
		{
			res = PTR_ERR(i2c_dev_class);
			goto out_unreg_chrdev;
		}
	// this function add all notification to blocking chain register
	
	res = bus_register_notifier(&i2c_bus_type, &i2cdev_notifier);
	if (res)
		goto out_unreg_class;
	//attach the device to adapter	
	i2c_for_each_dev(NULL, i2cdev_attach_adapter);
	
	
	if (!gpio_is_valid(gpioLed))
      	{
		printk(KERN_INFO "GPIO_TEST: invalid LED GPIO\n");
		}

 	gpio_request(gpioLed, "sysfs");          // request LED GPIO
   	gpio_direction_output(gpioLed, 1);   // set in output mode and on
   	gpio_export(gpioLed, false);  
	return 0;

	out_unreg_class:
		class_destroy(i2c_dev_class);
	out_unreg_chrdev:
		unregister_chrdev_region(MKDEV(NUM, 0), I2C_MINORS);
	out:
		printk(KERN_ERR "%s: Driver Initialisation failed\n", __FILE__);
		return res;
}

static void __exit prox_exit(void)
{
	
	bus_unregister_notifier(&i2c_bus_type, &i2cdev_notifier);
	i2c_for_each_dev(NULL, i2cdev_detach_adapter);
	class_destroy(i2c_dev_class);
	unregister_chrdev_region(MKDEV(NUM, 0), I2C_MINORS);
	 gpio_unexport(gpioLed);
	 gpio_free(gpioLed) ;
}

MODULE_AUTHOR("Sahil");
MODULE_DESCRIPTION("I2C driver for vcnl 4010");
MODULE_LICENSE("GPL");

module_init(prox_init);
module_exit(prox_exit);
