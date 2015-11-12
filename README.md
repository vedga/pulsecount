#### Linux kernel drivers for variety counter devices

#### Linux device driver for counting pulses at the GPIO line

#### Device tree configuration example for Banana PI:

```
/* Device node */
counters {
        compatible = "gpio-pulse-counter";
        /* Counter node */
        gas-meter@0 {
            label = "Gas meter";

            /* pinctrl and gpios may be omitted if present interrupt properties */
            pinctrl-names = "default";
            pinctrl-0 = <&ext_counter_bananapi>;

            /* CON3, pin 12: PH2 - pin 226 (Multi6 function: EINT2) */
            /* bank: 226 / 32 = 7, pin into the bank 226 % 32 = 2 */
            gpios = <&pio 7 2 GPIO_ACTIVE_LOW>;

            /* interrupt properties may be omitted if used gpios property */
            interrupt-parent = <&pio>;
            interrupt-names = "counter-edge-falling";
            interrupts = <2 IRQ_TYPE_EDGE_FALLING>; /* PH2 / EINT2 */
        };
};

&pio {
        /* External counter */
        ext_counter_bananapi: counter_pins@0 {
                allwinner,pins = "PH2";
                allwinner,function = "gpio_in";
                allwinner,drive = <SUN4I_PINCTRL_10_MA>;
                allwinner,pull = <SUN4I_PINCTRL_NO_PULL>;
        };
};
```

#### Check functionality

Manually loading module:

```
# modprobe gpio-pulse
```

Check dmesg output

```
[158215.259395] counters: Class driver loaded.
[158215.261738] gpio_pulse: Device #0 gas-meter: IRQ: 53 GPIO: 226
```

Check GPIO allocation:

```
# grep 226 /sys/kernel/debug/pinctrl/1c20800.pinctrl/pinmux-pins
pin 226 (PH2): (MUX UNCLAIMED) 1c20800.pinctrl:226
```

Check device class:

```
# ls -la /sys/class/counters/
drwxr-xr-x  2 root root    0 Nov 12 12:52 .
drwxr-xr-x 55 root root    0 Nov 10 16:52 ..
-rw-r--r--  1 root root 4096 Nov 12 12:52 clear_count_when_reading
lrwxrwxrwx  1 root root    0 Nov 12 12:52 counter0 -> ../../devices/virtual/counters/counter0
```

Check device:

```
# cat /sys/class/counters/counter0/name
gas-meter
```

Check measurements:

```
# cat /sys/class/counters/counter0/values/count
0
# echo 1 > /sys/class/counters/counter0/values/pulse
# echo 1 > /sys/class/counters/counter0/values/pulse
# echo 1 > /sys/class/counters/counter0/values/pulse
# cat /sys/class/counters/counter0/values/count
3
```
