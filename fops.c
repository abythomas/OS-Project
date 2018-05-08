#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/time.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>

#include <asm/hardirq.h>

#include "main.h"
#include "fops.h"

int tym_currentime(struct seq_file *m, void *p)
{
	struct timeval tv1;
	struct timespec tv2;
	unsigned long j1;
	u64 j2;

	PDEBUG("%s() is invoked\n", __FUNCTION__);

	j1 = jiffies;
	j2 = get_jiffies_64();
	do_gettimeofday(&tv1);
	tv2 = current_kernel_time();

	seq_printf(m ,"0x%08lx 0x%016Lx %10i.%06i\n"
		   "%41i.%09i\n",
		   j1, j2,
		   (int) tv1.tv_sec, (int) tv1.tv_usec,
		   (int) tv2.tv_sec, (int) tv2.tv_nsec);
	return 0;
}

int tym_fn(struct seq_file *m, void *p)
{
	unsigned long j0, j1; 
	wait_queue_head_t wait;
	extern int delay;

	PDEBUG("%s() is invoked\n", __FUNCTION__);

	init_waitqueue_head(&wait);

	j0 = jiffies;
	j1 = j0 + delay;

	switch((long)(m->private)) {
	case JIT_BUSY:
		while (time_before(jiffies, j1))
			cpu_relax();
		break;
	case JIT_SCHED:
		while (time_before(jiffies, j1))
			schedule();
		break;
	
	default:
		PDEBUG("Known option\n");
	}

	seq_printf(m, "%9li %9li\n", j0, j1);

	return 0;
}


struct tym_data {
	struct timer_list timer;
	struct tasklet_struct tlet;
	int hi;
	wait_queue_head_t wait;
	unsigned long prevjiffies;
	unsigned char *buf;
	int loops;
};

#define JIT_ASYNC_LOOPS 5

void tym_timer_fn(unsigned long arg)
{
	struct tym_data *data = (struct tym_data *)arg;
	unsigned long j = jiffies;

	PDEBUG("%s() is invoked\n", __FUNCTION__);

	data->buf += sprintf(data->buf, "%9li  %3li     %i    %6i   %i   %s\n",
			     j, j - data->prevjiffies, in_interrupt() ? 1 : 0,
			     current->pid, smp_processor_id(), current->comm);

	if (--data->loops) {
		data->timer.expires += tdelay;
		data->prevjiffies = j;
		add_timer(&data->timer);
	} else {
		wake_up_interruptible(&data->wait);
	}
}

int tym_timer(struct seq_file *m, void *p)
{
	extern int tdelay;
	struct tym_data *data;
	char *buf = NULL, *buf2 = NULL;
	unsigned long j = jiffies;
	int retval = 0;

	PDEBUG("%s() is invoked\n", __FUNCTION__);

	if (!(data = kmalloc(sizeof(struct tym_data), GFP_KERNEL)))
		return -ENOMEM;

	if (!(buf = kmalloc(PAGE_SIZE, GFP_KERNEL))) {
		retval = -ENOMEM;
		goto alloc_buf_error;
	}
	memset(buf, 0, PAGE_SIZE);
	buf2 = buf;

	init_timer(&data->timer);
	init_waitqueue_head(&data->wait);

	buf2 += sprintf(buf2, "   time   delta  inirq    pid   cpu command\n");
	buf2 += sprintf(buf2, "%9li  %3li     %i    %6i   %i   %s\n",
			j, 0L, in_interrupt() ? 1 : 0,
			current->pid, smp_processor_id(), current->comm);

	data->prevjiffies = j;
	data->buf = buf2;
	data->loops = JIT_ASYNC_LOOPS;

	data->timer.data = (unsigned long)data;
	data->timer.function = tym_timer_fn;
	data->timer.expires = j + tdelay;
	add_timer(&data->timer);

	wait_event_interruptible(data->wait, !data->loops);

	if (signal_pending(current)) {
		retval = -ERESTARTSYS;
		goto out;
	}

	seq_printf(m, "%s", buf);


out:
	kfree(buf);

alloc_buf_error:
	kfree(data);

	return retval;
}

void tym_tasklet_fn(unsigned long arg)
{
	struct tym_data *data = (struct tym_data *)arg;
	unsigned long j = jiffies;

	PDEBUG("%s() is invoked\n", __FUNCTION__);

	data->buf += sprintf(data->buf, "%9li  %3li     %i    %6i   %i   %s\n",
			     j, j - data->prevjiffies, in_interrupt() ? 1 : 0,
			     current->pid, smp_processor_id(), current->comm);

	if (--data->loops) {
		data->prevjiffies = j;
		if (data->hi)
			tasklet_hi_schedule(&data->tlet);
		else
			tasklet_schedule(&data->tlet);
	} else {
		wake_up_interruptible(&data->wait);
	}
}

int tym_tasklet(struct seq_file *m, void *p)
{
	struct tym_data *data;
	char *buf, *buf2;
	unsigned long j = jiffies;
	long hi = (long)(m->private);
	int retval = 0;

	PDEBUG("%s() is invoked\n", __FUNCTION__);

	if (!(data = kmalloc(sizeof(struct tym_data), GFP_KERNEL)))
		return -ENOMEM;

	if (!(buf = kmalloc(PAGE_SIZE, GFP_KERNEL))) {
		retval = -ENOMEM;
		goto alloc_buf_error;
	}
	memset(buf, 0, PAGE_SIZE);
	buf2 = buf;

	init_waitqueue_head(&data->wait);

	buf2 += sprintf(buf2, "   time   delta  inirq    pid   cpu command\n");
	buf2 += sprintf(buf2, "%9li  %3li     %i    %6i   %i   %s\n",
			j, 0L, in_interrupt() ? 1 : 0,
			current->pid, smp_processor_id(), current->comm);

	data->prevjiffies = j;
	data->buf = buf2;
	data->loops = JIT_ASYNC_LOOPS;

	tasklet_init(&data->tlet, tym_tasklet_fn, (unsigned long)data);
	data->hi = hi;
	if (hi)
		tasklet_hi_schedule(&data->tlet);
	else
		tasklet_schedule(&data->tlet);

	wait_event_interruptible(data->wait, !data->loops);

	if (signal_pending(current)) {
		goto out;
		retval = -ERESTARTSYS;
	}

	seq_printf(m, "%s", buf);

out:
	kfree(buf);

alloc_buf_error:
	kfree(data);

	return retval;
}
