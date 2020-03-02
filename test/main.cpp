//
//  main.cpp
//  hashSketch
//
//  Created by 熊斌 on 2020/1/9.
//  Copyright © 2020 熊斌. All rights reserved.
//

#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sched.h>
#include<unistd.h>
#include<pthread.h>
#include <iostream>
#include <fstream>
#include <string>
#include <assert.h>
#include <time.h>
using namespace std;



static int api_get_thread_policy (pthread_attr_t *attr)
{
    int policy;
    int rs = pthread_attr_getschedpolicy (attr, &policy);
    assert (rs == 0);

    switch (policy)
    {
        case SCHED_FIFO:
            printf ("policy = SCHED_FIFO\n");
            break;
        case SCHED_RR:
            printf ("policy = SCHED_RR");
            break;
        case SCHED_OTHER:
            printf ("policy = SCHED_OTHER\n");
            break;
        default:
            printf ("policy = UNKNOWN\n");
            break;
    }
    return policy;
}

static void api_show_thread_priority (pthread_attr_t *attr,int policy)
{
    int priority = sched_get_priority_max (policy);
    assert (priority != -1);
    printf ("max_priority = %d\n", priority);
    priority = sched_get_priority_min (policy);
    assert (priority != -1);
    printf ("min_priority = %d\n", priority);
}

static int api_get_thread_priority (pthread_attr_t *attr)
{
    struct sched_param param;
    int rs = pthread_attr_getschedparam (attr, &param);
    assert (rs == 0);
    printf ("priority = %d\n", param.sched_priority);
    return param.sched_priority;
}

static void api_set_thread_policy (pthread_attr_t *attr,int policy)
{
    int rs = pthread_attr_setschedpolicy (attr, policy);
    assert (rs == 0);
    struct sched_param param;
    rs = pthread_attr_getschedparam (attr, &param);
    param.sched_priority = 47;
    rs = pthread_attr_setschedparam(attr, &param);
    api_get_thread_policy (attr);
}
    
static void * myprocess(void * ptr){
    clock_t st_time = clock();
    long long cnt = 0;
    for(long long index = 0; index < 10000000000; index ++){
        if(index % 3  == 0){
            cnt ++;
        }
        else{
            cnt --;
        }
    }
    clock_t ed_time = clock();
    double real_time = ((double)(ed_time - st_time)) / CLOCKS_PER_SEC;
    cout<<"程序运行时间为 "<<real_time<<" 秒。"<<endl;
    return NULL;
}
int main(void)
{
    pthread_attr_t attr;       // 线程属性
    struct sched_param sched;  // 调度策略
    int rs;

    /*
     * 对线程属性初始化
     * 初始化完成以后，pthread_attr_t 结构所包含的结构体
     * 就是操作系统实现支持的所有线程属性的默认值
     */
    rs = pthread_attr_init (&attr);
    assert (rs == 0);     // 如果 rs 不等于 0，程序 abort() 退出

    
    
    pthread_t tid ;
    /*初始化属性值，均设为默认值*/
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_create(&tid, &attr, myprocess, NULL);
    
    pthread_join(tid, NULL);
    /*
     * 反初始化 pthread_attr_t 结构
     * 如果 pthread_attr_init 的实现对属性对象的内存空间是动态分配的，
     * phread_attr_destory 就会释放该内存空间
     */
    rs = pthread_attr_destroy (&attr);
    assert (rs == 0);

    return 0;
}
