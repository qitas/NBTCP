
#include <stdio.h>
#include "main.h"
#include "ov2640api.h"
#include "BC26/BC26.h"
#include "BASE64/cbase64.h"
#include "JSON/cjson.h"
//#include "ledctl.h"
#include "common.h"

static char MYMICCID[42] =  "99900000000000000000";
static char MYDEVICEID[32] = "460000000000000";
static char RSSI[] = "00000000000000000000000000000000";

static int make_json_data(char *oustr)
{
	
	char * p = 0;
	cJSON * pJsonRoot = NULL;
	char tmpstr[32];
	uint32_t RTC_buff = RTC_GetCounter();	

	pJsonRoot = cJSON_CreateObject();
	if(NULL == pJsonRoot){return -1;}
	
	cJSON_AddNumberToObject(pJsonRoot, "TIME SEED ", RTC_buff);
	cJSON_AddStringToObject(pJsonRoot, "DEVID",MYDEVICEID);
	cJSON_AddStringToObject(pJsonRoot, "MICCID", MYMICCID);
	cJSON_AddStringToObject(pJsonRoot, "RSSI ", RSSI);
	cJSON_AddStringToObject(pJsonRoot, "INFO","2600KPa");
	cJSON_AddStringToObject(pJsonRoot, "NET", "China Mobile");
	cJSON_AddStringToObject(pJsonRoot, "STATUS", "live");
	cJSON_AddStringToObject(pJsonRoot, "Auth ","Qitas");
	cJSON_AddStringToObject(pJsonRoot, "TIME","2018.11.14");
	
	p = cJSON_Print(pJsonRoot);
	
	if(NULL == p)
	{
		cJSON_Delete(pJsonRoot);
		return -1;
	}
	cJSON_Delete(pJsonRoot);
	
	sprintf(oustr,"%s",p);
	
	printf("JSON:%s\r\n",oustr);
	
	free(p);
	return 0;
}

static int make_send_data_str(char *outstr , unsigned char *data , int length)
{
	//AT+QSOSEND=0,5,3132333435\r\n
	
	char *tmp = malloc(1024);
	conv_hex_2_string((unsigned char*)data,length,tmp);
	sprintf(outstr,"AT+QSOSEND=0,%d,%s\r\n",length,tmp);
	free(tmp);
	//printf("SEND: %s \r\n",outstr);
	return 0;
}



/*******************************************************************************
* Function Name  : main
* Description    : Main program
* Input          : None
* Output         : None
* Return         : None
* Attention		   : None
*******************************************************************************/
int main(void)
{
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable,ENABLE);

	//init_led();
	SysTick_Config(SystemCoreClock / 100);
	
	init_uart1();
	init_uart2();
	
	
	RTC_Init();
	SET_BOOTLOADER_STATUS(2);
	WKUP_Pin_Init();
	
	init_utimer();
	init_task();
	init_mem();
	init_uart2_buffer();
	
	//LED_NETWORK_REGISTER_STATUS;
	
	modem_poweron();
	
	uint32_t startcnt = RTC_GetCounter();
	uint32_t DURcnt=0;	
	
	int tst;
	uint8_t search=0;
	while(neul_bc26_get_netstat()<0 && search< 50)
	{
		utimer_sleep(200);//等待连接上网络
		led0_off();
		utimer_sleep(200);//等待连接上网络
		led0_on();	
		search++;		
	}
	//while(neul_bc26_get_netstat()<0){};										//等待连接上网络
	{
		
		/*
		 * 分配内存
		 */
		#define RECV_BUF_LEN 1024
		char *recvbuf = malloc(RECV_BUF_LEN);
		char *atbuf = malloc(1024);
		char *jsonbuf = malloc(512);
		int ret=0,PTR=0;
		
		
		/*
		 * 发送AT指令
		 */
		memset(recvbuf,0x0,RECV_BUF_LEN);
		uart_data_write("AT+ZADC?\r\n", strlen("AT+ZADC?\r\n"), 0);
		uart_data_read(recvbuf, RECV_BUF_LEN, 0, 200);
//		{
//			char * __tmp = strstr(recvbuf,"OK");
//			if (__tmp > 0)
//			{
//				__tmp -= 19;
//				int i=0;
//				for(i=0;i<15;i++)
//				{
//					MYDEVICEID[i] = __tmp[i];
//				}
//				//MYDEVICEID[15] = 'f'; 
//				MYDEVICEID[16] = 0x0;
//				printf("ZADC : [%s\r\n",MYDEVICEID);
//			}
//		}		
		/*
		 * 打开PSM
		 */
		memset(recvbuf,0x0,RECV_BUF_LEN);
		uart_data_write("AT+CPSMS=1\r\n", strlen("AT+CPSMS=1\r\n"), 0);
		uart_data_read(recvbuf, RECV_BUF_LEN, 0, 200);
		
		/*
		 * 关闭回显,涉及数据提取
		 */
		memset(recvbuf,0x0,RECV_BUF_LEN);
		uart_data_write("ATE0\r\n", strlen("ATE0\r\n"), 0);
		uart_data_read(recvbuf, RECV_BUF_LEN, 0, 200);
		
		memset(recvbuf,0x0,RECV_BUF_LEN);
		uart_data_write("AT+CGPADDR=1\r\n", strlen("AT+CGPADDR=1\r\n"), 0);
		uart_data_read(recvbuf, RECV_BUF_LEN, 0, 200);
		/*
		 * 获取信号值
		 */
		memset(recvbuf,0x0,RECV_BUF_LEN);
		uart_data_write("AT+CSQ\r\n", strlen("AT+CSQ\r\n"), 0);
		ret = uart_data_read(recvbuf, RECV_BUF_LEN, 0, 200);
		if(strstr(recvbuf,"OK"))
		{	
			memcpy(RSSI,uart2_rx_buffer+8,ret-16);	
			PTR=ret-16;
			RSSI[PTR++] =':';
			//printf("CSQ RSSI: %s\r\n",RSSI);

		}
		//memcpy(RSSI,uart2_rx_buffer+8,4);
		//memset(RSSI+4,':',2);
		//printf("CSQ RSSI: %s\r\n",RSSI);
		
		memset(recvbuf,0x0,RECV_BUF_LEN);
		uart_data_write("AT+CESQ\r\n", strlen("AT+CESQ\r\n"), 0);
		ret = uart_data_read(recvbuf, RECV_BUF_LEN, 0, 200);
		if(strstr(recvbuf,"OK"))
		{	
			memcpy(RSSI+PTR,uart2_rx_buffer+9,ret-17);		
			PTR+=ret-17;
			memset(RSSI+PTR,'.',32-PTR);							
		}
		//memcpy(RSSI+6,uart2_rx_buffer+9,20);
		printf("RSSI: %s\r\n",RSSI);
		/*
		 * 获取设备ID
		 */
		memset(recvbuf,0x0,RECV_BUF_LEN);
		uart_data_write("AT+CIMI\r\n", strlen("AT+CIMI\r\n"), 0);
		uart_data_read(recvbuf, RECV_BUF_LEN, 0, 200);	
		{
			char * __tmp = strstr(recvbuf,"OK");
			if (__tmp > 0)
			{
				__tmp -= 19;
				int i=0;
				for(i=0;i<15;i++)
				{
					MYDEVICEID[i] = __tmp[i];
				}
				//MYDEVICEID[15] = 'f'; 
				MYDEVICEID[16] = 0x0;
				printf("IMSI : [%s\r\n",MYDEVICEID);
			}
		}
		/*
		 * 获取设备ID
		 */
//		memset(recvbuf,0x0,RECV_BUF_LEN);
//		uart_data_write("AT+CGSN=1\r\n", strlen("AT+CGSN=1\r\n"), 0);
//		uart_data_read(recvbuf, RECV_BUF_LEN, 0, 200);	
//		{
//			char * __tmp = strstr(recvbuf,"OK");
//			if (__tmp > 0)
//			{
//				__tmp -= 19;
//				int i=0;
//				for(i=0;i<15;i++)
//				{
//					MYDEVICEID[i] = __tmp[i];
//				}
//				//MYDEVICEID[15] = 'f'; 
//				MYDEVICEID[16] = 0x0;
//				printf("IMEI : [%s\r\n",MYDEVICEID);
//			}
//		}
				
		memset(recvbuf,0x0,RECV_BUF_LEN);
		uart_data_write("AT*MICCID\r\n", strlen("AT*MICCID\r\n"), 0);
		uart_data_read(recvbuf, RECV_BUF_LEN, 0, 200);
		{
			char * __tmp = strstr(recvbuf,"OK");
			if (__tmp > 0)
			{
				__tmp -= 24;
				int i=0;
				for(i=0;i<20;i++)
				{
					MYMICCID[i] = __tmp[i];
				}
				MYMICCID[20] = 0x0;
				printf("MYMICCID : [%s\r\n",MYMICCID);
			}
		}	
		
		/*
		 * 获取设备ID IMEI
		 */
		
		memset(recvbuf,0x0,RECV_BUF_LEN);
		uart_data_write("AT+CGSN=1\r\n", strlen("AT+CGSN=1\r\n"), 0);
		uart_data_read(recvbuf, RECV_BUF_LEN, 0, 200);		
		/*
		 * 创建Socket
		 */
		tst=0;
		do{
			memset(recvbuf,0x0,RECV_BUF_LEN);
			uart_data_write("AT+QSOC=1,1,1\r\n", strlen("AT+QSOC=1,1,1\r\n"), 0);
			ret = uart_data_read(recvbuf, RECV_BUF_LEN, 0, 200);
			tst++;
		}while(ret!=17 && tst < 10);
		
		do{
			memset(recvbuf,0x0,RECV_BUF_LEN);
			uart_data_write("AT+QSOCON=0,17799,\"120.79.63.76\"\r\n", strlen("AT+QSOCON=0,17799,\"120.79.63.76\"\r\n"), 0);
			ret = uart_data_read(recvbuf, RECV_BUF_LEN, 0, 200);	
			tst++;
		}while(ret!=0 && tst < 10);

//		do{	
//					
//			memset(recvbuf,0x0,RECV_BUF_LEN);
//			uart_data_write("AT+ESOSEND=0,5,3234363831\r\n", strlen("AT+ESOSEND=0,5,3234363831\r\n"), 0);
//			ret = uart_data_read(recvbuf, RECV_BUF_LEN, 0, 200);
//			tst++;	
//		}while(ret!=255&& tst> 10);

		
		make_json_data(jsonbuf);
		make_send_data_str(atbuf,(unsigned char*)jsonbuf,strlen(jsonbuf));
		memset(recvbuf,0x0,RECV_BUF_LEN);
		uart_data_write(atbuf,strlen(atbuf),0);
		uart_data_read(recvbuf, RECV_BUF_LEN, 0, 200);
		
		memset(recvbuf,0x0,RECV_BUF_LEN);
		uart_data_write("AT+QSODIS=0\r\n", strlen("AT+QSODIS=0\r\n"), 0);
		uart_data_read(recvbuf, RECV_BUF_LEN, 0, 200);
		
		memset(recvbuf,0x0,RECV_BUF_LEN);
		uart_data_write("AT+QSOCL=0\r\n", strlen("AT+QSOCL=0\r\n"), 0);
		uart_data_read(recvbuf, RECV_BUF_LEN, 0, 200);
		
		
		/*
		 * 发送ATI指令
		 */
//		memset(recvbuf,0x0,RECV_BUF_LEN);
//		uart_data_write("ATI\r\n", strlen("ATI\r\n"), 0);
//		uart_data_read(recvbuf, RECV_BUF_LEN, 0, 200);
		
		/*
		释放内存
		*/
		free(recvbuf);
		free(atbuf);
		free(jsonbuf);
	}
	
	printf("CurrentTim %d\r\n",RTC_GetCounter());
	
	/*
	 * 设置2 min之后再次启动并进入PSM模式
	 */
	//RTC_SetAlarm(RTC_GetCounter() + 120);
	DURcnt = RTC_GetCounter() - startcnt;
	RTC_SetAlarm(RTC_GetCounter()+ (58-DURcnt));
	//进入休眠
	utimer_sleep(10);
	Sys_Enter_Standby();

	return 0;
}





#ifdef __GNUC__
  /* With GCC/RAISONANCE, small printf (option LD Linker->Libraries->Small printf
     set to 'Yes') calls __io_putchar() */
  #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */


/**
  * @brief  Retargets the C library printf function to the USART.
  * @param  None
  * @retval None
  */
PUTCHAR_PROTOTYPE
{
	
  USART_SendData(USART1, (uint8_t) ch);
  /* Loop until the end of transmission */
  while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
  {}

  return ch;
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/*********************************************************************************************************
      END FILE
*********************************************************************************************************/

