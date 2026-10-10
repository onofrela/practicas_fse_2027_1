#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/uaccess.h>

static int aperturas;
static DEFINE_MUTEX(candado);

static int p6_open(struct inode *inode, struct file *f)
{
	int a;

	mutex_lock(&candado);
	a = ++aperturas;
	mutex_unlock(&candado);

	pr_info("open: %d\n", a);
	return 0;
}

static ssize_t p6_read(struct file *f, char __user *dst, size_t n, loff_t *pos)
{
	char kbuf[32];
	int len;

	mutex_lock(&candado);
	len = scnprintf(kbuf, sizeof(kbuf), "aperturas: %d\n", aperturas);
	mutex_unlock(&candado);

	return simple_read_from_buffer(dst, n, pos, kbuf, len);
}

static ssize_t p6_write(struct file *f, const char __user *src, size_t n, loff_t *pos)
{
	char tmp[16];

	if (n >= sizeof(tmp))
		return -EINVAL;

	if (copy_from_user(tmp, src, n))
		return -EFAULT;

	tmp[n] = '\0';

	if ((n == 5 && !memcmp(tmp, "reset", 5)) ||
	    (n == 6 && tmp[5] == '\n' && !memcmp(tmp, "reset", 5))) {
		mutex_lock(&candado);
		aperturas = 0;
		mutex_unlock(&candado);
		pr_info("reset\n");
		return n;
	}

	return -EINVAL;
}

static const struct file_operations p6_fops = {
	.owner = THIS_MODULE,
	.open = p6_open,
	.read = p6_read,
	.write = p6_write,
	.llseek = default_llseek,
};

static struct miscdevice p6_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "p6cnt",
	.fops = &p6_fops,
	.mode = 0666,
};

static int __init p6_init(void)
{
	int rc = misc_register(&p6_dev);

	if (rc) {
		pr_err("misc_register fallo: %d\n", rc);
		return rc;
	}

	pr_info("/dev/p6cnt creado\n");
	return 0;
}

static void __exit p6_exit(void)
{
	misc_deregister(&p6_dev);
	pr_info("total aperturas: %d\n", aperturas);
}

module_init(p6_init);
module_exit(p6_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ciscopapus");
MODULE_DESCRIPTION("Practica 6 FSE: contador de aperturas");
