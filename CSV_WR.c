#include "CSV_WR.h"

static const char *TAG = "csv_wr";
static const char *partition_path = "storage";    /* 挂载分区(盘符) */
static const char *base_path = "/disk";           /* 挂载路径(根目录) */
static const char *file_path = "/disk/test.csv";  /* csv文件路径 */
static wl_handle_t wl_handle = WL_INVALID_HANDLE; /* 定义磨损平衡库句柄 */
static SemaphoreHandle_t csv_xMutex = NULL;

/* 数据结构体模板 */
static data_info_t test_data = {
    .name = "玛卡巴卡",                      /* 样品名称2 */
    .id = "20240125144502",                  /* 样品编号 */
    .ProjectName = "过氧乙酸(800-3000mg/L)", /* 检测项目 */
    .absorbance = "0.298",                   /* 吸光度 */
    .content = "1555.675mg/L",               /* 消毒液含量3 */
    .Date = "2024-01-25 14:45:02",           /* 检测日期时间1 */

    /* 以下四项一般为空 */
    .inspector = "",       /* 检测人 */
    .TestingUnit = "",     /* 检测单位 */
    .TestingLocation = "", /* 检测地点 */
    .MerchantName = "",    /* 商户名称 */
    .Telephone = ""        /* 联系电话 */
};

/**
 * @brief 从给定的字符串str中删除双引号
 * @param str 要处理的字符串
 * @return 处理后的字符串
 * */
char *remove_quoted(char *str)
{
    int length = strlen(str);                  /* 计算字符串长度，不包含末尾的空字符 */
    char *result = (char *)malloc(length + 1); /* 为了容纳字符串本身加上结尾的空字符 */
    int index = 0;                             /* 初始化索引变量 index，用于跟踪新字符串中字符的F位置 */

    for (int i = 0; i < length; i++) /* 遍历输入字符串的每个字符 */
    {
        if (str[i] != '\"')
        {
            /* 若当前字符是双引号 */
            result[index] = str[i]; /* 将当前字符复制到结果字符串的相应位置，并递增 index */
            index++;                /* 递增索引 */
        }
    }
    result[index] = '\0'; /* 在结果字符串的末尾添加空字符，以标记字符串的结束 */
    return result;
}

/**
 * @brief 从给定的行line中提取第num个字段
 * @param line 要处理的行
 * @param num 要提取的字段编号
 * */
char *get_field(char *line, int num)
{
    char *tok;               /* 用于存储当前字段 */
    tok = strtok(line, ","); /* 用来存储通过 strtok 函数分割出的当前字段 */
    for (int i = 1; i != num; i++)
    {                            /* 它会跳过前 num-1 个字段，直到到达第 num 个字段 */
        tok = strtok(NULL, ","); /* 第一个参数为 NULL，表示继续在之前的位置进行分割 */
    }
    char *result = remove_quoted(tok); /* 移除可能存在的引号或其他不需要的字符 */
    free(line);                        /* 释放掉strdup函数分配的内存 */
    return result;
}

/**
 * @brief 打印单个结构体信息
 * @param info 结构体指针
 * @return 无
 * */
void print_info(data_info_t *info)
{
    printf("%s,%s,%s,%s,%s,%s\n", (char *)info->name, (char *)info->id, (char *)info->ProjectName,
           (char *)info->absorbance, (char *)info->content, (char *)info->Date);
}

/**
 * @brief 打印csv文件的所有结构体(不包含首行)
 * @param csv_info 结构体指针
 * @return 无
 * */
void print_csv_info(csv_info_t *csv_info)
{
    data_info_t *arr = csv_info->arr_infos;
    int size = csv_info->size;
    for (int i = 0; i < size; i++)
    {
        printf("%s,%s,%s,%s,%s,%s\n",
               (char *)arr[i].name, (char *)arr[i].id,
               (char *)arr[i].ProjectName, (char *)arr[i].absorbance,
               (char *)arr[i].content, (char *)arr[i].Date);
    }
    printf("csv info lines = %d\n", size);
}

/**
 * @brief 释放csv文件结构体和结构体数组的内存
 * @param csv_info 结构体指针
 * @return 无
 * */
void free_csv_info(csv_info_t *csv_info)
{
    if (csv_info != NULL)
    {
        free(csv_info->arr_infos);
        csv_info->arr_infos = NULL;
        free(csv_info);
    }
    else
    {
        ESP_LOGW(TAG, "No pointer needs to be freed !");
    }
}

/******************************************************************************
 * @brief 初始化CsvMutex
 * @param 无
 * @return 无 csvFile_xMutexInit
 *****************************************************************************/
void csvFile_xMutexInit(void)
{
    csv_xMutex = xSemaphoreCreateMutex();
    if (csv_xMutex == NULL)
    {
        ESP_LOGE(TAG, "csv_xMutex create failed");
        return;
    }
}

/******************************************************************************
 * @brief 检查文件是否存在
 * @param file_path 文件路径
 * @return 存在返回true，否则返回false
 *****************************************************************************/
static bool file_exists(const char *file_path)
{
    struct stat buffer; /* 声明文件状态结构体 */

    /* 使用stat函数判断文件是否存在，存在返回0，不存在返回-1 */
    return stat(file_path, &buffer) == 0;
}

/******************************************************************************
 * @brief 将传入的结构体数据写入到csv文件
 * @param file_path 文件路径, @param data_info 结构体指针
 * @return 无
 *****************************************************************************/
void Csv_write_file(const char *file_path, data_info_t *data_info)
{
    if (xSemaphoreTake(csv_xMutex, portMAX_DELAY) == pdTRUE)
    {
        ESP_LOGI(TAG, "csv_xMutex take successful");
    }
    // /* 检查目录是否存在，不存在则尝试创建 */
    // if (!file_exists(base_path))
    // {
    //     /* 使用0775权限创建目录，失败则记录错误日志 */
    //     if (mkdir(base_path, 0775) != 0)
    //     {
    //         ESP_LOGE(TAG, "mkdir failed with errno: %s", strerror(errno));
    //         xSemaphoreGive(csv_xMutex);
    //         return;
    //     }
    // }
    FILE *f = NULL;
    if (!file_exists(file_path)) /* 若文件不存在 */
    {
        ESP_LOGI(TAG, "Creating file...");
        f = fopen(file_path, "w");
        if (f == NULL)
        {
            ESP_LOGE(TAG, "Failed to open file for writing");
            xSemaphoreGive(csv_xMutex);
            return;
        }
        fprintf(f, "样品名称,样品编号,检测项目,吸光度,含量,检测日期时间,检测人,检测单位,检测地点,商户名称,联系电话\n");
        ESP_LOGI(TAG, "File init successful");
        fclose(f);
    }
    f = fopen(file_path, "a");
    fprintf(f, "%s,%s,%s,%s,%s,%s\n", data_info->name, data_info->id, data_info->ProjectName,
            data_info->absorbance, data_info->content, data_info->Date);

    ESP_LOGI(TAG, "Write a data entry to file");
    fclose(f);

    xSemaphoreGive(csv_xMutex);
    return;
}

/******************************************************************************
 * @brief 遍历读取csv文件中的数据
 * @param file_path 文件路径
 * @return 无
 *****************************************************************************/

csv_info_t *get_csv_info(const char *file_path) /* 正序遍历 */
{
    if (xSemaphoreTake(csv_xMutex, portMAX_DELAY) == pdTRUE) /* 获取互斥锁 */
    {
        ESP_LOGI(TAG, " csv_xMutex take successful ");
    }
    if (!file_exists(base_path) || !file_exists(file_path)) /* 若路径不存在，返回NULL */
    {
        ESP_LOGW(TAG, " base_path/file_path is not exist ");
        xSemaphoreGive(csv_xMutex);
        return NULL;
    }

    FILE *fp = fopen(file_path, "r");
    if (fp == NULL) /* 若文件打开失败，返回NULL */
    {
        ESP_LOGE(TAG, "Failed to open file: %s", file_path);
        xSemaphoreGive(csv_xMutex);
        return NULL;
    }

    char row[1024];                             /* 声明字符缓冲区(单行) */
    int num_lines = 0;                          /* 总行数，包含标题行 */
    while (fgets(row, sizeof(row), fp) != NULL) /* 获取总行数 */
    {
        num_lines++;
    }

    if (num_lines <= 1) /* 若除标题行外，不存在其他数据，返回 NULL */
    {
        ESP_LOGW(TAG, "The CSV file is empty.");
        fclose(fp);
        xSemaphoreGive(csv_xMutex);
        return NULL;
    }

    /* 为相应行数的结构体数组分配内存 */
    data_info_t *arr_infos = (data_info_t *)malloc(num_lines * sizeof(data_info_t));
    if (arr_infos == NULL)
    {
        ESP_LOGE(TAG, "Arr_infos Memory allocation failed.\n");
        fclose(fp);
        xSemaphoreGive(csv_xMutex);
        return NULL;
    }

    rewind(fp);                  /* 重定位指针 fp 到文件开头 */
    fgets(row, sizeof(row), fp); /* 执行 fgets() 到第二行，即跳过标题行 */

    /* 填充结构体数组 */
    int index = 0;
    while (fgets(row, sizeof(row), fp) != NULL)
    {
        char *tmp = NULL; /* 临时字符指针 tmp，用于存储每行数据的各个字段 */
        tmp = get_field(strdup(row), 1);
        strcpy(arr_infos[index].name, tmp);
        free(tmp); /* 释放掉remove_quoted函数分配的内存空间 */

        tmp = get_field(strdup(row), 2);
        strcpy(arr_infos[index].id, tmp);
        free(tmp);

        tmp = get_field(strdup(row), 3);
        strcpy(arr_infos[index].ProjectName, tmp);
        free(tmp);

        tmp = get_field(strdup(row), 4);
        strcpy(arr_infos[index].absorbance, tmp);
        free(tmp);

        tmp = get_field(strdup(row), 5);
        strcpy(arr_infos[index].content, tmp);
        free(tmp);

        tmp = get_field(strdup(row), 6);
        strcpy(arr_infos[index].Date, tmp);
        free(tmp);

        index++;
    }
    fclose(fp);

    /* 为返回的结构体指针分配内存 */
    csv_info_t *csv_info = (csv_info_t *)malloc(sizeof(csv_info_t));
    if (csv_info == NULL) /* 若分配内存失败，则返回 NULL */
    {
        ESP_LOGE(TAG, "Csv_info Memory allocation failed.\n");
        return NULL;
    }
    csv_info->arr_infos = arr_infos;
    csv_info->size = index;

    xSemaphoreGive(csv_xMutex);
    return csv_info;
}

csv_info_t *get_csv_info_reverse(const char *file_path) /* 倒序遍历(未完成) */
{

    if (xSemaphoreTake(csv_xMutex, portMAX_DELAY) == pdTRUE) /* 获取互斥锁 */
    {
        ESP_LOGI(TAG, " csv_xMutex take successful ");
    }
    if (!file_exists(base_path) || !file_exists(file_path)) /* 若路径不存在，返回NULL */
    {
        ESP_LOGW(TAG, " base_path/file_path is not exist ");
        xSemaphoreGive(csv_xMutex);
        return NULL;
    }

    FILE *fp = fopen(file_path, "r");
    if (fp == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file: %s", file_path);
        xSemaphoreGive(csv_xMutex);
        return NULL;
    }

    fseek(fp, 0, SEEK_END); // 从文件末尾开始
    long size = ftell(fp);  // 获取文件大小
    long pos = size - 1;    // 当前指针的位置

    // 反向读取文件直到开头
    while (pos >= 0)
    {
        fseek(fp, pos, SEEK_SET); // 将指针移到当前位置

        // 读取一行
        char buffer[MAX_LINE_LENGTH] = {0};
        fgets(buffer, MAX_LINE_LENGTH, fp);
        if (strlen(buffer) > 0)
        {
            if (buffer[strlen(buffer) - 1] == '\n')
            {
                buffer[strlen(buffer) - 1] = '\0'; // 移除换行符
            }
            printf("%s\n", buffer); // 打印行内容
        }

        // 查找上一个换行符的位置
        while (pos >= 0 && fgetc(fp) != '\n')
        {
            pos--;
        }
        pos--; // 跳过换行符
    }

    fclose(fp);
    return NULL;
}

void Csv_read_file3(const char *file_path)
{
    csv_info_t *csv_info = get_csv_info(file_path);
    if (csv_info == NULL)
    {
        ESP_LOGE(TAG, "csv_info is NULL");
        return;
    }

    print_csv_info(csv_info);
    free_csv_info(csv_info);
}

/******************************************************************************
 * @brief 删除csv文件中的一行数据
 * @param file_path 文件路径, @param lineNumberToDelete 待删除行号
 * @return 无
 *****************************************************************************/
void Csv_del_file3(const char *file_path, uint32_t lineNumberToDelete)
{
    if (xSemaphoreTake(csv_xMutex, portMAX_DELAY) == pdTRUE) /* 获取互斥锁 */
    {
        ESP_LOGI(TAG, " csv_xMutex take successful ");
    }
    if (!file_exists(file_path)) /* 若路径不存在，返回NULL */
    {
        ESP_LOGW(TAG, " file_path is not exist ");
        xSemaphoreGive(csv_xMutex);
        return;
    }
    if (lineNumberToDelete < 0 || lineNumberToDelete > UINT32_MAX - 1) /* 检查删除行 */
    {
        ESP_LOGW(TAG, "Error: Invalid line number: %lu\n", lineNumberToDelete);
        xSemaphoreGive(csv_xMutex);
        return;
    }
    char temp_filename[256]; /* 临时文件名 */

    /* 读取文件名，直到遇到 ' 或者 . 字符为止 */
    sscanf(file_path, "%[^'.']", temp_filename);
    strcat(temp_filename, ".tmp"); /* 为临时文件名添加 .tmp 后缀 */
    // sprintf(temp_filename, "%s.tmp", temp_filename);

    FILE *fp_in, *fp_out;
    fp_in = fopen(file_path, "r");
    fp_out = fopen(temp_filename, "w");
    if (fp_in == NULL) /* 若打开输入文件失败 */
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        xSemaphoreGive(csv_xMutex);
        return;
    }
    if (fp_out == NULL) /* 若打开输出文件失败 */
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        fclose(fp_in); /* 需要把上一步打开的fp_in 关闭 */
        xSemaphoreGive(csv_xMutex);
        return;
    }

    char buf[1024]; /* 读取缓冲区(单行) */

    fgets(buf, sizeof(buf), fp_in); /* 读取标题行 */
    fputs(buf, fp_out);             /* 向新文件写入标题行 */

    int count = 0;                                 /* 用于记录读到了第几行 */
    while (fgets(buf, sizeof(buf), fp_in) != NULL) /* 从第2行开始 */
    {
        if (count != lineNumberToDelete) /* 循环比对 */
        {
            fputs(buf, fp_out);
        }
        count++;
    }
    fclose(fp_in);
    fclose(fp_out);
    remove(file_path);                /* 移除原有文件 */
    rename(temp_filename, file_path); /* 将临时文件重命名为原有文件名 */
    ESP_LOGI(TAG, "Successfully deleted line %ld from file %s\n", lineNumberToDelete, file_path);

    xSemaphoreGive(csv_xMutex);
    return;
}

/****************************************************************************
 * @brief 初始化文件系统
 * @param wl_handle 磨损平衡句柄
 * @return 成功：ESP_OK 失败：ESP_FAIL
 ****************************************************************************/
static esp_err_t storage_init_spiflash(wl_handle_t *wl_handle)
{
    ESP_LOGI(TAG, "Initializing wear levelling");
    /* 查找类型为data且子类型为fat的分区 */
    const esp_partition_t *data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                                     ESP_PARTITION_SUBTYPE_DATA_FAT,
                                                                     NULL);
    if (data_partition == NULL)
    {
        ESP_LOGE(TAG, "Failed to find FATFS partition. Check the partition table.");
        return ESP_ERR_NOT_FOUND;
    }
    /* 挂载分区到磨损平衡层 */
    return wl_mount(data_partition, wl_handle);
}

void FATFS_csv_wrDemo(void)
{
    /* 挂载磨损句柄至data_partition分区 */
    ESP_LOGI(TAG, "Initializing storage...");
    ESP_ERROR_CHECK(storage_init_spiflash(&wl_handle));

    /* 文件系统配置实例 */
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,                               /* 最大打开文件数 */
        .format_if_mount_failed = true,               /* 如果挂载失败，是否自动格式化 */
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE /*分配单元的大小*/
    };
    
    /* 配置msc结构体，使用磨损平衡，并初始化msc存储，挂载基本路径 */
    const tinyusb_msc_spiflash_config_t config_spi = {
        .wl_handle = wl_handle,
        .mount_config = mount_config};
    ESP_ERROR_CHECK(tinyusb_msc_storage_init_spiflash(&config_spi));
    ESP_ERROR_CHECK(tinyusb_msc_storage_mount(base_path));

    /* 配置tinyusb结构体，并安装tinyusb驱动 */
    ESP_LOGI(TAG, "USB Composite initialization...");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,        /* 包含设备描述符的数组，如设备类型、版本、制造商、产品ID等 */
        .string_descriptor = NULL,        /* 包含字符串描述符的数组，如制造商名称、产品名称等 */
        .string_descriptor_count = 0,     /* 指定字符串描述符数组中字符串的数量 */
        .external_phy = false,            /* 是否使用外部物理层(PHY)接口 */
        .configuration_descriptor = NULL, /* 包含配置描述符的数组，定义了USB设备的不同配置选项 */
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    /* (重大疑问)没有这个重复步骤的话，连接USB时http文件服务器会会显示目录不存在!!!; add by amxl */
    tinyusb_msc_storage_deinit();
    tinyusb_msc_storage_init_spiflash(&config_spi);
    /*end add*/

    vTaskDelete(NULL);
}