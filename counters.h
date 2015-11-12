#ifndef __COUNTERS_H
#define __COUNTERS_H

#include <linux/spinlock.h>
#include <linux/time.h>

/* Имя класса устройств */
#define DEVICE_CLASS "counters"
/* Имена устройств в классе */
#define DEVICE_NAME  "counter"

struct counters_device {
    /* Physical resource name */
    const char* name;
    /* Для эксклюзивного доступа к результатам измерений */
    spinlock_t measurements_lock;
    /* Кол-во подсчитанных импульсов на устройстве */
    unsigned long pulse_count;
    /* Временная точка последнего обнаруженного импульса */
    struct timeval last_pulse;
    /* Время, за которое был обнаружен последний зафиксированный импульс */
    struct timeval last_pulse_period;
    /* Среднее время между импульсами */
    struct timeval average_pulse_period;
    /* Release device driver's resources function */
    void (*shutdown)(struct counters_device *);
    /* Kernel device resource */
    struct device dev;
};
/* Метод получения адреса struct counters_device из адреса переменной dev,
 * которая расположена в данной структуре.
 */
#define to_counters_device(d) container_of(d, struct counters_device, dev)

/**
 * Увеличение счетчика использования структуры struct device
 * 
 * @param dev
 * @return 
 */
static inline struct counters_device *counters_get_device(struct counters_device *dev) {
    return dev ? to_counters_device(get_device(&dev->dev)) : NULL;
}

/**
 * Уменьшение счетчика использования структуры struct device
 * @param dev
 */
static inline void counters_put_device(struct counters_device *dev) {
    if(dev) {
        put_device(&dev->dev);
    }
}

struct gpio_pulse_counter {
    int irq;
    int gpio;
};

struct counters_device *counters_allocate_device(const char* name, size_t driver_private_data_size);
void counters_free_device(struct counters_device *dev);
int counters_register_device(struct counters_device *dev);
void counters_unregister_device(struct counters_device *dev);
void counters_pulse(struct counters_device *dev);

#endif
