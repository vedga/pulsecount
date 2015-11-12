#### Linux kernel drivers for variety counter devices

#### Linux device driver for counting pulses at the GPIO line

#### Device tree configuration example for Banana PI:

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

