#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>

#include "counters.h"

#define DRIVER_AUTHOR "Igor V. Nikolaev <support@vedga.com>"
#define DRIVER_DESC   "GPIO pulse counter"
#define DRIVER_VERSION "0.1"

#define TRACE(level, ...) printk(level KBUILD_MODNAME ": " __VA_ARGS__);

struct gpio_pulse_counter_device {
    struct counters_device* cdev;
    struct list_head list;
};

static int device_driver_probe(struct platform_device *pdev);
static int device_driver_remove(struct platform_device *pdev);

/* Protect access to the platform driver data */
static DEFINE_MUTEX(this_driver_lock);

static const struct of_device_id pulse_counter_of_match[] = {
        { .compatible = "gpio-pulse-counter", },
        { },
};
MODULE_DEVICE_TABLE(of, pulse_counter_of_match);

static struct platform_driver gpio_pulse_counter_device_driver = {
        .probe          = device_driver_probe,
        .remove         = device_driver_remove,
        .driver         = {
                .name   = "gpio-pulse-counter",
                .of_match_table = of_match_ptr(pulse_counter_of_match),
/*
                .pm     = &gpio_keys_pm_ops,
                .of_match_table = of_match_ptr(gpio_keys_of_match),
 */
        }
};

static irqreturn_t device_isr(int irq, 
                              void *dev_id) {
//    TRACE(KERN_INFO, "IRQ: %d, devid: %pK\n", irq, dev_id);
    
    if(dev_id) {
        /* Account this pulse */
        counters_pulse((struct counters_device*)dev_id);
        
        /* IRQ handled by this device */
        return IRQ_HANDLED;
    }
    
    
    /* IRQ not handled by this device */
    return IRQ_NONE;
}

static void shutdown_device(struct counters_device *cdev) {
    struct gpio_pulse_counter *drvdata = dev_get_drvdata(&cdev->dev);
    
    TRACE(KERN_INFO, "Driver's shutdown routine for cdev=%pK, drvdata=%pK\n", cdev, drvdata);

    /* Free IRQ */
    free_irq(drvdata->irq, cdev);
    
    if(gpio_is_valid(drvdata->gpio)) {
        /* Need to free GPIO pin */
        devm_gpio_free(&cdev->dev, drvdata->gpio);
    }
}

/**
 * Build device data and register new device in system
 * 
 * @param name
 * @param irq
 * @param gpio
 * @return registered device driver
 * 
 * 1. Allocate counters_device structure
 * 2. Setup driver's private data
 * 3. Register device driver by counters_device structure
 */
struct counters_device *build_device(const char *name, int irq, int gpio) {
    struct counters_device *cdev = 
        counters_allocate_device(sizeof(struct gpio_pulse_counter));

    if(IS_ERR_OR_NULL(cdev)) {
        TRACE(KERN_ALERT, "Unable to allocate class data.\n");

        return cdev ? cdev : ERR_PTR(-ENOMEM);
    } else {
        struct gpio_pulse_counter *drvdata = dev_get_drvdata(&cdev->dev);
        int status;

        drvdata->irq = irq;
        drvdata->gpio = gpio;

        status = counters_register_device(cdev);

        if(status) {
            TRACE(KERN_ALERT, "Unable to register device.\n");

            counters_free_device(cdev);

            return ERR_PTR(status);
        }

        if(gpio_is_valid(gpio)) {
            /* Specified GPIO pin, allocate it to prevent usage by other drivers */
            status = devm_gpio_request(&cdev->dev, gpio, name);
            
            if(status) {
                TRACE(KERN_ALERT, "Unable to allocate GPIO pin %d.\n", gpio);
                
                counters_unregister_device(cdev);
            
                return ERR_PTR(status);
            }
        }
        
        /* Attach IRQ handler */
        status = request_irq(irq, 
                             device_isr, 
                             IRQF_SHARED,
                             name,
                             cdev);  

        if(status) {
            TRACE(KERN_ALERT, "Unable to register IRQ handler.\n");
            
            counters_unregister_device(cdev);
            
            return ERR_PTR(status);
        }
        
        /* Assign special driver's shutdown routine */
        cdev->shutdown = shutdown_device;
        
        return cdev;
    }
}

static int device_driver_probe_dt(struct platform_device *pdev, 
                                  struct device_node *node) {
    int devices = 0;
    
    if(node) {
        struct device_node *pp;
        
        TRACE(KERN_INFO, "Populate device tree nodes (total=%u)\n", of_get_child_count(node));
        
        for_each_child_of_node(node, pp) {
            int gpio = of_get_gpio(pp, 0);
            int irq = irq_of_parse_and_map(pp, 0);

            if(!irq && gpio_is_valid(gpio)) {
                /* Try to determine IRQ by GPIO */
                irq = gpio_to_irq(gpio);

                if(irq < 0) {
                    /* GPIO not support IRQ mode */
                    return irq;
                }
            }
            
            if(irq) {
                /* Build and register device */
                struct counters_device *cdev = build_device(pp->name, irq, gpio);
                
                if(IS_ERR_OR_NULL(cdev)) {
                    TRACE(KERN_ALERT, "Unable to allocate data for %s, skipped.\n", pp->name);
                } else {
                    struct gpio_pulse_counter_device *entry = 
                        kmalloc(sizeof(struct gpio_pulse_counter_device), GFP_KERNEL);
                    
                    if(entry) {
                        struct list_head *devlist = platform_get_drvdata(pdev);

                        TRACE(KERN_INFO, "Allocated deventry=%pK\n", entry);
                        
                        INIT_LIST_HEAD(&entry->list);
                        entry->cdev = cdev;
                        
                        list_add(&entry->list, devlist);
                        
                        if(gpio_is_valid(gpio)) {
                            TRACE(KERN_INFO, "Device #%u %s: IRQ: %d GPIO: %d\n", devices, pp->name, irq, gpio);
                        } else {
                            TRACE(KERN_INFO, "Device #%u %s: IRQ: %d\n", devices, pp->name, irq);
                        }

                        devices++;
                    } else {
                        counters_unregister_device(cdev);
                        
                        TRACE(KERN_ALERT, "Unable to allocate device entry for %s, skipped.\n", pp->name);
                    }
                }
            } else {
                TRACE(KERN_ALERT, "Device %s don't have IRQ, skipped.\n", pp->name);
            }
        }
    }
    
    return devices;
}

static int device_driver_probe(struct platform_device *pdev) {
    struct list_head *devlist = kmalloc(sizeof(struct list_head), GFP_KERNEL);
    
    if(!devlist) {
        TRACE(KERN_ALERT, "Unable to allocate memory for device list.\n");
        
        return -ENOMEM;
    }
    
    TRACE(KERN_INFO, "Allocated devlist=%pK\n", devlist);
    
    INIT_LIST_HEAD(devlist);
    
    mutex_lock(&this_driver_lock);
    
    platform_set_drvdata(pdev, devlist);
    
    TRACE(KERN_INFO, "Called device driver probe.\n");
    
    if(of_have_populated_dt()) {
        // Используется device tree
        device_driver_probe_dt(pdev, pdev->dev.of_node);
    } else {
        TRACE(KERN_ALERT, "Currently support only device tree configuration data.\n");

        platform_set_drvdata(pdev, NULL);
        
        mutex_unlock(&this_driver_lock);
    
        kfree(devlist);
        
        return -ENODEV;
    }

    mutex_unlock(&this_driver_lock);
    
    return 0;
}

static int device_driver_remove(struct platform_device *pdev) {
    struct list_head *devlist;
    
    mutex_lock(&this_driver_lock);
    
    devlist = platform_get_drvdata(pdev);
    
    platform_set_drvdata(pdev, NULL);

    mutex_unlock(&this_driver_lock);
    
    TRACE(KERN_INFO, "Called device driver remove.\n");
    
    if(devlist) {
        struct list_head *pos;
        struct list_head *n;
        
        list_for_each_safe(pos, n, devlist) {
            /* Got entry from the list */
            struct gpio_pulse_counter_device *entry = 
                list_entry(pos, struct gpio_pulse_counter_device, list);
            
            /* Remove entry from list */
            list_del(pos);
            
            /* Unregister device */
            counters_unregister_device(entry->cdev);
            
            TRACE(KERN_INFO, "Free deventry=%pK\n", entry);
            
            /* Free memory */
            kfree(entry);
        }

        TRACE(KERN_INFO, "Free devlist=%pK\n", devlist);
        
        /* Free platform driver data */
        kfree(devlist);
    }
    
    return 0;
}

//struct counters_device *regDev;

static int __init pulsecount_init(void)
{
    return platform_driver_register(&gpio_pulse_counter_device_driver);
}

static void __exit pulsecount_exit(void)
{
    platform_driver_unregister(&gpio_pulse_counter_device_driver);
}


module_init(pulsecount_init)
module_exit(pulsecount_exit)

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
