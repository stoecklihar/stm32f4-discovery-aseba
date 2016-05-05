#include <stdio.h>
#include <stdlib.h>
#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "shell.h"
#include "usbcfg.h"
#include "cmd.h"
#include "malloc_lock.h"
#include "memory_protection.h"
#include "main.h"
#include "config_flash_storage.h"

#include "discovery_demo/accelerometer.h"
#include "discovery_demo/leds.h"
#include "discovery_demo/button.h"

#include "aseba_vm/aseba_node.h"
#include "aseba_vm/aseba_can_interface.h"
#include "aseba_vm/aseba_bridge.h"

#define SHELL_WA_SIZE   THD_WORKING_AREA_SIZE(2048)

parameter_namespace_t parameter_root, aseba_ns;

static bool load_config(void)
{
    extern uint32_t _config_start, _config_end;
    size_t len = (size_t)(&_config_end - &_config_start);

    return config_load(&parameter_root, &_config_start, len);
}

int main(void)
{
    thread_t *shelltp = NULL;

    halInit();
    chSysInit();
    mpu_init();

    parameter_namespace_declare(&parameter_root, NULL, NULL);


    // UART2 on PA2(TX) and PA3(RX)
    sdStart(&SD2, NULL);
    palSetPadMode(GPIOA, 2, PAL_MODE_ALTERNATE(7));
    palSetPadMode(GPIOA, 3, PAL_MODE_ALTERNATE(7));

    // serial-over-USB CDC driver.
    sduObjectInit(&SDU1);
    sduStart(&SDU1, &serusbcfg);
    usbDisconnectBus(serusbcfg.usbp);
    chThdSleepMilliseconds(1000);
    usbStart(serusbcfg.usbp, &usbcfg);
    usbConnectBus(serusbcfg.usbp);


    // Initialise Aseba system, declaring parameters
    parameter_namespace_declare(&aseba_ns, &parameter_root, "aseba");
    aseba_declare_parameters(&aseba_ns);

    /* Load parameter tree from flash. */
    load_config();

    /* Start AsebaCAN. Must be after config was loaded because the CAN id
     * cannot be changed at runtime. */
    aseba_vm_init();
    aseba_can_start(&vmState);

    /* If button is pressed, start in translator mode. */
    if (palReadPad(GPIOA, GPIOA_BUTTON)) {
        aseba_bridge((BaseSequentialStream *)&SDU1);
        while (true) {
            chThdSleepMilliseconds(100);
        }
    } else {
        // Initialise Discovery board demo setup
        demo_led_init();
        aseba_vm_start();
    }

    demo_acc_start(&accelerometer_cb);
    demo_button_start();


    shellInit();

    static const ShellConfig shell_cfg1 = {
        (BaseSequentialStream *)&SDU1,
        shell_commands
    };

    while (TRUE) {
        if (!shelltp) {
            if (SDU1.config->usbp->state == USB_ACTIVE) {
                shelltp = shellCreate(&shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);
            }
        } else {
            if (chThdTerminatedX(shelltp)) {
                chThdRelease(shelltp);
                shelltp = NULL;
            }
        }
        chThdSleepMilliseconds(500);
    }
}
