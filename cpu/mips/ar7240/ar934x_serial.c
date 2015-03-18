
#include <asm/u-boot.h> /* boot information for Linux kernel */
#include <asm/global_data.h>	/* global data used for startup functions */

#include <asm/addrspace.h>
#include <asm/types.h>
#include <config.h>
#include <hornet_soc.h>

DECLARE_GLOBAL_DATA_PTR;

#define uart_reg_read(x)        ar7240_reg_rd( (AR934X_UART1_BASE+x) )
#define uart_reg_write(x, y)    ar7240_reg_wr( (AR934X_UART1_BASE+x), y)

#define CHOWCHOW_GPIO_UART0_RX	13
#define CHOWCHOW_GPIO_UART0_TX	14
#define CHOWCHOW_GPIO_UART1_RX	9
#define CHOWCHOW_GPIO_UART1_TX	10

#define AR934X_GPIO_UART1_TD_OUT	79	/* table 2.16 */
#define AR934X_GPIO_UART0_SOUT	24	/* table 2.16 */
#define AR934X_GPIO_OUT_FUNC2_ENABLE_GPIO10	16
#define AR7240_GPIO_IN_ENABLE9_UART1_RD	16 
#define AR7240_GPIO_IN_ENABLE1_UART0_RD	8
#define AR934X_GPIO_OUT_FUNC3_ENABLE_GPIO14	16


#define		REG_OFFSET		4

/* === END OF CONFIG === */

/* register offset */
#define         OFS_RCV_BUFFER          (0*REG_OFFSET)
#define         OFS_TRANS_HOLD          (0*REG_OFFSET)
#define         OFS_SEND_BUFFER         (0*REG_OFFSET)
#define         OFS_INTR_ENABLE         (1*REG_OFFSET)
#define         OFS_INTR_ID             (2*REG_OFFSET)
#define         OFS_DATA_FORMAT         (3*REG_OFFSET)
#define         OFS_LINE_CONTROL        (3*REG_OFFSET)
#define         OFS_MODEM_CONTROL       (4*REG_OFFSET)
#define         OFS_RS232_OUTPUT        (4*REG_OFFSET)
#define         OFS_LINE_STATUS         (5*REG_OFFSET)
#define         OFS_MODEM_STATUS        (6*REG_OFFSET)
#define         OFS_RS232_INPUT         (6*REG_OFFSET)
#define         OFS_SCRATCH_PAD         (7*REG_OFFSET)

#define         OFS_DIVISOR_LSB         (0*REG_OFFSET)
#define         OFS_DIVISOR_MSB         (1*REG_OFFSET)

#define         MY_WRITE(y, z)  ((*((volatile u32*)(y))) = z)
#define         UART16550_READ(y)   ar7240_reg_rd((AR7240_UART_BASE+y))
#define         UART16550_WRITE(x, z)  ar7240_reg_wr((AR7240_UART_BASE+x), z)


void serial_setbrg (void);

int lowspeed_serial_init(void)
{
    u32 div,val;
    u32 ahb_freq, ddr_freq, cpu_freq;

    val = ar7240_reg_rd(WASP_BOOTSTRAP_REG);

    if ((val & WASP_REF_CLK_25) == 0) {
        div = (25 * 1000000) / (16 * CONFIG_BAUDRATE);
    } else {
        div = (40 * 1000000) / (16 * CONFIG_BAUDRATE);
    }
    /*
     * set DIAB bit
     */
    UART16550_WRITE(OFS_LINE_CONTROL, 0x80);

    /* set divisor */
    UART16550_WRITE(OFS_DIVISOR_LSB, (div & 0xff));
    UART16550_WRITE(OFS_DIVISOR_MSB, ((div >> 8) & 0xff));

    /* clear DIAB bit*/
    UART16550_WRITE(OFS_LINE_CONTROL, 0x00);

    /* set data format */
    UART16550_WRITE(OFS_DATA_FORMAT, 0x3);

    UART16550_WRITE(OFS_INTR_ENABLE, 0);

    return 0;
}

static int
AthrUartGet(char *__ch_data)
{
    u32    rdata;    
    
    rdata = uart_reg_read(UARTDATA_ADDRESS);

    if (UARTDATA_UARTRXCSR_GET(rdata)) {
        *__ch_data = (char)UARTDATA_UARTTXRXDATA_GET(rdata);
        rdata = UARTDATA_UARTRXCSR_SET(1);
        uart_reg_write(UARTDATA_ADDRESS, rdata); 
        return 1;
    }
    else {
        return 0;        
    }
}

static void
AthrUartPut(char __ch_data)
{
    u32 rdata;

    do {
        rdata = uart_reg_read(UARTDATA_ADDRESS);
    } while (UARTDATA_UARTTXCSR_GET(rdata) == 0);
    
    rdata = UARTDATA_UARTTXRXDATA_SET((u32)__ch_data);
    rdata |= UARTDATA_UARTTXCSR_SET(1);

    uart_reg_write(UARTDATA_ADDRESS, rdata);
}

void
ar7240_sys_frequency(u32 *cpu_freq, u32 *ddr_freq, u32 *ahb_freq)
{
    u32 pll, pll_div, ref_div, ahb_div, ddr_div, freq;

    pll = ar7240_reg_rd(AR7240_CPU_PLL_CONFIG);

    pll_div =
        ((pll & PLL_CONFIG_PLL_DIV_MASK) >> PLL_CONFIG_PLL_DIV_SHIFT);

    ref_div =
        ((pll & PLL_CONFIG_PLL_REF_DIV_MASK) >> PLL_CONFIG_PLL_REF_DIV_SHIFT);

    ddr_div =
        ((pll & PLL_CONFIG_DDR_DIV_MASK) >> PLL_CONFIG_DDR_DIV_SHIFT) + 1;

    ahb_div =
       (((pll & PLL_CONFIG_AHB_DIV_MASK) >> PLL_CONFIG_AHB_DIV_SHIFT) + 1)*2;

    freq = pll_div * ref_div * 5000000;

    if (cpu_freq)
        *cpu_freq = freq;

    if (ddr_freq)
        *ddr_freq = freq/ddr_div;

    if (ahb_freq)
        *ahb_freq = freq/ahb_div;
}

#define UART_BAUDRATE_COMP_MEGA32U4

int serial_init(void)
{
    u32 rdata;
    u32 fcEnable = 0; 
    u32 ahb_freq, ddr_freq, cpu_freq;
#ifndef UART_BAUDRATE_COMP_MEGA32U4    
    u32 baudRateDivisor, clock_step;
    u32 baud = CONFIG_BAUDRATE;
#endif

    ar7240_sys_frequency(&cpu_freq, &ddr_freq, &ahb_freq);    

    /* GPIO Configuration */
    rdata = ar7240_reg_rd(AR7240_GPIO_OE);
    rdata |= (1<<CHOWCHOW_GPIO_UART1_RX | 1<<CHOWCHOW_GPIO_UART0_RX);
    rdata &= ~(1<<CHOWCHOW_GPIO_UART1_TX | 1<<CHOWCHOW_GPIO_UART0_TX);
    ar7240_reg_wr(AR7240_GPIO_OE, rdata);
    
    /* GPIO Configuration for UART1 TX on gpio10*/

    rdata = ar7240_reg_rd(AR7240_GPIO_FUNC2);
    rdata &= ~(0xff << AR934X_GPIO_OUT_FUNC2_ENABLE_GPIO10);
    rdata |= AR934X_GPIO_UART1_TD_OUT << AR934X_GPIO_OUT_FUNC2_ENABLE_GPIO10; 
    ar7240_reg_wr(AR7240_GPIO_FUNC2, rdata);
    
    /* GPIO Configuration for UART1 RX on gpio9*/

    rdata = ar7240_reg_rd(AR7240_GPIO_IN_ENABLE9);
    rdata &=  ~(0xff << AR7240_GPIO_IN_ENABLE9_UART1_RD);
    rdata |= CHOWCHOW_GPIO_UART1_RX << AR7240_GPIO_IN_ENABLE9_UART1_RD; 
    ar7240_reg_wr(AR7240_GPIO_IN_ENABLE9, rdata);
    
    /* GPIO Configuration for UART0 TX on gpio14*/

    rdata = ar7240_reg_rd(AR7240_GPIO_FUNC3);
    rdata &= ~(0xff << AR934X_GPIO_OUT_FUNC3_ENABLE_GPIO14);
    rdata |= AR934X_GPIO_UART0_SOUT << AR934X_GPIO_OUT_FUNC3_ENABLE_GPIO14; 
    ar7240_reg_wr(AR7240_GPIO_FUNC3, rdata);
    
    /* GPIO Configuration for UART0 RX on gpio13*/

    rdata = ar7240_reg_rd(AR7240_GPIO_IN_ENABLE1);
    rdata &=  ~(0xff << AR7240_GPIO_IN_ENABLE1_UART0_RD);
    rdata |= CHOWCHOW_GPIO_UART0_RX << AR7240_GPIO_IN_ENABLE1_UART0_RD; 
    ar7240_reg_wr(AR7240_GPIO_IN_ENABLE1, rdata);

    /* Get reference clock rate, then set baud rate to 115200 */
    

#ifdef UART_BAUDRATE_COMP_MEGA32U4

    serial_setbrg();

#else
#ifndef CONFIG_HORNET_EMU

    rdata = ar7240_reg_rd(HORNET_BOOTSTRAP_STATUS);
    rdata &= HORNET_BOOTSTRAP_SEL_25M_40M_MASK;

    if (rdata)
        baudRateDivisor = ( 40000000 / (16*baud) ) - 1; // 40 MHz clock is taken as UART clock        
    else
        baudRateDivisor = ( 25000000 / (16*baud) ) - 1; // 25 MHz clock is taken as UART clock	        
#else
    baudRateDivisor = ( ahb_freq / (16*baud) ) - 1; // 40 MHz clock is taken as UART clock 
#endif
 
    clock_step = 8192;

    rdata = UARTCLOCK_UARTCLOCKSCALE_SET(baudRateDivisor) | UARTCLOCK_UARTCLOCKSTEP_SET(clock_step);
	uart_reg_write(UARTCLOCK_ADDRESS, rdata);    
#endif
    
    /* Config Uart Controller */
#if 1 /* No interrupt */
	rdata = UARTCS_UARTDMAEN_SET(0) | UARTCS_UARTHOSTINTEN_SET(0) | UARTCS_UARTHOSTINT_SET(0)
	        | UARTCS_UARTSERIATXREADY_SET(0) | UARTCS_UARTTXREADYORIDE_SET(~fcEnable) 
	        | UARTCS_UARTRXREADYORIDE_SET(~fcEnable) | UARTCS_UARTHOSTINTEN_SET(0);
#else    
	rdata = UARTCS_UARTDMAEN_SET(0) | UARTCS_UARTHOSTINTEN_SET(0) | UARTCS_UARTHOSTINT_SET(0)
	        | UARTCS_UARTSERIATXREADY_SET(0) | UARTCS_UARTTXREADYORIDE_SET(~fcEnable) 
	        | UARTCS_UARTRXREADYORIDE_SET(~fcEnable) | UARTCS_UARTHOSTINTEN_SET(1);
#endif	        	        
	        
    /* is_dte == 1 */
    rdata = rdata | UARTCS_UARTINTERFACEMODE_SET(2);   
    
	if (fcEnable) {
	   rdata = rdata | UARTCS_UARTFLOWCONTROLMODE_SET(2); 
	}
	
    /* invert_fc ==0 (Inverted Flow Control) */
    //rdata = rdata | UARTCS_UARTFLOWCONTROLMODE_SET(3);
    
    /* parityEnable == 0 */
    //rdata = rdata | UARTCS_UARTPARITYMODE_SET(2); -->Parity Odd  
    //rdata = rdata | UARTCS_UARTPARITYMODE_SET(3); -->Parity Even
    uart_reg_write(UARTCS_ADDRESS, rdata);
    
    lowspeed_serial_init();
    
    return 0;
}

int serial_tstc (void)
{
    return (UARTDATA_UARTRXCSR_GET(uart_reg_read(UARTDATA_ADDRESS)));
}

u8 serial_getc(void)
{
    char    ch_data;

    while (!AthrUartGet(&ch_data))  ;

    return (u8)ch_data;
}


void serial_putc(u8 byte)
{
    if (byte == '\n')   AthrUartPut('\r');

    AthrUartPut((char)byte);
}

void serial_setbrg (void)
{
	 u32 rdata;
	 u32 baudRateDivisor, clock_step;
	 u32 baud = gd->baudrate /*CONFIG_BAUDRATE*/;

	 //printf("%s: %d, %d", __FUNCTION__, baud, gd->baudrate);
#ifdef UART_BAUDRATE_COMP_MEGA32U4
        /* Fix timing issues on Atmega32U4 w/16Mhz oscillator */
        if (baud == 115200) {
                baudRateDivisor = 0x0003;
                clock_step  = 0x05e6;
        }
        if (baud == 250000 || baud == 230400) {
                baudRateDivisor = 0x0017;
                clock_step  = 0x4ccd;
        }
        if (baud == 500000) {
                baudRateDivisor = 0x000B;
                clock_step  = 0x4ccd;
        }
#endif
	rdata = UARTCLOCK_UARTCLOCKSCALE_SET(baudRateDivisor) | UARTCLOCK_UARTCLOCKSTEP_SET(clock_step);
	uart_reg_write(UARTCLOCK_ADDRESS, rdata);


}

void serial_puts (const char *s)
{
	while (*s)
	{
		serial_putc (*s++);
	}
}
