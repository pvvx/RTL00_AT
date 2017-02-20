#include "FreeRTOS.h"
#include "task.h"
#include "diag.h"
#include "main.h"
#include <example_entry.h>
#include "user/atcmd_user.h"

#include "rtl8195a.h"

#ifdef CONFIG_DEBUG_LOG
#define DEBUG_MAIN_LEVEL CONFIG_DEBUG_LOG
#else
#define DEBUG_MAIN_LEVEL 0
#endif

#ifdef CONFIG_WLAN_CONNECT_CB
/*_WEAK*/ void connect_start(void)
{
#if CONFIG_DEBUG_LOG
	rtl_printf("Time at start %d ms.\n", xTaskGetTickCount());
#endif
}

/*_WEAK*/ void connect_close(void)
{
}
#endif // CONFIG_WLAN_CONNECT_CB


/*-------------------------
 * void Init_Rand(void)
 * __low_level_init()
 *------------------------*/
void Init_Rand(void)
{
	extern u32 _rand_z1, _rand_z2, _rand_z3, _rand_z4, _rand_first;
	u32 *p = (u32 *)0x1FFFFF00;
	while(p < (u32 *)0x20000000) _rand_z1 ^= *p++;
	_rand_z1 ^= (*((u32 *)0x40002018) << 24) ^ (*((u32 *)0x40002118) << 16) ^ (*((u32 *)0x40002218) << 8) ^ *((u32 *)0x40002318);
	_rand_z2 = ((_rand_z1 & 0x007F00FF) << 7) ^	((_rand_z1 & 0x0F80FF00) >> 8);
	_rand_z3 = ((_rand_z2 & 0x007F00FF) << 7) ^	((_rand_z2 & 0x0F80FF00) >> 8);
	_rand_z4 = ((_rand_z3 & 0x007F00FF) << 7) ^	((_rand_z3 & 0x0F80FF00) >> 8);
	_rand_first = 1;
#if DEBUG_MAIN_LEVEL > 100
	DBG_8195A("Rand z: %p, %p, %p, %p.\n", _rand_z1, _rand_z2, _rand_z3, _rand_z4);
	DBG_8195A("Rand() %p, %p, %p, %p.\n", Rand(), Rand(), Rand(), Rand());
#endif
}
/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */
void main(void)
{
#if DEBUG_MAIN_LEVEL > 3
	 ConfigDebugErr  = -1;
	 ConfigDebugInfo = -1; //~_DBG_SPI_FLASH_;
	 ConfigDebugWarn = -1;
	 CfgSysDebugErr = -1;
	 CfgSysDebugInfo = -1;
	 CfgSysDebugWarn = -1;
#endif
/*
	 if ( rtl_cryptoEngine_init() != 0 ) {
	 DBG_8195A("crypto engine init failed\r\n");
	 }
	 */
#if 1 // def CONFIG_CPU_CLK
	if(HalGetCpuClk() != PLATFORM_CLOCK) {
		*((int *)0x40000074) &= ~(1<<17);
		HalCpuClkConfig(CPU_CLOCK_SEL_VALUE); // 0 - 166666666 Hz, 1 - 83333333 Hz, 2 - 41666666 Hz, 3 - 20833333 Hz, 4 - 10416666 Hz, 5 - 4000000 Hz
		HAL_LOG_UART_ADAPTER pUartAdapter;
		pUartAdapter.BaudRate = RUART_BAUD_RATE_38400;
		HalLogUartSetBaudRate(&pUartAdapter);
		SystemCoreClockUpdate();
		En32KCalibration();
	}
#else // 200 MHz
	HalCpuClkConfig(0);
	*((int *)0x40000074) |= (1<<17); // 6 - 200000000 Hz, 7 - 10000000 Hz, 8 - 50000000 Hz, 9 - 25000000 Hz, 10 - 12500000 Hz, 11 - 4000000 Hz
	HAL_LOG_UART_ADAPTER pUartAdapter;
	pUartAdapter.BaudRate = RUART_BAUD_RATE_38400;
	HalLogUartSetBaudRate(&pUartAdapter);
	SystemCoreClockUpdate();
	En32KCalibration();
#endif

#if DEBUG_MAIN_LEVEL > 1
	vPortFree(pvPortMalloc(4)); // Init RAM heap
	fATST(NULL); // RAM/TCM/Heaps info
#endif

	/* Initialize log uart and at command service */
	console_init();

	/* pre-processor of application example */
	pre_example_entry();

	/* wlan intialization */
#if defined(CONFIG_WIFI_NORMAL) && defined(CONFIG_NETWORK)
	wlan_network();
#endif

	/* Execute application example */
	example_entry();

	/*Enable Schedule, Start Kernel*/
#if defined(CONFIG_KERNEL) && !TASK_SCHEDULER_DISABLED
	#ifdef PLATFORM_FREERTOS
	vTaskStartScheduler();
	#endif
#else
	RtlConsolTaskRom(NULL);
#endif
}
