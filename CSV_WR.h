#pragma once
#include <stdlib.h> /*标准库头文件*/
#include <stdio.h>  /*标准库头文件*/
#include <string.h> /*标准库头文件*/
#include <errno.h>  /*标准库头文件*/
#include "FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <dirent.h>           /*POSIX标准的目录项结构头文件，用于遍历目录*/
#include <sys/stat.h>         /*POSIX标准的文件状态头文件，用于获取文件状态信息，例如文件是否存在、权限等*/
#include "esp_vfs.h"          /*ESP-IDF虚拟文件系统（VFS）的API。VFS允许在不同的存储介质使用统一的接口*/
#include "esp_vfs_fat.h"      /*ESP-IDF中FAT文件系统相关的API。这个库提供了与FAT文件系统交互的接口*/
#include "esp_system.h"       /*ESP-IDF系统级别的功能，如重启、关机、内存管理、任务管理等高级系统服务*/
#include "esp_partition.h"    /*ESP-IDF框架提供的头文件，用于操作ESP32的分区表*/
#include "esp_check.h"        /*ESP-IDF框架提供的头文件，用于执行断言检查*/
#include "tinyusb.h"          /*TinyUSB库的主要头文件，提供了USB设备的通用接口*/
#include "tusb_msc_storage.h" /*TinyUSB库的一部分，专门用于实现USB大容量存储设备(MSC)的功能，允许设备被识别为U盘*/
#include "tusb_cdc_acm.h"     /*TinyUSB库的一部分，用于实现USB通信设备类抽象控制模型(CDC ACM)，实现类似于串口通信的功能*/

#define EXAMPLE_MAX_CHAR_SIZE 128
#define MAX_LINE_LENGTH 1024

typedef struct
{
    char name[100];        /* 样品名称2 */
    char id[30];           /* 样品编号 */
    char ProjectName[100]; /* 检测项目 */
    char absorbance[30];   /* 吸光度 */
    char content[30];      /* 消毒液含量3 */
    char Date[30];         /* 检测日期时间1 */

    /* 以下五项一般为空 */
    char inspector[20];       /* 检测人 */
    char TestingUnit[20];     /* 检测单位 */
    char TestingLocation[20]; /* 检测地点 */
    char MerchantName[20];    /* 商户名称 */
    char Telephone[20];       /* 联系电话 */
} data_info_t;

typedef struct
{
    data_info_t *arr_infos;
    int size; /* 不包含第一行(标题行) */
} csv_info_t;

/************************************************************************************
 * @brief 初始化CsvMutex
 * @param 无
 * @return 无
 ***********************************************************************************/
void csvFile_xMutexInit(void);

/************************************************************************************
 * @brief 写入一行(结构体)数据到csv文件文件路径,
 * @param file_path, @param data_info 结构体指针
 * @return 无
 ***********************************************************************************/
void Csv_write_file(const char *file_path, data_info_t *data_info); //(写操作最终版)

/************************************************************************************
 * @brief 遍历读取csv文件中的数据,使用后请调用free_csv_info()释放内存
 * @param file_path 文件路径
 * @return 无
 ***********************************************************************************/
csv_info_t *get_csv_info(const char *file_path); //(读操作最终版)
void Csv_read_file3(const char *file_path);      //(测试用)

/************************************************************************************
 * @brief 释放csv文件结构体和结构体数组的内存
 * @param csv_info 结构体指针
 * @return 无
 ***********************************************************************************/
void free_csv_info(csv_info_t *csv_info);

/************************************************************************************
 * @brief 删除csv文件中的一行数据
 * @param file_path 文件路径, @param lineNumberToDelete 待删除行号
 * @return 无
 ***********************************************************************************/
void Csv_del_file3(const char *file_path, uint32_t lineNumberToDelete); //(删操作最终版)

/* 读写csv文件的demo */
void FATFS_csv_wrDemo(void);