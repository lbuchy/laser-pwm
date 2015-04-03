#include "mem.h"
#include "c_types.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "espconn.h"
#include "gpio.h"
#include "driver/pwm.h"

#define PORT 7777
#define SERVER_TIMEOUT 1000
#define MAX_CONNS 5
#define MAX_FRAME 2000

#define procTaskPrio        0
#define procTaskQueueLen    1

static volatile os_timer_t some_timer;
static struct espconn *g_pUdpServer;

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
char ssid[32] = TOSTRING(SSID);
char passwd[64] = TOSTRING(PASSWD);

// Double buffer to handle the input capture messages
// One can be serviced by a task while the other is filled by the uart
#define UART_RX_BUF_SIZE 10
static char uartRxBuf[2][UART_RX_BUF_SIZE];
// Buffer which is currently being written to by uart
static uint8_t activeBuffer = 0;
// An index into the current active buffer to the next available character
static uint8_t activeBufferChar = 0;

#define SIGNAL_NEW_PWM_MEASUREMENT 0x10

static struct station_config g_stationCfg;

/**
 * @brief The message the pwm measurement device sends
 */
typedef struct PwmMeasurement_s
{
	uint32_t frequency_hz;
	uint16_t duty_percent_div100;
} PwmMeasurement_t;

// Sizeof PwmMeasurement_t as long as it is packed
// TODO gcc's attribute __packed__ will probably guarantee that sizeof(PwmMeasurement_t)
// returns 6 but I am too lazy to find out right now
#define MSG_LENGTH 6

static PwmMeasurement_t g_lastPwmMeasurement = {0,0};

os_event_t    procTaskQueue[procTaskQueueLen];

static void ICACHE_FLASH_ATTR HandleFault()
{
	while(1)
	{}
}

static int q = 0;
static void ICACHE_FLASH_ATTR procTask(os_event_t *events)
{
	if( events->sig == SIGNAL_NEW_PWM_MEASUREMENT && events->par != 0 )
	{
		char msgScratch[40];
		// par holds the buffer that was just filled
		char* pRxBuf = (char*)events->par;
		
		// All values are MSB
		g_lastPwmMeasurement.frequency_hz  = pRxBuf[0] << 24;
		g_lastPwmMeasurement.frequency_hz += pRxBuf[1] << 16;
		g_lastPwmMeasurement.frequency_hz += pRxBuf[2] <<  8;
		g_lastPwmMeasurement.frequency_hz += pRxBuf[3];

		g_lastPwmMeasurement.duty_percent_div100  = pRxBuf[4] << 8;
		g_lastPwmMeasurement.duty_percent_div100 += pRxBuf[5];


		int payloadSize = 
			ets_sprintf( 
			&msgScratch, 
			"{\"d\":%d \"f\":%d}\n",
		       	g_lastPwmMeasurement.duty_percent_div100,
		       	g_lastPwmMeasurement.frequency_hz);
		// Send a packet to say we got a new measurement
		espconn_sent(g_pUdpServer, msgScratch, payloadSize);
	}

}

//Called when new packet comes in.
static void ICACHE_FLASH_ATTR
udpserver_recv(void *arg, char *pusrdata, unsigned short len)
{
	// Do nothing...
	ets_wdt_disable();
}

void at_recvTask()
{
	//Called from UART.
	int rxCount = (READ_PERI_REG(UART_STATUS(0)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT;
	if (!rxCount)
		return;

	char str[2];
	str[1] = '\0';
	int cnt = 0;
	for(; cnt < rxCount; cnt++)
	{
		char c = READ_PERI_REG(UART_FIFO(0)) & 0xff;
		str[0] = c;
		uart0_sendStr(str);
		if (c == '\r')
		{
			// End of message
			if (activeBufferChar != MSG_LENGTH)
			{
				// Message ended too soon!!
				// throw it away
				activeBufferChar = 0;
				activeBuffer = activeBuffer ? 0 : 1;
				continue;
			}
			
			// Correct number of bytes arrived! hurray
			// Post to the task with the buffer to parse
			system_os_post(
				procTaskPrio,
			       	SIGNAL_NEW_PWM_MEASUREMENT,
			       	(os_param_t)&uartRxBuf[activeBuffer][0]);
			// Swap the buffers and change char to 0
			activeBuffer = activeBuffer ? 0 : 1;
			activeBufferChar = 0;
		}
		else
		{
			// Middle of message
			if (activeBufferChar > MSG_LENGTH)
			{
				// This should have been an end of message...
				// wtf
				// FIXME Should reset the pwm monitor
				activeBufferChar = 0;
				activeBuffer = activeBuffer ? 0 : 1;
			}
			uartRxBuf[activeBuffer][activeBufferChar++] = c;
		}

	}
}

void SetupUdpServer(void)
{
	uart0_sendStr("Starting UDP Server...\r\n");

	// Construct the UDP server
    	g_pUdpServer = (struct espconn *)os_zalloc(sizeof(struct espconn));
	ets_memset( g_pUdpServer, 0, sizeof( struct espconn ) );
	espconn_create( g_pUdpServer );
	g_pUdpServer->type = ESPCONN_UDP;
	g_pUdpServer->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
	g_pUdpServer->proto.udp->remote_port = 7777;
	g_pUdpServer->proto.udp->local_port = 7777;
	g_pUdpServer->proto.udp->remote_ip[0] = IP1;
	g_pUdpServer->proto.udp->remote_ip[1] = IP2;
	g_pUdpServer->proto.udp->remote_ip[2] = IP3;
	g_pUdpServer->proto.udp->remote_ip[3] = IP4;

	espconn_regist_recvcb(g_pUdpServer, udpserver_recv);
	
	uart0_sendStr("Starting DHCP Client...\r\n");
	wifi_station_dhcpc_start();

	if( espconn_create( g_pUdpServer ) )
	{
		HandleFault();
	}
}

void user_init(void)
{
	gpio_init();

	// We don't really need GPIOs right now.. leave them as JTAG
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_GPIO15);
	GPIO_OUTPUT_SET(2,q);
	q = q ? 0 : 1;

	uart_init(BIT_RATE_9600, BIT_RATE_9600);
	int wifiMode = wifi_get_opmode();

	wifi_set_opmode( 0x1 /*STATION*/);

	os_memcpy(g_stationCfg.ssid, ssid, 32);
	os_memcpy(g_stationCfg.password, passwd, 64);
	g_stationCfg.bssid_set = 0;

	wifi_station_set_config(&g_stationCfg);
	wifi_station_set_auto_connect(1);
	wifi_station_disconnect();
	wifi_station_connect();

	SetupUdpServer();
	//XXX TODO figure out how to safely re-allow this.
	ets_wdt_disable();

	//Add a process
	uart0_sendStr("Registering idle task...\r\n");
	system_os_task(procTask, procTaskPrio, procTaskQueue, procTaskQueueLen);

	uart0_sendStr("Starting idle task...\r\n");
	system_os_post(procTaskPrio, 0, 0 );
}


