
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

#define DEFAULT_STACK_SIZE 2048
#define CDC_ITF_TX      1


// Tehtävä 3: Tilakoneen esittely Add missing states.
enum state { WAITING=1, DATA_READY = 2
};
enum state programState = WAITING;

// Tehtävä 3: Valoisuuden globaali muuttuja
uint32_t ambientLight;
char morseBuffer[64];

static void btn_fxn(uint gpio, uint32_t eventMask) {
    // btn_fxn vastaanottaa napin merkin? viiva ja piste
    // tallennetaan mihin? Bittirekisteriin? muuhun muuttujaan? 

    // btn_fxn ottaa napinpainalluksen -> sensor_task havaitsee ees taas heilutuksen ja tulkitsee välilyöntimerkit -> sensor_task havaitsee ylösalan heilutuksen ja lisää viestiin \n ja lähettää usbilla viestin
    // ICM42670_I2C_ADDRESS (0x69)

    printf("\nButton pressed: %d", gpio);
    uint8_t pinValue = gpio_get(LED1);
    pinValue = !pinValue;
    gpio_put(LED1, pinValue);

}

static void sensor_task(void *arg){

    // Alusta gyroskooppianturi ICM42670_I2C_ADDRESS (0x69)
    init_ICM42670;
    init_veml6030;
    
    for(;;){
        
        // Tehtävä 2: Muokkaa tästä eteenpäin sovelluskoodilla. Kommentoi seuraava rivi.
        // printf("%d\n", veml6030_read_light);
        // tight_loop_contents(); 


        // Tehtävä 3:  Muokkaa aiemmin Tehtävässä 2 tehtyä koodia ylempänä.
        //             Jos olet oikeassa tilassa, tallenna anturin arvo tulostamisen sijaan
        //             globaaliin muuttujaan.
        //             Sen jälkeen muuta tilaa.

        ambientLight = veml6030_read_light();
        printf("state = dataready\n"); //debug
        programState = DATA_READY;


        // Exercise 2. Just for sanity check. Please, comment this out
        // printf("sensorTask\n");

        // Do not remove this
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void print_task(void *arg){
    (void)arg;
    
    while(1){
        
        // Tehtävä 3: Kun tila on oikea, tulosta sensoridata merkkijonossa debug-ikkunaan
        //            Muista tilamuutos
        //            Älä unohda kommentoida seuraavaa koodiriviä.

        // tight_loop_contents();
        printf("%d\n", ambientLight);
        programState = WAITING;
        printf("state = waiting\n");

        // Tehtävä 3: Just for sanity check. Please, comment this out
        printf("printTask\n");
        
        // Do not remove this
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


int main() {


    stdio_init_all();

    // Uncomment this lines if you want to wait till the serial monitor is connected
    /*while (!stdio_usb_connected()){
        sleep_ms(10);
    }*/ 
    
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.


    // Tehtävä 1:  Alusta painike ja LEd ja rekisteröi vastaava keskeytys.
    //             Keskeytyskäsittelijä on määritelty yläpuolella nimellä btn_fxn

        gpio_init(BUTTON1);
        gpio_set_dir(BUTTON1, GPIO_IN);
        gpio_init(BUTTON2);
        gpio_set_dir(BUTTON2, GPIO_IN);
    
        gpio_init(LED1);
        gpio_set_dir(LED1, GPIO_OUT);

    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_FALL, true, btn_fxn);
    gpio_set_irq_enabled_with_callback(BUTTON2, GPIO_IRQ_EDGE_FALL, true, btn_fxn);


    
    TaskHandle_t hSensorTask, hPrintTask, hUSB = NULL;

    // Create the tasks with xTaskCreate
    BaseType_t result = xTaskCreate(sensor_task, // (en) Task function
                "sensor",                        // (en) Name of the task 
                DEFAULT_STACK_SIZE,              // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,                            // (en) Arguments of the task 
                2,                               // (en) Priority of this task
                &hSensorTask);                   // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Sensor task creation failed\n");
        return 0;
    }
    result = xTaskCreate(print_task,  // (en) Task function
                "print",              // (en) Name of the task 
                DEFAULT_STACK_SIZE,   // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,                 // (en) Arguments of the task 
                2,                    // (en) Priority of this task
                &hPrintTask);         // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Print Task creation failed\n");
        return 0;
    }

    // Start the scheduler (never returns)
    vTaskStartScheduler();
    
    // Never reach this line.
    return 0;
}

