/* 
   
   nRF51-diwali/main.c

   Hacking a cheap Diwali LED Lamp with nRF51822 BLE.

   Author: Mahesh Venkitachalam
   Website: electronut.in


   Reference:

   http://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.sdk51.v9.0.0%2Findex.html

 */

#include "ble_init.h"

extern ble_nus_t m_nus;                                  

// Create the instance "PWM1" using TIMER2.
APP_PWM_INSTANCE(PWM1,2);                   

// These are based on default values sent by Nordic nRFToolbox app
// Modify as neeeded
#define FORWARD "FastForward"
#define REWIND "Rewind"
#define STOP "Stop"
#define PAUSE "Pause"
#define PLAY "Play"
#define START "Start"
#define END "End"
#define RECORD "Rec"
#define SHUFFLE "Shuffle"


// events
typedef enum _AppEventType {
    eAppEvent_Start,
    eAppEvent_Stop,
    eAppEvent_Slower,
    eAppEvent_Faster
} AppEventType;


// structure handle pending events
typedef struct _AppEvent
{
    bool pending;
    AppEventType event;
    int data;
} AppEvent;

AppEvent appEvent;

uint32_t delay = 20;


// Function for handling the data from the Nordic UART Service.
static void nus_data_handler(ble_nus_t * p_nus, uint8_t * p_data, 
                             uint16_t length)
{
    if (strstr((char*)(p_data), REWIND)) {
        appEvent.event = eAppEvent_Slower;
        appEvent.pending = true;
    }
    else if (strstr((char*)(p_data), FORWARD)) {
        appEvent.event = eAppEvent_Faster;
        appEvent.pending = true;
    }
    else if (strstr((char*)(p_data), STOP)) {
        appEvent.event = eAppEvent_Stop;
        appEvent.pending = true;
    }
    else if (strstr((char*)(p_data), PLAY)) {
        appEvent.event = eAppEvent_Start;
        appEvent.pending = true;
    }
}


// A flag indicating PWM status.
static volatile bool pwmReady = false;            

// PWM callback function
void pwm_ready_callback(uint32_t pwm_id)    
{
    pwmReady = true;
}

// Function for initializing services that will be used by the application.
void services_init()
{
    uint32_t       err_code;
    ble_nus_init_t nus_init;
    
    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = nus_data_handler;
    
    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);
}

#define APP_TIMER_PRESCALER  0    /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS 6    /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE 4  /**< Size of timer operation queues. */

// Application main function.
int main(void)
{
  uint32_t err_code;

  // set up timers
  APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, 
                   APP_TIMER_OP_QUEUE_SIZE, false);

  // initlialize BLE
  ble_stack_init();
  gap_params_init();
  services_init();
  advertising_init();
  conn_params_init();
  // start BLE advertizing
  err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
  APP_ERROR_CHECK(err_code);
  
  // init GPIOTE
  err_code = nrf_drv_gpiote_init();
  APP_ERROR_CHECK(err_code);
  
  // init PPI
  err_code = nrf_drv_ppi_init();
  APP_ERROR_CHECK(err_code);
  
  // intialize UART
  uart_init();
  
  // prints to serial port
  printf("starting...\n");
  
  // set up LED
  uint32_t pinLED = 28;
  //nrf_gpio_pin_dir_set(pinLED, NRF_GPIO_PIN_DIR_OUTPUT);

  // 2-channel PWM
  app_pwm_config_t pwm1_cfg = 
      APP_PWM_DEFAULT_CONFIG_1CH(5000L, pinLED);
  
  pwm1_cfg.pin_polarity[0] = APP_PWM_POLARITY_ACTIVE_LOW;
  
  printf("before pwm init\n");
  /* Initialize and enable PWM. */
  err_code = app_pwm_init(&PWM1,&pwm1_cfg,pwm_ready_callback);
  APP_ERROR_CHECK(err_code);
  printf("after pwm init\n");
  
  //app_pwm_enable(&PWM1);
    
  printf("entering loop\n");

  int dir = 1;
  int val = 0;
  appEvent.pending = false;

  while(1) {

      // PWM stop/start requires some tricks 
      // because app_pwm_disable() has a bug. 
      // See: 
      // https://devzone.nordicsemi.com/question/41179/how-to-stop-pwm-and-set-pin-to-clear/

      // is event flag set?
      if (appEvent.pending) {
          switch(appEvent.event) {
              
              case eAppEvent_Start:
              {
                  nrf_drv_gpiote_out_task_enable(pinLED);
                  app_pwm_enable(&PWM1);
              }
              break;
                  
              case eAppEvent_Stop:
              {
                  app_pwm_disable(&PWM1);
                  nrf_drv_gpiote_out_task_disable(pinLED);
                  nrf_gpio_cfg_output(pinLED);
                  nrf_gpio_pin_set(pinLED);
              }
              break;
                  
              case eAppEvent_Faster:
              {
                  if (delay > 5) {
                      delay -= 5;
                  }
              }
              break;

              case eAppEvent_Slower:
              {
                  if( delay < 80) {
                      delay += 5;
                  }
              }
              break;
          }

          // reset flag
          appEvent.pending = false;
      }
  
      while (app_pwm_channel_duty_set(&PWM1, 0, val) == NRF_ERROR_BUSY);

      // change direction at edges
      if(val > 99) {
          dir = -1;
      }
      else if (val < 1){
          dir = 1;
      }
      // increment/decrement
      val += dir*2;

      // delay
      nrf_delay_ms(delay);
  }
}
