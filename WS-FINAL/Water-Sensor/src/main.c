 #include "asf.h"
 #include "main.h"
 #include <string.h>
 #include "bsp/include/nm_bsp.h"
 #include "driver/include/m2m_wifi.h"
 #include "socket/include/socket.h"
 #include "conf_board.h"
 
 /************************************************************************/
 /*WIFI Settings                                                         */
 /************************************************************************/
 
 #define STRING_EOL    "\r\n"
 #define STRING_HEADER "-- WINC1500 weather client example --"STRING_EOL	\
 "-- "BOARD_NAME " --"STRING_EOL	\
 "-- Compiled: "__DATE__ " "__TIME__ " --"STRING_EOL

 /** IP address of host. */
 uint32_t gu32HostIp = 0;

 /** TCP client socket handlers. */
 static SOCKET tcp_client_socket = -1;

 /** Receive buffer definition. */
 static uint8_t gau8ReceivedBuffer[MAIN_WIFI_M2M_BUFFER_SIZE] = {0};

 /** Wi-Fi status variable. */
 static bool gbConnectedWifi = false;

 /** Get host IP status variable. */
 /** Wi-Fi connection state */
 static uint8_t wifi_connected;


 /** Instance of HTTP client module. */
 static bool gbHostIpByName = false;

 /** TCP Connection status variable. */
 static bool gbTcpConnection = false;

 /** Server host name. */
 static char server_host_name[] = MAIN_SERVER_NAME;


 #define TASK_WIFI_STACK_SIZE            (4096/sizeof(portSTACK_TYPE))
 #define TASK_WIFI_STACK_PRIORITY        (tskIDLE_PRIORITY)

 extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,
 signed char *pcTaskName);
 extern void vApplicationIdleHook(void);
 extern void vApplicationTickHook(void);
 extern void vApplicationMallocFailedHook(void);
 extern void xPortSysTickHandler(void);
 
 


/************************************************************************/
/* defines                                                              */
/************************************************************************/

/* defines para o RTC */
#define YEAR        2018
#define MONTH       3
#define DAY         19
#define WEEK        12
#define HOUR        15
#define MINUTE      45
#define SECOND      0


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


/************************************************************************/
/* globals                                                              */
/************************************************************************/

volatile tank_height = 224;
volatile spin_counter = 0;
volatile flag_tc = false;

/**Variaveis para o timestamp **/
int year;
int month;
int day;
int week;
int hour;
int minute;
int second;

int id;
int value;
char timestamp[64];
int dispositivo = 1;

/** Semaforo do counter do timer do sonar */
SemaphoreHandle_t xSemaphoreCounter2;


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
	char timestamp[64];
} sensorData;



/** prototypes */
void but_callback(void);
static void ECHO_init(void);
static void USART1_init(void);
uint32_t usart_puts(uint8_t *pstring);
void io_init(void);
void TC_init0(Tc * TC, int ID_TC, int TC_CHANNEL, int freq);
void RTC_init(void);

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

void RTC_Handler(void)
{
	printf("ALBBBBBBBBBBBBBBRME");
	uint32_t ul_status = rtc_get_status(RTC);


	if ((ul_status & RTC_SR_SEC) == RTC_SR_ALARM) {
		printf("ALAAAAAAAAAAAAAAAARME");
		rtc_clear_status(RTC, RTC_SCCR_SECCLR);
		
		
		//rtc_get_time(ID_RTC);
	}
	
	rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
	rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
	rtc_clear_status(RTC, RTC_SCCR_CALCLR);
	rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);
	rtc_clear_status(RTC, RTC_SCCR_SECCLR);
	
}


/************************************************************************/
/* funcoes                                                              */
/************************************************************************/

 /**
 * \brief Configure UART console.
 */
 static void configure_console(void)
 {
	 const usart_serial_options_t uart_serial_options = {
		 .baudrate =		CONF_UART_BAUDRATE,
		 .charlength =	CONF_UART_CHAR_LENGTH,
		 .paritytype =	CONF_UART_PARITY,
		 .stopbits =		CONF_UART_STOP_BITS,
	 };

	 /* Configure UART console. */
	 sysclk_enable_peripheral_clock(CONSOLE_UART_ID);
	 stdio_serial_init(CONF_UART, &uart_serial_options);
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

void RTC_init(){
	/* Configura o PMC */
	pmc_enable_periph_clk(ID_RTC);

	/* Default RTC configuration, 24-hour mode */
	rtc_set_hour_mode(RTC, 0);

	/* Configura data e hora manualmente */
	//rtc_set_date(RTC, YEAR, MONTH, DAY, WEEK);
	//rtc_set_time(RTC, HOUR, MINUTE, SECOND);

	/* Configure RTC interrupts */
	//NVIC_DisableIRQ(RTC_IRQn);
	//NVIC_ClearPendingIRQ(RTC_IRQn);
	//NVIC_SetPriority(RTC_IRQn, 5);
	//NVIC_EnableIRQ(RTC_IRQn);

	/* Ativa interrupcao via alarme */
	//rtc_enable_interrupt(RTC, RTC_IER_ALREN);
	

	


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

void get_time(char *timestamp){
	rtc_get_date(RTC, &year, &month, &day, &week);
	rtc_get_time(RTC, &hour, &minute, &second);
	sprintf(timestamp, "%d/%d/%d - %02d:%02d:%02d", day, month, year, hour, minute, second);

}

/************************************************************************/
/* funcoes WIFI                                                             */
/************************************************************************/

 /*
 * Check whether "cp" is a valid ascii representation
 * of an Internet address and convert to a binary address.
 * Returns 1 if the address is valid, 0 if not.
 * This replaces inet_addr, the return value from which
 * cannot distinguish between failure and a local broadcast address.
 */
 /* http://www.cs.cmu.edu/afs/cs/academic/class/15213-f00/unpv12e/libfree/inet_aton.c */
 int inet_aton(const char *cp, in_addr *ap)
 {
	 int dots = 0;
	 register u_long acc = 0, addr = 0;

	 do {
		 register char cc = *cp;

		 switch (cc) {
			 case '0':
			 case '1':
			 case '2':
			 case '3':
			 case '4':
			 case '5':
			 case '6':
			 case '7':
			 case '8':
			 case '9':
			 acc = acc * 10 + (cc - '0');
			 break;

			 case '.':
			 if (++dots > 3) {
				 return 0;
			 }
			 /* Fall through */

			 case '\0':
			 if (acc > 255) {
				 return 0;
			 }
			 addr = addr << 8 | acc;
			 acc = 0;
			 break;

			 default:
			 return 0;
		 }
	 } while (*cp++) ;

	 /* Normalize the address */
	 if (dots < 3) {
		 addr <<= 8 * (3 - dots) ;
	 }

	 /* Store it if requested */
	 if (ap) {
		 ap->s_addr = _htonl(addr);
	 }

	 return 1;
 }


 /**
 * \brief Callback function of IP address.
 *
 * \param[in] hostName Domain name.
 * \param[in] hostIp Server IP.
 *
 * \return None.
 */
 static void resolve_cb(uint8_t *hostName, uint32_t hostIp)
 {
	 gu32HostIp = hostIp;
	 gbHostIpByName = true;
	 printf("resolve_cb: %s IP address is %d.%d.%d.%d\r\n\r\n", hostName,
	 (int)IPV4_BYTE(hostIp, 0), (int)IPV4_BYTE(hostIp, 1),
	 (int)IPV4_BYTE(hostIp, 2), (int)IPV4_BYTE(hostIp, 3));
 }

 /**
 * \brief Callback function of TCP client socket.
 *
 * \param[in] sock socket handler.
 * \param[in] u8Msg Type of Socket notification
 * \param[in] pvMsg A structure contains notification informations.gbTcpConnection
 *
 * \return None.
 */
 static void socket_cb(SOCKET sock, uint8_t u8Msg, void *pvMsg)
 {
	 
	 /* Check for socket event on TCP socket. */
	 if (sock == tcp_client_socket) {
		 
		 switch (u8Msg) {
			 case SOCKET_MSG_CONNECT:
			 {
				 //printf("\nsocket_msg_connect\n");
				 if (gbTcpConnection) {
					 memset(gau8ReceivedBuffer, 0, sizeof(gau8ReceivedBuffer));
					 char buffer[270];
					 
					 //json
					 sprintf(buffer, "{\"%s\":%d,\"%s\":%d,\"%s\":%d ,\"%s\":\"%s\"}", "dispositivo" , dispositivo, "id", id, "value", value, "timestamp", timestamp);
						 
					 //"Content-Type: application/x-www-form-urlencoded\n\n",
					 sprintf((char *)gau8ReceivedBuffer, "%sContent-Length: %d\n%s%s", "PUT /1 HTTP/1.0\n", strlen(buffer),
					 "Content-Type: application/json\n\n",
					 buffer);
					 printf("\n\n");
					 printf("\n----------------------------------------------\n");
					 //printf(gau8ReceivedBuffer);
					 printf(buffer);
					 printf("\n%s", timestamp);
					 printf("\n----------------------------------------------\n");
					 printf("\n\n");

					 tstrSocketConnectMsg *pstrConnect = (tstrSocketConnectMsg *)pvMsg;
					 if (pstrConnect && pstrConnect->s8Error >= SOCK_ERR_NO_ERROR) {
						 //printf("send \n");
						 send(tcp_client_socket, gau8ReceivedBuffer, strlen((char *)gau8ReceivedBuffer), 0);

						 memset(gau8ReceivedBuffer, 0, MAIN_WIFI_M2M_BUFFER_SIZE);
						 recv(tcp_client_socket, &gau8ReceivedBuffer[0], MAIN_WIFI_M2M_BUFFER_SIZE, 0);
						////////sinalizacao mandou
						LED_Toggle(LED0);
						vTaskDelay(1000);
						LED_Toggle(LED0);
						 } else {
						 printf("\nsocket_cb: connect error!\r\n");
						 gbTcpConnection = false;
						 close(tcp_client_socket);
						 tcp_client_socket = -1;
					 }
				 }
			 }
			 break;
			 


			 case SOCKET_MSG_RECV:
			 {
				 char *pcIndxPtr;
				 char *pcEndPtr;

				 tstrSocketRecvMsg *pstrRecv = (tstrSocketRecvMsg *)pvMsg;
				 if (pstrRecv && pstrRecv->s16BufferSize > 0) {
					 //printf(pstrRecv->pu8Buffer);
					 
					 memset(gau8ReceivedBuffer, 0, sizeof(gau8ReceivedBuffer));
					 recv(tcp_client_socket, &gau8ReceivedBuffer[0], MAIN_WIFI_M2M_BUFFER_SIZE, 0);
					 } else {
					 //printf("socket_cb: recv error!\r\n");
					 close(tcp_client_socket);
					 tcp_client_socket = -1;
				 }
			 }
			 break;

			 default:
			 break;
		 }
	 }
 }

 static void set_dev_name_to_mac(uint8_t *name, uint8_t *mac_addr)
 {
	 /* Name must be in the format WINC1500_00:00 */
	 uint16 len;

	 len = m2m_strlen(name);
	 if (len >= 5) {
		 name[len - 1] = MAIN_HEX2ASCII((mac_addr[5] >> 0) & 0x0f);
		 name[len - 2] = MAIN_HEX2ASCII((mac_addr[5] >> 4) & 0x0f);
		 name[len - 4] = MAIN_HEX2ASCII((mac_addr[4] >> 0) & 0x0f);
		 name[len - 5] = MAIN_HEX2ASCII((mac_addr[4] >> 4) & 0x0f);
	 }
 }

 /**
 * \brief Callback to get the Wi-Fi status update.
 *
 * \param[in] u8MsgType Type of Wi-Fi notification.
 * \param[in] pvMsg A pointer to a buffer containing the notification parameters.
 *
 * \return None.
 */
 static void wifi_cb(uint8_t u8MsgType, void *pvMsg)
 {
	 switch (u8MsgType) {
		 case M2M_WIFI_RESP_CON_STATE_CHANGED:
		 {
			 tstrM2mWifiStateChanged *pstrWifiState = (tstrM2mWifiStateChanged *)pvMsg;
			 if (pstrWifiState->u8CurrState == M2M_WIFI_CONNECTED) {
				 printf("wifi_cb: M2M_WIFI_CONNECTED\r\n");
				 m2m_wifi_request_dhcp_client();
				 } else if (pstrWifiState->u8CurrState == M2M_WIFI_DISCONNECTED) {
				 printf("wifi_cb: M2M_WIFI_DISCONNECTED\r\n");
				 gbConnectedWifi = false;
				 wifi_connected = 0;
			 }

			 break;
		 }

		 case M2M_WIFI_REQ_DHCP_CONF:
		 {
			 uint8_t *pu8IPAddress = (uint8_t *)pvMsg;
			 printf("wifi_cb: IP address is %u.%u.%u.%u\r\n",
			 pu8IPAddress[0], pu8IPAddress[1], pu8IPAddress[2], pu8IPAddress[3]);
			 wifi_connected = M2M_WIFI_CONNECTED;
			 
			 /* Obtain the IP Address by network name */
			 //gethostbyname((uint8_t *)server_host_name);
			 break;
		 }
		 
		 case M2M_WIFI_RESP_GET_SYS_TIME:
		 /*Initial first callback will be provided by the WINC itself on the first communication with NTP */
		 {
			 tstrSystemTime *strSysTime_now = (tstrSystemTime *)pvMsg;
			 
			 /* Print the hour, minute and second.
			 * GMT is the time at Greenwich Meridian.
			 */
			 
			 printf("socket_cb: Year: %d, Month: %d, The GMT time is %u:%02u:%02u\r\n",
			 strSysTime_now->u16Year,
			 strSysTime_now->u8Month,
			 strSysTime_now->u8Hour,           /* hour (86400 equals secs per day) */
			 strSysTime_now->u8Minute,         /* minute (3600 equals secs per minute) */
			 strSysTime_now->u8Second);        /* second */
			 
			 year = strSysTime_now->u16Year;
			 month = strSysTime_now->u8Month;
			 day = strSysTime_now->u8Day;
			 hour = (strSysTime_now->u8Hour) - 3;
			 minute = strSysTime_now->u8Minute;
			 second = strSysTime_now->u8Second;
			 
			 rtc_set_time(RTC, hour, minute, second);
			 rtc_set_date(RTC, YEAR, MONTH, DAY, NULL);
			 
			 break;
		 }

		 default:
		 {
			 break;
		 }
	 }
 }


/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

static void task_hc04_A(void *pvParameters){
	//hc04_A_StartSemaphore = xSemaphoreCreateBinary();

	xSemaphoreCounter2 = xSemaphoreCreateBinary();
	
	//struct para enviar os dados para a task DATA
	sensorData data;

	hc04_A_EchoQueue = xQueueCreate( 1, sizeof( float ) );

	TC_init0(TIMER_A, TIMER_ID_A, TIMER_CHANNEL_A, TIMER_FREQ_A);
	float ts;
	char timestamp[64];
	
	while(1){
		signal_trigger();

		

		
		if( xQueueReceive(hc04_A_EchoQueue, &ts, ( TickType_t ) 100 / portTICK_PERIOD_MS) == pdTRUE ){
			float dm = calc_distance_m(ts);
			int water_level = (int) (45 - (dm*100));
			//printf("Distancia = %d cm\t", (int) (dm*100));
			//printf("--------------------------------");
			//printf("\nNivel de agua = %d cm\n\n", (int) (45 - (dm*100)));
			//printf("--------------------------------");
			

			if( xSemaphoreTake(xSemaphoreCounter2, ( TickType_t ) 5) == pdTRUE){
				get_time(&timestamp);
				data.id = 1;
				data.value = 45 - (dm * 100);
				strcpy(data.timestamp, timestamp);
				xQueueSend( xQueueData, &data, 0);
				
			}
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
	char timestamp[64];
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
			
			get_time(&timestamp);
			data.id = 2;
			data.value = water_flow;
			strcpy(data.timestamp, timestamp);
			//printf(data.timestamp);
			xQueueSend( xQueueData, &data, 0);
		}
		
	}
}

static void task_timer(void *pvParameters){
	//funcao para contar 1 segundo para o sensor de ciclos de agua
	xSemaphoreCounter = xSemaphoreCreateBinary();

	while(1){
		vTaskDelay(500 / portTICK_PERIOD_MS);
		xSemaphoreGive(xSemaphoreCounter);
		//printf("1seg");
	}
}

static void task_timer2(void *pvParameters){
	//funcao para contar 10 segundos para  timer do sonar
	xSemaphoreCounter2 = xSemaphoreCreateBinary();

	while(1){
		vTaskDelay(1000 / portTICK_PERIOD_MS); //timer sonar
		xSemaphoreGive(xSemaphoreCounter2);
		//printf("10 seg");
	}
}

void task_afec(void){
	xQueueAfec = xQueueCreate( 10, sizeof( int32_t ) );
	
	sensorData data;

	config_ADC();
	afec_start_software_conversion(AFEC0);
	
	int32_t adc_value;
	int32_t ph_value;
	char timestamp[64];

	while (true) {
		if (xQueueReceive( xQueueAfec, &(adc_value), ( TickType_t )  2000 / portTICK_PERIOD_MS)) {
			ph_value = (int) (convert_adc_to_ph(adc_value))/10;
			afec_start_software_conversion(AFEC0);
			//xQueueSend( xQueuePh, &ph_value, 0);
			//printf("\nPH: %d \n", ph_value);
			
			get_time(&timestamp);
			data.id = 3;
			data.value = ph_value;
			strcpy(data.timestamp, timestamp);
			xQueueSend( xQueueData, &data, 0);
		}
		vTaskDelay(200);
	}
}

void task_data(void){
	//xQueueData = xQueueCreate( 10, sizeof( sensorData ) );
	
	sensorData data;

	while (true) {
		//if (xQueueReceive( xQueueData, &(data), ( TickType_t )  500 / portTICK_PERIOD_MS)) {
			

			//printf("---------------------------");
			//printf("\nID: %d \t", data.id);
			//printf("VALUE: %d \n", data.value);
			//printf("TIME: %s \n", data.timestamp);
			//printf("---------------------------");
		//}
		//vTaskDelay(300);
	}
}


 /**
 * \brief This task, when activated, send every ten seconds on debug UART
 * the whole report of free heap and total tasks status
 */
 static void task_monitor(void *pvParameters)
 {
	 static portCHAR szList[256];
	 UNUSED(pvParameters);

	 for (;;) {
		 printf("--- task ## %u", (unsigned int)uxTaskGetNumberOfTasks());
		 vTaskList((signed portCHAR *)szList);
		 printf(szList);
		 vTaskDelay(1000);
	 }
 }

 static void task_wifi(void *pvParameters) {
	 
	 
	 xQueueData = xQueueCreate( 10, sizeof( sensorData ) );
	 sensorData data;
	 
	 
	 tstrWifiInitParam param;
	 int8_t ret;
	 uint8_t mac_addr[6];
	 uint8_t u8IsMacAddrValid;
	 struct sockaddr_in addr_in;
	 
	 /* Initialize the BSP. */
	 nm_bsp_init();
	 
	 /* Initialize Wi-Fi parameters structure. */
	 memset((uint8_t *)&param, 0, sizeof(tstrWifiInitParam));

	 /* Initialize Wi-Fi driver with data and status callbacks. */
	 param.pfAppWifiCb = wifi_cb;
	 ret = m2m_wifi_init(&param);
	 if (M2M_SUCCESS != ret) {
		 printf("main: m2m_wifi_init call error!(%d)\r\n", ret);
		 while (1) {
			 
		 }
	 }
	 
	 /* Initialize socket module. */
	 socketInit();

	 /* Register socket callback function. */
	 registerSocketCallback(socket_cb, resolve_cb);

	 /* Connect to router. */
	 printf("main: connecting to WiFi AP %s...\r\n", (char *)MAIN_WLAN_SSID);
	 m2m_wifi_connect((char *)MAIN_WLAN_SSID, sizeof(MAIN_WLAN_SSID), MAIN_WLAN_AUTH, (char *)MAIN_WLAN_PSK, M2M_WIFI_CH_ALL);

	 addr_in.sin_family = AF_INET;
	 addr_in.sin_port = _htons(MAIN_SERVER_PORT);
	 inet_aton(MAIN_SERVER_NAME, &addr_in.sin_addr);
	 printf("Inet aton : %d", addr_in.sin_addr);
	 
	 while(1){
		 if (xQueueReceive( xQueueData, &(data), ( TickType_t )  500 / portTICK_PERIOD_MS)) {
			id = data.id;
			value = data.value;
			strcpy(data.timestamp, timestamp);
			 m2m_wifi_handle_events(NULL);

			 if (wifi_connected == M2M_WIFI_CONNECTED) {
				 /* Open client socket. */
				 if (tcp_client_socket < 0) {
					 //printf("socket init \n");
					 //printf("\n%d/%d/%d, The GMT time is %u:%02u:%02u\r\n", day, month, year, hour, minute, second);
					 if ((tcp_client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
						 printf("main: failed to create TCP client socket error!\r\n");
						 continue;
					 }

					 /* Connect server */
					 printf("\nsocket connecting\n");
				 
					 if (connect(tcp_client_socket, (struct sockaddr *)&addr_in, sizeof(struct sockaddr_in)) != SOCK_ERR_NO_ERROR) {
						 close(tcp_client_socket);
						 tcp_client_socket = -1;
						 printf("error\n");
						 }else{
						 gbTcpConnection = true;
					 }
				 }
			 }
			 //vTaskDelay(100);
		 }
		 //vTaskDelay(300);
		}
 }
 
 
 /**
 * \brief Main application function.
 *
 * Initialize system, UART console, network then start weather client.
 *
 * \return Program return value.
 */
 int main(void)
 {
	 /* Initialize the board. */
	 sysclk_init();
	 board_init();

	 /* Initialize the UART console. */
	 configure_console();
	 RTC_init();
	 
	 printf(STRING_HEADER);
	 
	 
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
	 	
	if (xTaskCreate(task_timer2, "timer2", TASK_UARTTX_STACK_SIZE, NULL,
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
	 
	 if (xTaskCreate(task_wifi, "Wifi", TASK_WIFI_STACK_SIZE, NULL,
	 TASK_WIFI_STACK_PRIORITY, NULL) != pdPASS) {
		 printf("Failed to create Wifi task\r\n");
	 }

	 vTaskStartScheduler();
	 
	 while(1) {};
	 return 0;

 }
