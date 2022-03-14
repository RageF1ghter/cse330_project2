#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/semaphore.h>
#include <linux/timekeeping.h>

MODULE_AUTHOR("zy");
MODULE_DESCRIPTION("LKM");
MODULE_LICENSE("GPL");

static struct task_struct *thread1;
static struct task_struct *thread2;
static int counter;
module_param(counter, int, 0);

static struct semaphore empty, full, mutex;

struct bufferEnrty {
	struct task_struct *task;
	int index;
	int count;
} buffer [5];

//struct bufferEntry buffer[5];

static int producer(void *args){
	printk(KERN_INFO "In producer kernel");	
	sema_init(&empty, 5);
	sema_init(&full, 0);


	struct task_struct *task_list;	
	for_each_process(task_list){
		++counter;
		//printk(KERN_INFO "Empety before: %d", empty.count);
		if(down_interruptible(&empty)){
			break;
		}
		//printk(KERN_INFO "Empety after: %d", empty.count);	

		struct bufferEnrty item;
		item.task = task_list;
		item.index = full.count;
		item.count = counter;
		printk(KERN_INFO "[Producer-1] Produced Item#-%d at buffer index:%d for PID:%d"
		,counter, item.index, item.task->pid);
		buffer[full.count] = item;


		//printk(KERN_INFO "full before up: %d", full.count);
		up(&full);		
		//printk(KERN_INFO "full after up: %d", full.count);

	}
	return 0;
}

static int consumer(void *args){
	printk(KERN_INFO "In consumer thread");
	while(!kthread_should_stop()){
		if(down_interruptible(&full)){
			break;
		}
			
		struct bufferEnrty item;
		if(full.count == 4){
			up(&full);
		}
		item = buffer[full.count];		
		int time;
		time = ktime_get_ns() - item.task->start_time;
		time = time / 1000000000;
		printk(KERN_INFO "[Consumer-1] Consumed Item#-%d on buffer index:%d PID:%d Elapsed Time-%d"
		, item.count, item.index, item.task->pid, time);
		
		up(&empty);		
	}

	return 0;
}



int thread_init(void){
	printk(KERN_INFO "in init");
	
	thread1 = kthread_run(producer, NULL, "thread1");
	thread2 = kthread_run(consumer, NULL, "thread2");
	return 0;
}	

void thread_cleanup(void){
	int ret1;
	int ret2;
	ret1 = kthread_stop(thread1);
	ret2 = kthread_stop(thread2);
	if(!ret1 && !ret2){
		printk(KERN_INFO "thread stopped");
	}
}
	

module_init(thread_init);
module_exit(thread_cleanup);