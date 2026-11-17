
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

#define DEFAULT_STACK_SIZE 2048
#define CDC_ITF_TX      1
#define DEBOUNCE_MS 200

// Tilakone
enum state { WAITING=1, DATA_READY = 2, DASH_PRESSED = 3, DOT_PRESSED = 4, ADD_SPACE = 5, ADD_FINAL_MARK = 6, SENDING = 7};
enum state programState = WAITING;

static volatile uint32_t lastButtonTime = 0;

// gyroskoopin muuttujat
typedef struct {
    float ax;
    float ay;
    float az;
    float gx;
    float gy;
    float gz;
    float temperature;
} imu_sample_t;
imu_sample_t sample;

// Btn_fxn käsittelee kummankin nappin painallukset, ja tulkitsee niistä viivan ja pisteen
static void btn_fxn(uint gpio, uint32_t eventMask) { 

    // Debounce logiikka
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - lastButtonTime < DEBOUNCE_MS) {
        return;  // liian pian edellisestä, ohitetaan
    }
    lastButtonTime = now;

    // nappien erottelulogiikka
    if (gpio == BUTTON1) {                  // piste
        gpio_put(LED1, !gpio_get(LED1));    // testaushommeleita 
        printf("Dot: .\n");
        programState = DOT_PRESSED;
        // LISÄÄ MERKKIJONOON PISTE
    } else if (gpio == BUTTON2) {           // viiva
        printf("Dash: -\n");
        programState = DASH_PRESSED;
        // LISÄÄ MERKKIJONOON VIIVA
    }
}

// funktio, joka tunnistaa kun pico tiputetaan -> viestin lähetys
static void has_dropped(){ 

    // seuraavalla logiikalla: 
    // pyyhi merkkijonon perä tyhjäksi välilyönneistä -> lisää 2x välilyönti ja rivivaihtomerkki -> lähetä
    // jos merkkijono tyhjä -> ignore (debounce jos vahingossa sensori havaitsee 2 kertaa putkeen)
    printf("Pico Dropper -> lähetä viesti\n");
    programState = ADD_FINAL_MARK;
}

// funktio joka tunnistaa kun pico nostetaan pystyyn -> lisää välilyönti
static void pickup_upright(){
    printf("Upright picked up\n");
    // LISÄÄ VÄLILYÖNTI
    programState = ADD_SPACE;
}

// funktio, joka tulkitsee gyroskoopin ja kiihtyvyysnaturin dataa
static void sensor_task(void *arg){

    // alustukset
    init_ICM42670();
    ICM42670_start_with_default_values();
    // välilyönti kun pico nostetaan pystyyn
    // tiputtaminen = lähetys
    bool wasUpright = false;

    for(;;){

        // luetaan sensorin data "sample"-tietueen alkioista
        if (ICM42670_read_sensor_data(&sample.ax, &sample.ay, &sample.az, &sample.gx, &sample.gy, &sample.gz, &sample.temperature) != 0) {    
            printf("Failed to read imu data\n");    
        }

        // nokka ylöspäin nosto -> välilyönnin lisäys
        //                          arvot siis 0 < x < 1
        bool upright = fabsf(sample.ay) > 0.875; 
        if (fabsf(sample.ay) > 0.875 && !wasUpright) {      // wasUpright boolean debouncea varten -> vain 1kpl ylöspäin nosto per ylöspäin nosto
            pickup_upright();
            wasUpright = true;
        } else if (fabsf(sample.ay) < 0.6 && wasUpright) {  // tunnistetaan kun pico takaisin maassa
            printf("Pico is no longer upright\n");
            wasUpright = false;
        }

        // tiputtaminen -> lähetys
        // anturi tuottaa arvon -250 < x < 250, ja tiputuksissa y ja x akseli yleensä on ne arvot jotka nousee eniten
        if ((abs(sample.gy) + abs(sample.gx)) > 305){ 
            has_dropped();
            programState = ADD_FINAL_MARK;
        } 
        

        programState = DATA_READY;
        vTaskDelay(pdMS_TO_TICKS(100));

    }
}


// pelkkää printtausta varten
static void print_task(void *arg){
    
    while(1){
        //printf("Accel: X=%f, Y=%f, Z=%f | Gyro: X=%f, Y=%f, Z=%f| Temp: %2.2f°C\n", sample.ax, sample.ay, sample.az, sample.gx, sample.gy, sample.gz, sample.temperature);
        //printf("Gyro: X=%f, Y=%f, Z=%f| Temp: %2.2f°C\n", sample.gx, sample.gy, sample.gz, sample.temperature);
        //printf("Time: %d ms | Gyro: X=%.2f, Y=%.2f, Z=%.2f \n", pdTICKS_TO_MS(xTaskGetTickCount()), sample.gx, sample.gy, sample.gz);

        programState = WAITING;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


int main() {

    stdio_init_all();
    // Uncomment this lines if you want to wait till the serial monitor is connected
    /*while (!stdio_usb_connected()){
        sleep_ms(10);
    }*/
    
    // alustukset
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.

    gpio_init(BUTTON1);
    gpio_set_dir(BUTTON1, GPIO_IN);
    gpio_init(BUTTON2);
    gpio_set_dir(BUTTON2, GPIO_IN);

    // jäänne, vois käytännössä pyyhkiä
    gpio_init(LED1);
    gpio_set_dir(LED1, GPIO_OUT);

    // kummatki napit tohon btn_fxn:n
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_FALL, true, btn_fxn);
    gpio_set_irq_enabled(BUTTON2, GPIO_IRQ_EDGE_FALL, true);

    
    TaskHandle_t hSensorTask, hPrintTask = NULL;

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

