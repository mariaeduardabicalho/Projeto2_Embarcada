#include <asf.h>
#include "conf_board.h"


/************************************************************************/
/* defines                                                              */
/************************************************************************/

/* Canal do sensor de ph */
#define AFEC_CHANNEL 0

// AFEC
/** Reference voltage for AFEC,in mv. */
#define VOLT_REF        (3300)

/** The maximal digital value */
/** 2^12 - 1                  */
#define MAX_DIGITAL     (4095)



// TRIGGER SONAR
#define TRIGGER_PIO_A      PIOD
#define TRIGGER_PIO_ID_A    ID_PIOD
#define TRIGGER_IDX_A       26
#define TRIGGER_IDX_MASK_A  (1 << TRIGGER_IDX_A)

// ECHO SONAR
#define ECHO_PIO_A       PIOC
#define ECHO_PIO_ID_A    ID_PIOC
#define ECHO_IDX_A   13
#define ECHO_IDX_MASK_A  (1 << ECHO_IDX_A)

// Sonar Timer
#define TIMER_CHANNEL_A 1
#define TIMER_ID_A      ID_TC1
#define TIMER_A         TC0
#define TIMER_FREQ_A    42

// WaterFlow Timer
#define TIMER_CHANNEL_B 0
#define TIMER_ID_B      ID_TC0
#define TIMER_B         TC0
#define TIMER_FREQ_B    1

// WaterFlow sensor
#define WATER_PIO_A       PIOA
#define WATER_PIO_ID_A    ID_PIOA
#define WATER_IDX_A   4
#define WATER_IDX_MASK_A  (1 << WATER_IDX_A)

// FISICA
#define SOUND_SPEED_MS 340.0

#define USART_COM_ID ID_USART1
#define USART_COM    USART1

/** RTOS  */
#define TASK_TRIGGER_STACK_SIZE            (1024/sizeof(portSTACK_TYPE))
#define TASK_TRIGGER_STACK_PRIORITY        (tskIDLE_PRIORITY+1)
#define TASK_UARTTX_STACK_SIZE             (2048/sizeof(portSTACK_TYPE))
#define TASK_UARTTX_STACK_PRIORITY         (tskIDLE_PRIORITY)
#define TASK_UARTRX_STACK_SIZE             (2048/sizeof(portSTACK_TYPE))
#define TASK_UARTRX_STACK_PRIORITY         (1)
#define TASK_PROCESS_STACK_SIZE            (2048/sizeof(portSTACK_TYPE))
#define TASK_PROCESS_STACK_PRIORITY        (2)

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,
signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);

/************************************************************************/
/* globals                                                              */
/************************************************************************/

volatile tank_height = 224;
volatile spin_counter = 0;
volatile flag_tc = false;


SemaphoreHandle_t xSMF;//Objeto do semaforo
QueueHandle_t hc04_A_EchoQueue;

/** Semaforo a ser usado pela task do medidor de fluxo */
SemaphoreHandle_t xSemaphoreWater;
SemaphoreHandle_t xSemaphoreCounter;
SemaphoreHandle_t xSemaphoreTC;
QueueHandle_t xWaterQueue;

/** Queue da task AFEC */
QueueHandle_t xQueueAfec;
QueueHandle_t xQueuePh;



/** Variaveis da Task DATA */
QueueHandle_t xQueueData;

typedef struct {
	uint id;
	int32_t value;
	int timestamp;
} sensorData;



/** prototypes */
void but_callback(void);
static void ECHO_init(void);
static void USART1_init(void);
uint32_t usart_puts(uint8_t *pstring);
void io_init(void);
void TC_init0(Tc * TC, int ID_TC, int TC_CHANNEL, int freq);
void TC_init1(Tc * TC, int ID_TC, int TC_CHANNEL, int freq);

/************************************************************************/
/* RTOS application funcs                                               */
/************************************************************************/

/**
* \brief Called if stack overflow during execution
*/
extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,
signed char *pcTaskName)
{
	printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
	/* If the parameters have been corrupted then inspect pxCurrentTCB to
	* identify which task has overflowed its stack.
	*/
	for (;;) {
	}
}

/**
* \brief This function is called by FreeRTOS idle task
*/
extern void vApplicationIdleHook(void)
{
	pmc_sleep(SAM_PM_SMODE_SLEEP_WFI);
}

/**
* \brief This function is called by FreeRTOS each tick
*/
extern void vApplicationTickHook(void)
{
}

extern void vApplicationMallocFailedHook(void)
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

	/* Force an assert. */
	configASSERT( ( volatile void * ) NULL );
}

/************************************************************************/
/* handlers / callbacks                                                 */
/************************************************************************/

void EchoCallback(void){
	if(pio_get(ECHO_PIO_A, PIO_INPUT, ECHO_IDX_MASK_A)){
		tc_start(TIMER_A, TIMER_CHANNEL_A);
		}else{
		float ts = ((float) tc_read_cv(TIMER_A, TIMER_CHANNEL_A))/32000.0;
		xQueueSendToBackFromISR( hc04_A_EchoQueue, &ts, NULL );
	}
}

void WaterCallback(void){
	
	//printf("GIROU");
	spin_counter++;
	xSemaphoreGiveFromISR(xSemaphoreWater, NULL);
}

/**
*  Interrupt handler for TC1 interrupt.
*/
void TC0_Handler(void){
	
	volatile uint32_t ul_dummy;

	/****************************************************************
	* Devemos indicar ao TC que a interrupção foi satisfeita.
	******************************************************************/
	ul_dummy = tc_get_status(TC0, 1);

	/* Avoid compiler warning */
	UNUSED(ul_dummy);
	
	//xQueueSendToBackFromISR( xWaterQueue, &cicles, NULL );
	//BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	//xSemaphoreGiveFromISR(xSemaphoreTC, &xHigherPriorityTaskWoken);
	flag_tc = true;
	//printf("AA      ");
}

static void AFEC_callback(void)
{
	int32_t ph_value;
	ph_value = afec_channel_get_value(AFEC0, AFEC_CHANNEL);
	xQueueSendFromISR( xQueueAfec, &ph_value, 0);
	
}




/************************************************************************/
/* funcoes                                                              */
/************************************************************************/


static void configure_console(void){
	const usart_serial_options_t uart_serial_options = {
		.baudrate = CONF_UART_BAUDRATE,
		#if (defined CONF_UART_CHAR_LENGTH)
		.charlength = CONF_UART_CHAR_LENGTH,
		#endif
		.paritytype = CONF_UART_PARITY,
		#if (defined CONF_UART_STOP_BITS)
		.stopbits = CONF_UART_STOP_BITS,
		#endif
	};

	/* Configure console UART. */
	stdio_serial_init(CONF_UART, &uart_serial_options);

	/* Specify that stdout should not be buffered. */
	#if defined(__GNUC__)
	setbuf(stdout, NULL);
	#else
	/* Already the case in IAR's Normal DLIB default configuration: printf()
	* emits one character at a time.
	*/
	#endif
}

uint32_t usart_puts(uint8_t *pstring){
	uint32_t i ;

	while(*(pstring + i))
	if(uart_is_tx_empty(USART1))
	usart_serial_putchar(USART1, *(pstring+i++));
}

void TC_init0(Tc * TC, int ID_TC, int TC_CHANNEL, int freq){
	uint32_t ul_div;
	uint32_t ul_tcclks;
	uint32_t ul_sysclk = sysclk_get_cpu_hz();

	//uint32_t channel = 1;

	/* Configura o PMC */
	/* O TimerCounter é meio confuso
	o uC possui 3 TCs, cada TC possui 3 canais
	TC0 : ID_TC0, ID_TC1, ID_TC2
	TC1 : ID_TC3, ID_TC4, ID_TC5
	TC2 : ID_TC6, ID_TC7, ID_TC8
	*/
	pmc_enable_periph_clk(ID_TC);

	/** Configura o TC para operar em  4Mhz e interrupçcão no RC compare */
	tc_init(TC, TC_CHANNEL, TC_CMR_TCCLKS_TIMER_CLOCK5 | TC_CMR_CPCTRG);
	tc_write_rc(TC, TC_CHANNEL, 65535);

	/* Configura e ativa interrupçcão no TC canal 0 */
	/* Interrupção no C */
	NVIC_EnableIRQ((IRQn_Type) ID_TC);
	tc_enable_interrupt(TC, TC_CHANNEL, TC_IER_CPCS);

	/* Inicializa o canal 0 do TC */
}

void TC_init1(Tc * TC, int ID_TC, int TC_CHANNEL, int freq){
	
	uint32_t ul_div;
	uint32_t ul_tcclks;
	uint32_t ul_sysclk = sysclk_get_cpu_hz();

	//uint32_t channel = 1;

	/* Configura o PMC */
	/* O TimerCounter é meio confuso
	o uC possui 3 TCs, cada TC possui 3 canais
	TC0 : ID_TC0, ID_TC1, ID_TC2
	TC1 : ID_TC3, ID_TC4, ID_TC5
	TC2 : ID_TC6, ID_TC7, ID_TC8
	*/
	pmc_enable_periph_clk(ID_TC);

	/** Configura o TC para operar em  4Mhz e interrupçcão no RC compare */
	tc_find_mck_divisor(freq, ul_sysclk, &ul_div, &ul_tcclks, ul_sysclk);
	tc_init(TC, TC_CHANNEL, ul_tcclks | TC_CMR_CPCTRG);
	tc_write_rc(TC, TC_CHANNEL, (ul_sysclk / ul_div) / freq);

	/* Configura e ativa interrupçcão no TC canal 0 */
	/* Interrupção no C */
	NVIC_EnableIRQ((IRQn_Type) ID_TC);
	tc_enable_interrupt(TC, TC_CHANNEL, TC_IER_CPCS);

	/* Inicializa o canal 0 do TC */
	tc_start(TC, TC_CHANNEL);
}

static void config_ADC(void){
	afec_enable(AFEC0);

	/* struct de configuracao do AFEC */
	struct afec_config afec_cfg;

	/* Carrega parametros padrao */
	afec_get_config_defaults(&afec_cfg);

	/* Configura AFEC */
	afec_init(AFEC0, &afec_cfg);

	/* Configura trigger por software */
	afec_set_trigger(AFEC0, AFEC_TRIG_SW);

	/* configura call back */
	afec_set_callback(AFEC0, AFEC_INTERRUPT_EOC_0,	AFEC_callback, 5);

	/*** Configuracao espec?fica do canal AFEC ***/
	struct afec_ch_config afec_ch_cfg;
	afec_ch_get_config_defaults(&afec_ch_cfg);
	afec_ch_cfg.gain = AFEC_GAINVALUE_0;
	afec_ch_set_config(AFEC0, AFEC_CHANNEL, &afec_ch_cfg);

	/*
	* Calibracao:
	* Because the internal ADC offset is 0x200, it should cancel it and shift
	down to 0.
	*/
	afec_channel_set_analog_offset(AFEC0, AFEC_CHANNEL, 0x200);

	/***  Configura sensor de temperatura ***/
	struct afec_temp_sensor_config afec_temp_sensor_cfg;

	afec_temp_sensor_get_config_defaults(&afec_temp_sensor_cfg);
	afec_temp_sensor_set_config(AFEC0, &afec_temp_sensor_cfg);

	/* Selecina canal e inicializa convers?o */
	afec_channel_enable(AFEC0, AFEC_CHANNEL);
}

void io_init(void){

	//Configura TRIGGER
	pmc_enable_periph_clk(TRIGGER_PIO_ID_A);
	pio_configure(TRIGGER_PIO_A, PIO_OUTPUT_0, TRIGGER_IDX_MASK_A, PIO_DEFAULT);

	//Configura ECHO
	pmc_enable_periph_clk(ECHO_PIO_ID_A);
	pio_configure(ECHO_PIO_A, PIO_INPUT, ECHO_IDX_MASK_A, PIO_PULLUP);
	
	//Configura WATER
	pmc_enable_periph_clk(WATER_PIO_ID_A);
	pio_configure(WATER_PIO_A, PIO_INPUT, WATER_IDX_MASK_A, PIO_PULLUP);

	// Configura interrupção no pino referente ao ECHO e associa
	// função de callback caso uma interrupção for gerada
	// a função de callback é a: EchoCallback()
	pio_handler_set(ECHO_PIO_A,
	ECHO_PIO_ID_A,
	ECHO_IDX_MASK_A,
	PIO_IT_EDGE,
	EchoCallback);
	
	// Configura interrupção no pino referente ao WATER e associa
	// função de callback caso uma interrupção for gerada
	// a função de callback é a: WaterCallback()
	pio_handler_set(WATER_PIO_A,
	WATER_PIO_ID_A,
	WATER_IDX_MASK_A,
	PIO_IT_EDGE,
	WaterCallback);


	// Configura NVIC para receber interrupcoes do PIO dos botoes
	// com prioridade 4 (quanto mais próximo de 0 maior)
	NVIC_EnableIRQ(ECHO_PIO_ID_A);
	NVIC_SetPriority(ECHO_PIO_ID_A, 5);
	NVIC_EnableIRQ(WATER_PIO_ID_A);
	NVIC_SetPriority(WATER_PIO_ID_A, 5);
	
	// Ativa interrupção
	pio_enable_interrupt(ECHO_PIO_A, ECHO_IDX_MASK_A);
	pio_enable_interrupt(WATER_PIO_A, WATER_IDX_MASK_A);
	pio_get_interrupt_status(ECHO_PIO_A); // clear IRQ
	pio_get_interrupt_status(WATER_PIO_A); // clear IRQ
	
	
}

void signal_trigger(void){
	pio_set(TRIGGER_PIO_A, TRIGGER_IDX_MASK_A);
	delay_us(10);
	pio_clear(TRIGGER_PIO_A, TRIGGER_IDX_MASK_A);
}

float calc_distance_m(float ts){
	return(ts * SOUND_SPEED_MS/2);
}

static int32_t convert_adc_to_ph(int32_t ADC_value){

	int32_t ul_vol;
	int32_t ul_temp;

	/*
	* converte bits -> tens?o (Volts)
	*/
	ul_vol = ADC_value * 100 / 4096;
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	/*
	* According to datasheet, The output voltage VT = 0.72V at 27C
	* and the temperature slope dVT/dT = 2.33 mV/C
	*/
	
	return(ul_vol);
}

/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

static void task_hc04_A(void *pvParameters){
	//hc04_A_StartSemaphore = xSemaphoreCreateBinary();

	xSemaphoreTC = xSemaphoreCreateBinary();
	
	//struct para enviar os dados para a task DATA
	sensorData data;

	hc04_A_EchoQueue = xQueueCreate( 1, sizeof( float ) );

	TC_init0(TIMER_A, TIMER_ID_A, TIMER_CHANNEL_A, TIMER_FREQ_A);
	float ts;
	while(1){
		
		signal_trigger();

		if( xQueueReceive(hc04_A_EchoQueue, &ts, ( TickType_t ) 100 / portTICK_PERIOD_MS) == pdTRUE ){
			float dm = calc_distance_m(ts);
			int water_level = (int) (224 - (dm*100));
			//printf("Distancia = %d cm\t", (int) (dm*100));
			//printf("--------------------------------");
			//printf("\nNivel de agua = %d cm\n\n", (int) (224 - (dm*100)));
			//printf("--------------------------------");
			
			data.id = 1;
			data.value = 224 - (dm * 100);
			xQueueSend( xQueueData, &data, 0);
		}
		
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
}

static void task_water_cicles(void *pvParameters){
	xSemaphoreWater = xSemaphoreCreateBinary();
	xSemaphoreCounter = xSemaphoreCreateBinary();
	
	sensorData data;
	
	int cicles = 0;
	int water_flow;
	io_init();


	while(1){
		if( xSemaphoreTake(xSemaphoreWater, ( TickType_t ) 50) == pdTRUE)
		{
			//printf("ciclos = %d\t", cicles);
			cicles = cicles + 1;
			
		}

		if( xSemaphoreTake(xSemaphoreCounter, ( TickType_t ) 5) == pdTRUE){
			if(cicles <= 16){
				water_flow = 0;
			}
			else{
				water_flow = 7.2727 * cicles + 3.63636;
			}
			//printf("--------------------------------");
			//printf("\n Water flow = %d\n", water_flow);
			//printf("CICLOS = %d\t", cicles);
			cicles = 0;
			//y=7.27273\dots x+3.63636
			
			
			data.id = 2;
			data.value = water_flow;
			xQueueSend( xQueueData, &data, 0);
		}
		
	}
}

static void task_timer(void *pvParameters){
	xSemaphoreCounter = xSemaphoreCreateBinary();

	while(1){
		vTaskDelay(500 / portTICK_PERIOD_MS);
		xSemaphoreGive(xSemaphoreCounter);
		//printf("1seg");
	}
}

void task_afec(void){
	xQueueAfec = xQueueCreate( 10, sizeof( int32_t ) );
	
	sensorData data;

	config_ADC();
	afec_start_software_conversion(AFEC0);
	
	int32_t adc_value;
	int32_t ph_value;

	while (true) {
		if (xQueueReceive( xQueueAfec, &(adc_value), ( TickType_t )  2000 / portTICK_PERIOD_MS)) {
			ph_value = convert_adc_to_ph(adc_value);
			afec_start_software_conversion(AFEC0);
			//xQueueSend( xQueuePh, &ph_value, 0);
			//printf("\nPH: %d \n", ph_value);
			
			data.id = 3;
			data.value = ph_value;
			xQueueSend( xQueueData, &data, 0);
		}
		vTaskDelay(1000);
	}
}

void task_data(void){
	xQueueData = xQueueCreate( 10, sizeof( sensorData ) );
	
	sensorData data;

	while (true) {
		if (xQueueReceive( xQueueData, &(data), ( TickType_t )  500 / portTICK_PERIOD_MS)) {
			printf("---------------------------");
			printf("\nID: %d \t", data.id);
			printf("VALUE: %d \n", data.value);
			printf("---------------------------");
		}
		vTaskDelay(300);
	}
}

/************************************************************************/
/* inits                                                                */
/************************************************************************/


static void USART1_init(void){
	/* Configura USART1 Pinos */
	sysclk_enable_peripheral_clock(ID_PIOB);
	sysclk_enable_peripheral_clock(ID_PIOA);
	pio_set_peripheral(PIOB, PIO_PERIPH_D, PIO_PB4); // RX
	pio_set_peripheral(PIOA, PIO_PERIPH_A, PIO_PA21); // TX
	MATRIX->CCFG_SYSIO |= CCFG_SYSIO_SYSIO4;

	/* Configura opcoes USART */
	const sam_usart_opt_t usart_settings = {
		.baudrate       = 115200,
		.char_length    = US_MR_CHRL_8_BIT,
		.parity_type    = US_MR_PAR_NO,
		.stop_bits   	= US_MR_NBSTOP_1_BIT	,
		.channel_mode   = US_MR_CHMODE_NORMAL
	};

	/* Ativa Clock periferico USART0 */
	sysclk_enable_peripheral_clock(USART_COM_ID);

	/* Configura USART para operar em modo RS232 */
	usart_init_rs232(USART_COM, &usart_settings, sysclk_get_peripheral_hz());

	/* Enable the receiver and transmitter. */
	usart_enable_tx(USART_COM);
	usart_enable_rx(USART_COM);

	/* map printf to usart */
	ptr_put = (int (*)(void volatile*,char))&usart_serial_putchar;
	ptr_get = (void (*)(void volatile*,char*))&usart_serial_getchar;

	/* ativando interrupcao */
	usart_enable_interrupt(USART_COM, US_IER_RXRDY);
	NVIC_SetPriority(USART_COM_ID, 4);
	NVIC_EnableIRQ(USART_COM_ID);

}

/************************************************************************/
/* main                                                                 */
/************************************************************************/

/**
*  \brief FreeRTOS Real Time Kernel example entry point.
*
*  \return Unused (ANSI-C compatibility).
*/
int main(void){
	/* Initialize the SAM system */
	sysclk_init();
	board_init();
	
	WDT->WDT_MR = WDT_MR_WDDIS;

	/* Initialize the console uart */
	configure_console();
	
	/* Create task to run the hc04_A sensor*/
	if (xTaskCreate(task_hc04_A, "hc04_A", TASK_UARTTX_STACK_SIZE, NULL,
	TASK_UARTTX_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create UartTx task\r\n");
	}
	
	if (xTaskCreate(task_water_cicles, "cicles", TASK_UARTTX_STACK_SIZE, NULL,
	TASK_UARTTX_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create UartTx task\r\n");
	}

	if (xTaskCreate(task_timer, "timer", TASK_UARTTX_STACK_SIZE, NULL,
	TASK_UARTTX_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create UartTx task\r\n");
	}

	if (xTaskCreate(task_afec, "afec", TASK_UARTTX_STACK_SIZE, NULL,
	TASK_UARTTX_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create UartTx task\r\n");
	}

	if (xTaskCreate(task_data, "data", TASK_UARTTX_STACK_SIZE, NULL,
	TASK_UARTTX_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create UartTx task\r\n");
	}




	/* Start the scheduler. */
	vTaskStartScheduler();



	while(1){
	}

	/* Will only get here if there was insufficient memory to create the idle task. */
	return 0;
}
