#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "counters.h"

#define DRIVER_AUTHOR "Igor V. Nikolaev <support@vedga.com>"
#define DRIVER_DESC   "Pulse counters device class"
#define DRIVER_VERSION "0.1"

#define USEC_VALUE 1000000

#define TRACE(level, ...) printk(level KBUILD_MODNAME ": " __VA_ARGS__);


static char *counters_devnode(struct device *dev, umode_t *mode);
static ssize_t clear_count_when_reading_show(struct class *class, struct class_attribute *attr, char *buf);
static ssize_t clear_count_when_reading_store(struct class *class, struct class_attribute *attr, const char *buf, size_t size);
static void counters_device_release(struct device *device);

static ssize_t name_show(struct device *device, 
                         struct device_attribute *attr, 
                         char *buf);


static ssize_t pulse_store(struct device *device, 
                           struct device_attribute *attr, 
                           const char *buf, 
                           size_t size);
static ssize_t count_show(struct device *device, 
                          struct device_attribute *attr, 
                          char *buf);
static ssize_t count_store(struct device *device, 
                           struct device_attribute *attr, 
                           const char *buf, 
                           size_t size);
static ssize_t last_pulse_period_show(struct device *device, 
                                      struct device_attribute *attr, 
                                      char *buf);
static ssize_t last_pulse_period_store(struct device *device, 
                                       struct device_attribute *attr, 
                                       const char *buf, 
                                       size_t size);
static ssize_t average_pulse_period_show(struct device *device, 
                                         struct device_attribute *attr, 
                                         char *buf);
static ssize_t average_pulse_period_store(struct device *device, 
                                          struct device_attribute *attr, 
                                          const char *buf, 
                                          size_t size);

static int timeval_subtract(struct timeval *result, 
                            struct timeval  *x, 
                            struct timeval  *y);


/* Clear conters when it readed */
static int clear_count_when_reading = 0;

/* Root device attributes */
static DEVICE_ATTR_RO(name);

/* Device attributes in the group "values" */
static DEVICE_ATTR_WO(pulse); 
static DEVICE_ATTR_RW(count); 
static DEVICE_ATTR_RW(last_pulse_period); 
static DEVICE_ATTR_RW(average_pulse_period); 

/* Attributes at the "values" group for device drivers */
static struct attribute *counters_device_values_attributes[] = {
    &dev_attr_pulse.attr,
    &dev_attr_count.attr,
    &dev_attr_last_pulse_period.attr,
    &dev_attr_average_pulse_period.attr,
    NULL
};

/* Measurements result attribute group */
static const struct attribute_group counters_device_values = {
    .name = "values",
    .attrs = counters_device_values_attributes,
};

/* Attribute groups for each device driver for this device class */
static const struct attribute_group *counters_device_attr_groups[] = {
    &counters_device_values,
    NULL
};

/* Device driver descriptor */
static struct device_type counters_device_type = {
    /* Groups, added for each device driver for this class */
    .groups = counters_device_attr_groups,
    /* Release private resources, allocated by counters_allocate_device() */
    .release = counters_device_release,
};

/* Attributes for device class */
static struct class_attribute counters_class_attrs[] = {
        __ATTR_RW(clear_count_when_reading),
        __ATTR_NULL,
};

/* Device class descriptor */
struct class counters_class = {
        .name           = DEVICE_CLASS,
        .devnode        = counters_devnode,
        .class_attrs    = counters_class_attrs,
};
EXPORT_SYMBOL_GPL(counters_class);

/**
 * Allocate resource for device drivers
 * 
 * @param driver_private_data_size - driver's private data size or 0
 * @return 
 * 
 * NOTE:
 * For free allocated by this function resource use:
 * counters_free_device(), if device is not registered;
 * counters_unregister_device(), if device is registered by counters_register_device()
 */
struct counters_device *counters_allocate_device(const char* name, 
                                                 size_t driver_private_data_size) {
    static atomic_t counter_no = ATOMIC_INIT(-1);
    void *pvt = driver_private_data_size ? kzalloc(driver_private_data_size, GFP_KERNEL) : NULL;
    struct counters_device *dev;
    
    if(pvt) {
#if 0        
        TRACE(KERN_DEBUG, "Allocated driver's private data: %pK\n", pvt);
#endif        
    } else {
        if(driver_private_data_size) {
            TRACE(KERN_ALERT, "Unable to allocate private driver's memory.\n");

            return ERR_PTR(-ENOMEM);
        }
    }
    
    dev = kzalloc(sizeof(struct counters_device), GFP_KERNEL);

    if(dev) {
        /* Store physical resource name */
        dev->name = kstrdup_const(name, GFP_KERNEL);
        
        if(!dev->name) {
            if(pvt) {
                /* Free allocated resources */
                kfree(pvt);
            }
            
            kfree(dev);
            
            TRACE(KERN_ALERT, "Unable to allocate memory for device class data.\n");

            return ERR_PTR(-ENOMEM);
        }
        
        /* Устанавливаем тип и класс устройства и инициализируем его ресурсы */
        dev->dev.type = &counters_device_type;
        dev->dev.class = &counters_class;
        /* Инициализация структуры данных и установка счетчика ссылок на нее в 1 */
        device_initialize(&dev->dev);

        /* Формируем уникальное имя для создаваемого устройства */
        dev_set_name(&dev->dev, 
                     "%s%lu", 
                     DEVICE_NAME, (unsigned long)atomic_inc_return(&counter_no));

        /* Set area for private driver's data */
        dev_set_drvdata(&dev->dev, pvt);
        
        // Инициализация критической секции для доступа к результатам измерений
        spin_lock_init(&dev->measurements_lock);
        
        // Кол-во подсчитанных импульсов на устройстве
        dev->pulse_count = 0ul;
        
        /* Т.к. используются данные нашего модуля, увеличим кол-во ссылок на него  */
        __module_get(THIS_MODULE);
    } else {
        if(pvt) {
            /* Free allocated resources */
            kfree(pvt);
        }
        
        TRACE(KERN_ALERT, "Unable to allocate memory for device class data.\n");
        
        return ERR_PTR(-ENOMEM);
    }

#if 0        
    TRACE(KERN_DEBUG, "Allocated class data: %pK\n", dev);
#endif
    
    return dev;
}
EXPORT_SYMBOL(counters_allocate_device);

/**
 * Free resources, allocated by counters_allocate_device()
 * 
 * @param dev
 * 
 * NOTE:
 * Can be called only before register device
 */
void counters_free_device(struct counters_device *dev) {
    if(dev) {
        /* Уменьшаем кол-во ссылок на ресурс */
        counters_put_device(dev);
    }
}
EXPORT_SYMBOL(counters_free_device);

/**
 * Register device
 * 
 * @param dev
 * @return 
 */
int counters_register_device(struct counters_device *dev) {
    int rc;

#if 0            
    TRACE(KERN_DEBUG, "Register class device: %pK\n", dev);
#endif    
    
    rc = device_add(&dev->dev);

    if(!rc) {
        /* Create attribute "name" for this device */
        rc = device_create_file(&dev->dev, &dev_attr_name);
    }

    return rc;
}
EXPORT_SYMBOL(counters_register_device);

/**
 * Unregister device
 * 
 * @param dev
 */
void counters_unregister_device(struct counters_device *dev) {
#if 0        
    TRACE(KERN_DEBUG, "Unregister class device: %pK\n", dev);
#endif    

    /* Remove attribute name for this device */
    device_remove_file(&dev->dev, &dev_attr_name);
    
    device_del(&dev->dev);

    counters_put_device(dev);
}
EXPORT_SYMBOL(counters_unregister_device);

/**
 * Count pulse event
 * 
 * @param dev
 */
void counters_pulse(struct counters_device *dev) {
    struct timeval now;

    /* Current timestamp */
    do_gettimeofday(&now);
    
    spin_lock(&dev->measurements_lock);

    /* Total pulses */
    dev->pulse_count++;

    if(dev->last_pulse.tv_sec || dev->last_pulse.tv_usec) {
        // Есть информация о временной точке предпоследнего импульса
        // Определяем время, которое прошло с момента его регистрации
        if(timeval_subtract(&dev->last_pulse_period, &now, &dev->last_pulse)) {
            // Текущее значение меньше предыдущего (т.е. было переполнение)
            // @TODO: Fix time overflow
            dev->last_pulse_period.tv_sec = 0;
            dev->last_pulse_period.tv_usec = 0;
        }
        
        if(dev->average_pulse_period.tv_sec || dev->average_pulse_period.tv_usec) {
            // Не первое измерение, можем продолжать вычислять среднее значение
            dev->average_pulse_period.tv_sec += dev->last_pulse_period.tv_sec;
            dev->average_pulse_period.tv_usec += dev->last_pulse_period.tv_usec;
            
            if(dev->average_pulse_period.tv_usec >= USEC_VALUE) {
                dev->average_pulse_period.tv_sec += dev->average_pulse_period.tv_usec / USEC_VALUE;
                
                dev->average_pulse_period.tv_usec %= USEC_VALUE;
            }
            
            // Делим среднее значение на 2
            dev->average_pulse_period.tv_usec >>= 1;
            if(dev->average_pulse_period.tv_sec & 1) {
                dev->average_pulse_period.tv_usec += USEC_VALUE / 2;
            }
            dev->average_pulse_period.tv_sec >>= 1;

            if(dev->average_pulse_period.tv_usec >= USEC_VALUE) {
                dev->average_pulse_period.tv_sec += dev->average_pulse_period.tv_usec / USEC_VALUE;
                
                dev->average_pulse_period.tv_usec %= USEC_VALUE;
            }
        } else {
            // Первое измерение - пока не можем вычислить среднее значение
            memcpy(&dev->average_pulse_period, &dev->last_pulse_period, sizeof(dev->average_pulse_period));
        }
    }

    /* Current timestamp */
    memcpy(&dev->last_pulse, &now, sizeof(dev->last_pulse));
    
    spin_unlock(&dev->measurements_lock);
}
EXPORT_SYMBOL(counters_pulse);

/**
 * Free resources, allocated by  counters_allocate_device()
 * @param device
 */
static void counters_device_release(struct device *device) {
    struct counters_device *cdev = to_counters_device(device);
    void *pvt;

    if(cdev->shutdown) {
        /* Execute driver's shutdown routine */
        (*cdev->shutdown)(cdev);
    }
    
    pvt = dev_get_drvdata(&cdev->dev);

    if(pvt) {
#if 0        
        TRACE(KERN_DEBUG, "Deallocate driver's private data: %pK\n", pvt);
#endif        
        
        kfree(pvt);
    }
    
#if 0        
    TRACE(KERN_DEBUG, "Deallocate class data: %pK\n", cdev);
#endif    
    
    /* Release string resource */
    kfree_const(cdev->name);
    
    /* Release counters_device structure */
    kfree(cdev);

    /* Счетчик ссылок на модуль был увеличен при выделении ресурсов, теперь его можно уменьшить */
    module_put(THIS_MODULE);
}

static ssize_t name_show(struct device *device, 
                         struct device_attribute *attr, 
                         char *buf) {
    struct counters_device *cdev = to_counters_device(device);
    
    return scnprintf(buf, PAGE_SIZE, "%s", cdev->name);
}

/**
 * Simulate pulse
 * 
 * @param device
 * @param attr
 * @param buf
 * @param size
 * @return 
 */
static ssize_t pulse_store(struct device *device, 
                           struct device_attribute *attr, 
                           const char *buf, 
                           size_t size) {
    counters_pulse(to_counters_device(device));

    return size;
}

/**
 * Retrieve pulse count
 * 
 * @param device
 * @param attr
 * @param buf
 * @return 
 */
static ssize_t count_show(struct device *device, 
                          struct device_attribute *attr, 
                          char *buf) {
    unsigned long value;
    struct counters_device *dev = to_counters_device(device);

    spin_lock(&dev->measurements_lock);
    
    value = dev->pulse_count;
    
    if(clear_count_when_reading) {
        /* Requested clear count after it readed */
        dev->pulse_count = 0;
    }
    
    spin_unlock(&dev->measurements_lock);
    
    return scnprintf(buf, PAGE_SIZE, "%lu", value);
}

/**
 * Overwrite pulse count value
 * 
 * @param device
 * @param attr
 * @param buf
 * @param size
 * @return 
 */
static ssize_t count_store(struct device *device, 
                           struct device_attribute *attr, 
                           const char *buf, 
                           size_t size) {
    unsigned long value;
    
    if(sscanf(buf, "%lu", &value) == 1) {
        struct counters_device *dev = to_counters_device(device);
        
        spin_lock(&dev->measurements_lock);
        
        dev->pulse_count = value;
        
        spin_unlock(&dev->measurements_lock);
        
        return size;
    }

    return -EINVAL;
}

static ssize_t last_pulse_period_show(struct device *device, 
                                      struct device_attribute *attr, 
                                      char *buf) {
    struct timeval value;
    
    struct counters_device *dev = to_counters_device(device);

    spin_lock(&dev->measurements_lock);
    
    memcpy(&value, &dev->last_pulse_period, sizeof(value));
    
    spin_unlock(&dev->measurements_lock);
    
    return (value.tv_sec || value.tv_usec) ?
        scnprintf(buf, PAGE_SIZE, "%lu%lu", value.tv_sec, value.tv_usec) :
        scnprintf(buf, PAGE_SIZE, "%u", 0);
}

static ssize_t last_pulse_period_store(struct device *device, 
                                       struct device_attribute *attr, 
                                       const char *buf, 
                                       size_t size) {
    struct counters_device *dev = to_counters_device(device);

    spin_lock(&dev->measurements_lock);

    dev->last_pulse_period.tv_sec = 0;
    dev->last_pulse_period.tv_usec = 0;

    spin_unlock(&dev->measurements_lock);

    return size;
}

static ssize_t average_pulse_period_show(struct device *device, 
                                         struct device_attribute *attr, 
                                         char *buf) {
    struct timeval value;
    
    struct counters_device *dev = to_counters_device(device);

    spin_lock(&dev->measurements_lock);
    
    memcpy(&value, &dev->average_pulse_period, sizeof(value));
    
    spin_unlock(&dev->measurements_lock);
    
    return (value.tv_sec || value.tv_usec) ?
        scnprintf(buf, PAGE_SIZE, "%lu%lu", value.tv_sec, value.tv_usec) :
        scnprintf(buf, PAGE_SIZE, "%u", 0);
}

static ssize_t average_pulse_period_store(struct device *device, 
                                          struct device_attribute *attr, 
                                          const char *buf, 
                                          size_t size) {
    struct counters_device *dev = to_counters_device(device);

    spin_lock(&dev->measurements_lock);

    dev->average_pulse_period.tv_sec = 0;
    dev->average_pulse_period.tv_usec = 0;

    spin_unlock(&dev->measurements_lock);

    return size;
}


static ssize_t clear_count_when_reading_show(struct class *class, struct class_attribute *attr, char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "%d", clear_count_when_reading);
}

static ssize_t clear_count_when_reading_store(struct class *class, struct class_attribute *attr, const char *buf, size_t size)
{
        int value;
        ssize_t result;
        result = sscanf(buf, "%d", &value);
        if (result != 1) {
                return -EINVAL;
        }
        clear_count_when_reading = value ? 1 : 0;
        
        return size;
}

static char *counters_devnode(struct device *dev, umode_t *mode)
{
        return kasprintf(GFP_KERNEL, "%s/%s", DEVICE_CLASS, dev_name(dev));
}

/**
 * Calculate (*x) - (*y)
 * 
 * @param result
 * @param x
 * @param y
 * @return 1 if negative result, else 0
 */
static int timeval_subtract(struct timeval *result, 
                            struct timeval *x, 
                            struct timeval *y) {
    /* Perform the carry for the later subtraction by updating y. */
    if(x->tv_usec < y->tv_usec) {
        int nsec = (y->tv_usec - x->tv_usec) / USEC_VALUE + 1;
        y->tv_usec -= USEC_VALUE * nsec;
        y->tv_sec += nsec;
    }
    
    if(x->tv_usec - y->tv_usec > USEC_VALUE) {
        int nsec = (x->tv_usec - y->tv_usec) / USEC_VALUE;
        y->tv_usec += USEC_VALUE * nsec;
        y->tv_sec -= nsec;
    }

    /* Compute the time remaining to wait. tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}

static int __init counters_init(void)
{
    int rc = class_register(&counters_class);
    
    if(rc) {
        TRACE(KERN_ALERT, "Load class driver failed.\n");
    } else {
        TRACE(KERN_INFO, "Class driver loaded.\n");
    }

    return rc;
}

static void __exit counters_exit(void)
{
    TRACE(KERN_INFO, "Shutdown class driver.\n");

    class_unregister(&counters_class);
}


module_init(counters_init)
module_exit(counters_exit)

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
