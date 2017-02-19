#include <platform_opts.h>

#ifdef CONFIG_AT_USR

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "at_cmd/log_service.h"
#include "at_cmd/atcmd_wifi.h"
#include <lwip_netconf.h>
#include "tcpip.h"
#include <dhcp/dhcps.h>
#include <wifi/wifi_conf.h>
#include <wifi/wifi_util.h>
#include "tcm_heap.h"
#include "user/atcmd_user.h"

#include "sleep_ex_api.h"

//#include "lwip/err.h"
//#include "arch/cc.h"
//#include "lwip/mem.h"
//#include "lwip/tcp.h"
#include "lwip/tcp_impl.h"
//#include "lwip/udp.h"

rtw_mode_t wifi_mode = RTW_MODE_STA;
ext_server_setings ext_serv = {0,{0}}; //{ PLAY_PORT, { PLAY_SERVER }};

#define DEBUG_AT_USER_LEVEL 1

/******************************************************************************/
/*
#define	_AT_WLAN_SET_SSID_          "ATW0"
#define	_AT_WLAN_SET_PASSPHRASE_    "ATW1"
#define	_AT_WLAN_SET_KEY_ID_        "ATW2"
#define	_AT_WLAN_JOIN_NET_          "ATWC"
#define	_AT_WLAN_SET_MP3_URL_       "ATWS"
*/
//extern struct netif xnetif[NET_IF_NUM];

/* fastconnect use wifi AT command. Not init_wifi_struct when log service disabled
 * static initialize all values for using fastconnect when log service disabled
 */
static rtw_network_info_t wifi = {
	{0},    // ssid
	{0},    // bssid
	0,      // security
	NULL,   // password
	0,      // password len
	-1      // key id
};

static rtw_ap_info_t ap = {0};
static unsigned char password[65] = {0};

int connect_cfg_read(void)
{
	bzero(&ext_serv, sizeof(ext_serv));
	if(flash_read_cfg(&ext_serv, 0x5000, sizeof(ext_serv)) >= sizeof(ext_serv.port) + 2) {
		ext_serv.port = 10201;
		strcpy(ext_serv.url, "sesp8266/test.mp3");
	}
	return ext_serv.port;
}


_WEAK void connect_start(void)
{
#ifdef CONFIG_DEBUG_LOG
	printf("Time at start %d ms.\n", xTaskGetTickCount());
#endif
}

_WEAK void connect_close(void)
{
}

static void init_wifi_struct(void)
{
	memset(wifi.ssid.val, 0, sizeof(wifi.ssid.val));
	memset(wifi.bssid.octet, 0, ETH_ALEN);	
	memset(password, 0, sizeof(password));
	wifi.ssid.len = 0;
	wifi.password = NULL;
	wifi.password_len = 0;
	wifi.key_id = -1;
	memset(ap.ssid.val, 0, sizeof(ap.ssid.val));
	ap.ssid.len = 0;
	ap.password = NULL;
	ap.password_len = 0;
	ap.channel = 1;
}

void fATW0(void *arg){
	if(!arg){
		printf("ATW0: Usage: ATW0=SSID\n");
		goto exit;
	}
#if	DEBUG_AT_USER_LEVEL > 1
	printf("ATW0: %s\n", (char*)arg);
#endif
	strcpy((char *)wifi.ssid.val, (char*)arg);
	wifi.ssid.len = strlen((char*)arg);
exit:
	return;
}

void fATW1(void *arg){
#if	DEBUG_AT_USER_LEVEL > 1
    printf("ATW1: %s\n", (char*)arg);
#endif
	strcpy((char *)password, (char*)arg);
	wifi.password = password;
	wifi.password_len = strlen((char*)arg);
	return;	
}

void fATW2(void *arg){
#if	DEBUG_AT_USER_LEVEL > 1
	printf("ATW2: %s\n", (char*)arg);
#endif
	if((strlen((const char *)arg) != 1 ) || (*(char*)arg <'0' ||*(char*)arg >'3')) {
		printf("ATW2: Wrong WEP key id. Must be one of 0,1,2, or 3.\n");
		return;
	}
	wifi.key_id = atoi((const char *)(arg));
	return;
}

// Test
void fATST(void *arg){
	extern u8 __HeapLimit, __StackTop;
	extern struct Heap g_tcm_heap;
		//DBG_INFO_MSG_ON(_DBG_TCM_HEAP_); // On Debug TCM MEM
#if	DEBUG_AT_USER_LEVEL > 1
		printf("ATST: Mem info:\n");
#endif
//		vPortFree(pvPortMalloc(4)); // Init RAM heap
		printf("\nCLK CPU\t\t%d Hz\nRAM heap\t%d bytes\nTCM heap\t%d bytes\n",
				HalGetCpuClk(), xPortGetFreeHeapSize(), tcm_heap_freeSpace());
		dump_mem_block_list();
		u32 saved = ConfigDebugInfo;
		DBG_INFO_MSG_ON(_DBG_TCM_HEAP_); // On Debug TCM MEM
		tcm_heap_dump();
		ConfigDebugInfo = saved;
		printf("\n");
#if (configGENERATE_RUN_TIME_STATS == 1)
		char *cBuffer = pvPortMalloc(512);
		if(cBuffer != NULL) {
			vTaskGetRunTimeStats((char *)cBuffer);
			printf("%s", cBuffer);
		}
		vPortFree(cBuffer);
#endif
}

// Set server, Close connect
void fATWS(void *arg){
	int   argc           = 0;
	char *argv[MAX_ARGC] = {0};
	if(arg) {
       	argc = parse_param(arg, argv);
    	if (argc == 2) {
    		if(argv[1][0] == '?') {
			    printf("ATWS: %s,%d\n", ext_serv.url, ext_serv.port);
    		    return;
    		}
    		else if(strcmp(argv[1], "open") == 0) {
    		    printf("ATWS: open %s:%d\n", ext_serv.url, ext_serv.port);
    			connect_close();
    		    return;
    		}
    		else if(strcmp(argv[1], "close") == 0) {
    		    printf("ATWS: close\n");
    			connect_close();
    		    return;
    		}
    		else if(strcmp(argv[1], "read") == 0) {
    			connect_cfg_read();
    			connect_start();
    		    return;
    		}
    		else if(strcmp(argv[1], "save") == 0) {
			    printf("ATWS: %s,%d\n", ext_serv.url, ext_serv.port);
    			if(flash_write_cfg(&ext_serv, 0x5000, strlen(ext_serv.port) + strlen(ext_serv.url)))
    			    printf("ATWS: saved\n", ext_serv.url, ext_serv.port);
    		    return;
    		}
    	}
    	else if (argc >= 3 ) {
    		strcpy((char *)ext_serv.url, (char*)argv[1]);
        	ext_serv.port = atoi((char*)argv[2]);
        	printf("ATWS: %s,%d\r\n", ext_serv.url, ext_serv.port);
        	connect_start();
        	return;
    	}
	}	
	printf("ATWS: Usage: ATWS=URL,PORT or ATWS=close, ATWS=read, ATWS=save\n");
}


void fATWC(void *arg){
	int mode, ret;
	unsigned long tick1 = xTaskGetTickCount();
	unsigned long tick2, tick3;
	char empty_bssid[6] = {0}, assoc_by_bssid = 0;
	
	connect_close();
#if	DEBUG_AT_USER_LEVEL > 1
	printf("ATWC: Connect to AP...\n");
#endif
	if(memcmp (wifi.bssid.octet, empty_bssid, 6))
		assoc_by_bssid = 1;
	else if(wifi.ssid.val[0] == 0){
		printf("ATWC: Error: SSID can't be empty\n");
		ret = RTW_BADARG;
		goto EXIT;
	}
	if(wifi.password != NULL){
		if((wifi.key_id >= 0)&&(wifi.key_id <= 3)) {
			wifi.security_type = RTW_SECURITY_WEP_PSK;
		}
		else{
			wifi.security_type = RTW_SECURITY_WPA2_AES_PSK;
		}
	}
	else{
		wifi.security_type = RTW_SECURITY_OPEN;
	}
	//Check if in AP mode
	wext_get_mode(WLAN0_NAME, &mode);
	if(mode == IW_MODE_MASTER) {
		dhcps_deinit();
		wifi_off();
		vTaskDelay(wifi_test_timeout_step_ms/portTICK_RATE_MS);
		if (wifi_on(RTW_MODE_STA) < 0){
			printf("ERROR: Wifi on failed!\n");
                        ret = RTW_ERROR;
			goto EXIT;
		}
	}

	///wifi_set_channel(1);

	if(assoc_by_bssid){
		printf("Joining BSS by BSSID "MAC_FMT" ...\n", MAC_ARG(wifi.bssid.octet));
		ret = wifi_connect_bssid(wifi.bssid.octet, (char*)wifi.ssid.val, wifi.security_type, (char*)wifi.password, 
						ETH_ALEN, wifi.ssid.len, wifi.password_len, wifi.key_id, NULL);		
	} else {
		printf("Joining BSS by SSID %s...\n", (char*)wifi.ssid.val);
		ret = wifi_connect((char*)wifi.ssid.val, wifi.security_type, (char*)wifi.password, wifi.ssid.len,
						wifi.password_len, wifi.key_id, NULL);
	}
	
	if(ret!= RTW_SUCCESS){
		printf("ERROR: Can't connect to AP\n");
		goto EXIT;
	}
	tick2 = xTaskGetTickCount();
	printf("Connected after %dms\n", (tick2-tick1));
	/* Start DHCPClient */
	LwIP_DHCP(0, DHCP_START);
	tick3 = xTaskGetTickCount();
	printf("Got IP after %dms\n", (tick3-tick1));
	printf("\n\r");
//#if CONFIG_WLAN_CONNECT_CB
	connect_start();
EXIT:
	init_wifi_struct( );
}

void fATWD(void *arg){
	int timeout = wifi_test_timeout_ms/wifi_test_timeout_step_ms;;
	char essid[33];
	int ret = RTW_SUCCESS;

	connect_close();
#if	DEBUG_AT_USER_LEVEL > 1
	printf("ATWD: Disconnect...\n");
#endif
	printf("Deassociating AP ...\n");
	if(wext_get_ssid(WLAN0_NAME, (unsigned char *) essid) < 0) {
		printf("WIFI disconnected\n");
		goto exit;
	}

	if((ret = wifi_disconnect()) < 0) {
		printf("ERROR: Operation failed!\n");
		goto exit;
	}

	while(1) {
		if(wext_get_ssid(WLAN0_NAME, (unsigned char *) essid) < 0) {
			printf("WIFI disconnected\n");
			break;
		}

		if(timeout == 0) {
			printf("ERROR: Deassoc timeout!\n");
			ret = RTW_TIMEOUT;
			break;
		}

		vTaskDelay(wifi_test_timeout_step_ms/portTICK_RATE_MS);
		timeout --;
	}
    printf("\n\r");
exit:
    init_wifi_struct( );
	return;
}

// Dump register
void fATSD(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};

#if	DEBUG_AT_USER_LEVEL > 1
	printf("ATSD: dump registers\n");
#endif
	if(!arg){
		printf("ATSD: Usage: ATSD=REGISTER");
		return;
	}
	argc = parse_param(arg, argv);
	if(argc == 2 || argc == 3)
		CmdDumpWord(argc-1, (unsigned char**)(argv+1));
}

void fATSW(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};

#if	DEBUG_AT_USER_LEVEL > 1
	printf("ATSW: write register\n");
#endif
	if(!arg){
		printf("ATSW: Usage: ATSW=REGISTER,DATA");
		return;
	}
	argc = parse_param(arg, argv);
	if(argc == 2 || argc == 3)
		CmdWriteWord(argc-1, (unsigned char**)(argv+1));
}

// Close connections
void fATOF(void *arg)
{
#if	DEBUG_AT_USER_LEVEL > 1
	printf("ATOF: Close connections...\n");
#endif
	connect_close();
}

// Open connections
void fATON(void *arg)
{
#if	DEBUG_AT_USER_LEVEL > 1
	printf("ATON: Open connections...\n");
#endif
	connect_start();
}

/* Get one byte from the 4-byte address */
#define ip4_addr1(ipaddr) (((u8_t*)(ipaddr))[0])
#define ip4_addr2(ipaddr) (((u8_t*)(ipaddr))[1])
#define ip4_addr3(ipaddr) (((u8_t*)(ipaddr))[2])
#define ip4_addr4(ipaddr) (((u8_t*)(ipaddr))[3])
/* These are cast to u16_t, with the intent that they are often arguments
 * to printf using the U16_F format from cc.h. */
#define ip4_addr1_16(ipaddr) ((u16_t)ip4_addr1(ipaddr))
#define ip4_addr2_16(ipaddr) ((u16_t)ip4_addr2(ipaddr))
#define ip4_addr3_16(ipaddr) ((u16_t)ip4_addr3(ipaddr))
#define ip4_addr4_16(ipaddr) ((u16_t)ip4_addr4(ipaddr))

#define IP2STR(ipaddr) ip4_addr1_16(ipaddr), \
    ip4_addr2_16(ipaddr), \
    ip4_addr3_16(ipaddr), \
    ip4_addr4_16(ipaddr)

#define IPSTR "%d.%d.%d.%d"

extern const char * const tcp_state_str[];
/*
static const char * const tcp_state_str[] = {
  "CLOSED",
  "LISTEN",
  "SYN_SENT",
  "SYN_RCVD",
  "ESTABLISHED",
  "FIN_WAIT_1",
  "FIN_WAIT_2",
  "CLOSE_WAIT",
  "CLOSING",
  "LAST_ACK",
  "TIME_WAIT"
};
*/
/******************************************************************************
 * FunctionName : debug
 * Parameters   :
 * Returns      :
*******************************************************************************/
void print_udp_pcb(void)
{
  struct udp_pcb *pcb;
  bool prt_none = true;
  rtl_printf("UDP pcbs:\n");
  for(pcb = udp_pcbs; pcb != NULL; pcb = pcb->next) {
	  rtl_printf("flg:%02x\t" IPSTR ":%d\t" IPSTR ":%d\trecv:%p\n", pcb->flags, IP2STR(&pcb->local_ip), pcb->local_port, IP2STR(&pcb->remote_ip), pcb->remote_port, pcb->recv );
	  prt_none = false;
  }
  if(prt_none) rtl_printf("none\n");
}
/******************************************************************************
 * FunctionName : debug
 * Parameters   :
 * Returns      :
*******************************************************************************/
void print_tcp_pcb(void)
{
  struct tcp_pcb *pcb;
  rtl_printf("Active PCB states:\n");
  bool prt_none = true;
  for(pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) {
     rtl_printf("Port %d|%d\tflg:%02x\ttmr:%p\t%s\n", pcb->local_port, pcb->remote_port, pcb->flags, pcb->tmr, tcp_state_str[pcb->state]);
     prt_none = false;
  }
  if(prt_none) rtl_printf("none\n");
  rtl_printf("Listen PCB states:\n");
  prt_none = true;
  for(pcb = (struct tcp_pcb *)tcp_listen_pcbs.pcbs; pcb != NULL; pcb = pcb->next) {
    rtl_printf("Port %d|%d\tflg:%02x\ttmr:%p\t%s\n", pcb->local_port, pcb->remote_port, pcb->flags, pcb->tmr, tcp_state_str[pcb->state]);
    prt_none = false;
  }
  if(prt_none) rtl_printf("none\n");
  rtl_printf("TIME-WAIT PCB states:\n");
  prt_none = true;
  for(pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) {
    rtl_printf("Port %d|%d\tflg:%02x\ttmr:%p\t%s\n", pcb->local_port, pcb->remote_port, pcb->flags, pcb->tmr, tcp_state_str[pcb->state]);
    prt_none = false;
  }
  if(prt_none) rtl_printf("none\n");
}
/******************************************************************************
 * FunctionName : debug
 * Parameters   :
 * Returns      :
*******************************************************************************/
//------------------------------------------------------------------------------
void fATLW(void *arg) 	// Info Lwip
{
#if	DEBUG_AT_USER_LEVEL > 1
	printf("ATLW: Lwip pcb Info\n");
#endif
	print_udp_pcb();
	print_tcp_pcb();
}

void fATDS(void *arg) 	// Deep sleep
{
#if	DEBUG_AT_USER_LEVEL > 1
	printf("ATDS: Deep sleep\n");
#endif
/*
    // turn off log uart
    sys_log_uart_off();
    // initialize wakeup pin at PB_1
    gpio_t gpio_wake;
    gpio_init(&gpio_wake, PB_1);
    gpio_dir(&gpio_wake, PIN_INPUT);
    gpio_mode(&gpio_wake, PullDown);

    // enter deep sleep
    deepsleep_ex(DSLEEP_WAKEUP_BY_GPIO | DSLEEP_WAKEUP_BY_TIMER, 10000); */
    deepsleep_ex(DSLEEP_WAKEUP_BY_TIMER, 10000);

//	standby_wakeup_event_add(STANDBY_WAKEUP_BY_STIMER, 10000, 0);
//    deepstandby_ex();

//    sleep_ex(SLEEP_WAKEUP_BY_STIMER, 8000); // sleep_ex can't be put in irq handler
//	release_wakelock(WAKELOCK_OS);
}


void print_wlan_help(void *arg){
		printf("WLAN AT COMMAND SET:\n");
		printf("==============================\n");
        printf(" Set MP3 server\n");
        printf("\t# ATWS=URL,PATH,PORT\n");
        printf("\tSample:\tATWS=icecast.omroep.nl/3fm-sb-mp3,80\n");
        printf("\t\tATWS=meuk.spritesserver.nl/Ii.Romanzeandante.mp3,80\n");
        printf("\t\tATWS=?, ATWS=close, ATWS=save, ATWS=read\n");
        printf(" Connect to an AES AP\n");
        printf("\t# ATW0=SSID\n");
        printf("\t# ATW1=PASSPHRASE\n");
        printf("\t# ATWC\n");
        printf(" DisConnect AP\n");
        printf("\t# ATWD\n");
}

log_item_t at_user_items[ ] = {
	{"ATW0", fATW0,},
	{"ATW1", fATW1,},
	{"ATW2", fATW2,},
	{"ATWC", fATWC,},
	{"ATST", fATST,},
	{"ATDS", fATDS,},
	{"ATLW", fATLW,}, 	// Info Lwip
	{"ATSD", fATSD,},	// Dump register
	{"ATSW", fATSW,},	// Set register
	{"ATWD", fATWD,},	//
	{"ATWS", fATWS,},	// MP3 Set server, Close connect
	{"ATOF", fATOF,},	// Close connections
	{"ATON", fATON,},	// Open connections
};


void at_user_init(void)
{
	init_wifi_struct();
	connect_cfg_read();
	log_service_add_table(at_user_items, sizeof(at_user_items)/sizeof(at_user_items[0]));
}

log_module_init(at_user_init);

#endif //#ifdef CONFIG_AT_USR
