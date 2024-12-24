#ifndef _HELLO_Android_H_
#define _HELLO_ANDROID_H_

#include <linux/cdev.h>
#include <linux/semaphore.h>
#define HELLO_DEVICE_NODE_NAME "hello"
#define HELLO_DEVICE_FILE_NAME "hello"
#define HELLO_DEVICE_PROC_NAME "hello"
#define HELLO_DEVICE_CLASS_NAME "hello"

/*定义一个设备结构体,此结构体可以封装设备相关的一些信息等
  信号量等也可以封装在此结构中，后续的设备模块一般都
  应该封装一个这样的结构体，但此结构体中必须包含某些
  成员，对于字符设备来说，我们必须包含struct cdev cdev*/
struct hello_android_dev
{
    int val;
    struct semaphore sem;
    struct cdev dev;
};

#endif
