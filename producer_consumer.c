#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/timekeeping.h>
#include <linux/sched/signal.h>

MODULE_LICENSE("GPL");

static int uid = 1;
static int buff_size = 1;
static int p = 0;
static int c = 0;
static int Onexit = 0;

static long totalSec = 0;

struct task_struct **consumerThreadList;
struct task_struct *producerThread;
struct semaphore mutex, full, empty;
struct check {
    int flag;
    int itemNo;
    struct task_struct item;
};

module_param(uid, int, 1);
module_param(buff_size, int, 1);
module_param(p, int, 0);
module_param(c, int, 0);

static size_t process_counter = 0;
static struct check* buffer = NULL;

static struct task_struct readBuffer(int *index, int* itemNo){
    int i=0;
    int minItemNo = __INT32_MAX__;
    int minItemNoIndex = -1;
    for(; i < buff_size; i++){
        if(buffer[i].flag == 1){
            if (buffer[i].itemNo < minItemNo) {
                minItemNoIndex = i;
                minItemNo = buffer[i].itemNo;
            }
        }
    }
    buffer[minItemNoIndex].flag = 0;
    *index = minItemNoIndex;
    *itemNo = buffer[minItemNoIndex].itemNo;
    return buffer[minItemNoIndex].item;
}

static int writeBuffer(struct task_struct product, int counterNum){
    int i=0;
    for(; i<buff_size;i++){
        if (buffer[i].flag == 0){
            buffer[i].item = product;
            buffer[i].itemNo = counterNum;
            buffer[i].flag = 1;
            return i;
        }
    }
    return -1;
}

int producer(void* arg){
    struct task_struct *task;
    for_each_process(task) {
        long pid = task->pid;
        long task_uid = task->cred->uid.val;

        if(kthread_should_stop()){
            do_exit(0);
        }

        if(task_uid == uid) {
            ++process_counter;
            if(down_interruptible(&empty)){
                 break;
            }
            if(kthread_should_stop()){
            do_exit(0);
        }
            if(down_interruptible(&mutex)){
                break;
            }
            if(kthread_should_stop()){
            do_exit(0);
        }
            printk(KERN_INFO "[Producer-%d]Produced Item#-%d at buffer index:%d for PID:%d",p,process_counter,writeBuffer(*task, process_counter), pid);
            up(&full);
            up(&mutex);
        }
    }
    return 0;
}

static int consumer (void *id){
    int pointer = *(int *)id;
    while(!kthread_should_stop()){
        int index;
        int itemNo;
        if(down_interruptible(&full)){
            break;
        }
        if(kthread_should_stop()){
            do_exit(0);
        }

        if(down_interruptible(&mutex)){
            break;
        }
        if(kthread_should_stop()){
            do_exit(0);
        }
        struct task_struct temp = readBuffer(&index, &itemNo);

        long ns = ktime_get_ns() - temp.start_time;
        int s = ns / 1000000000;
        totalSec += ns;
        int Fs = s % 60;
        int m = s / 60;
        int Fm = m % 60;
        int h = s / 3600;

        printk(KERN_INFO "[Consumer-%d] Consumed Item#-%d on buffer index: %d PID:%d Elapsed Time- %d:%d:%d",id, itemNo ,index,temp.pid,h,Fm,Fs);

        up(&empty);
        up(&mutex);

    }
    return 0;
}


static int init(void){

    buffer = (struct check*) kmalloc((sizeof(struct check)) * buff_size,GFP_KERNEL);
    sema_init(&empty, buff_size);
    sema_init(&mutex, 1);
    sema_init(&full, 0);
    consumerThreadList = (struct task_struct**) kmalloc((sizeof(struct task_struct*))*c,GFP_KERNEL) ;
    if(p == 1){
        producerThread = kthread_run(producer, NULL, "producer-1");
    }

    
    int i =0;
    for(; i < c;i++){
        consumerThreadList[i] = kthread_run(consumer,i,"consumer-%d",i);
    }

    return 0;
}

static void Exit(void){
    printk(KERN_INFO "Finish\n");
    up(&empty);
    int i = 0;
    for(; i < c;i++){
        up(&mutex);
        up(&full);
        printk(KERN_INFO "rm consumer thread %d", i);
    }
    totalSec = totalSec / 1000000000;
    int Fs = totalSec % 60;

    int m = totalSec / 60;
    int Fm = m % 60;

    int h = totalSec / 3600;

    printk(KERN_INFO "The Total elapsed time of all processes for UID %d is %d:%d:%d",uid,h,Fm,Fs);
    
    printk(KERN_INFO "mod exit\n");
}

module_init(init);
module_exit(Exit);