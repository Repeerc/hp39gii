/*
 * A simple user interface for this project
 *
 * Copyright 2020 Creep_er
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */
#include "regsclkctrl.h"
#include "regspinctrl.h"
#include "regsicoll.h"
#include "regslcdif.h"
#include "regsapbh.h"
#include "display.h"
#include "irq.h"
#include "hw_irq.h"
#include "utils.h"
#include "FONT.H"

hw_lcdif_DmaDesc screen_buffer_dma_desc;

unsigned char *screen_buffer = (unsigned char *)VRAM_BASE; //显存

unsigned int LCD_is_busy()
{
	return BF_RD(LCDIF_CTRL, RUN);			//返回LCD控制器运行状态
}

//向LCD发送数据
void LCD_write_dat(unsigned int dat, unsigned int dat_size)
{
	while(LCD_is_busy());					//等待LCD忙状态
	BF_CS1 (LCDIF_CTRL, DATA_SELECT, 1);	//拉高数据信号线，设置为数据模式
	BF_CS1 (LCDIF_CTRL, READ_WRITEB, 0);	//设置LCD控制器为发送模式
	BF_CS1 (LCDIF_CTRL, COUNT, dat_size);	//设置要发送的字节数
	BF_CS1 (LCDIF_CTRL, RUN, 1);			//开始发送数据
	HW_LCDIF_DATA_WR(dat);					//向LCD控制器写入数据，并由控制器自动发送至屏幕
}

//向LCD发送命令
void LCD_write_cmd(unsigned int cmd, unsigned int cmd_size)
{

	while(LCD_is_busy());					//等待LCD忙状态
	BF_CS1 (LCDIF_CTRL, DATA_SELECT, 0);	//拉低数据信号线，设置为命令模式
	BF_CS1 (LCDIF_CTRL, READ_WRITEB, 0);	//设置LCD控制器为发送模式
	BF_CS1 (LCDIF_CTRL, COUNT, cmd_size);	//设置要发送的字节数
	BF_CS1 (LCDIF_CTRL, RUN, 1);			//开始发送数据
	HW_LCDIF_DATA_WR(cmd);					//向LCD控制器写入数据，并由控制器自动发送至屏幕
}

void LCD_read_dat(unsigned char *buffer, unsigned int size_bytes, unsigned char first_read_dummy)
{
	while(LCD_is_busy());											//等待LCD忙状态
	BF_CS1 (LCDIF_CTRL, DATA_SELECT, 1);							//拉高数据信号线，设置为数据模式
	BF_CS1 (LCDIF_CTRL, READ_WRITEB, 1);							//设置LCD控制器为接收模式
	BF_CS1 (LCDIF_CTRL, COUNT, size_bytes); 						//设置接收字节数
	BF_CS1 (LCDIF_CTRL1, READ_MODE_NUM_PACKED_SUBWORDS, 1);			//一次从屏幕读取一个字节
	BF_CS1 (LCDIF_CTRL1, FIRST_READ_DUMMY, first_read_dummy & 1);	//设置是否忽略读取的第一个字节
	BF_CS1 (LCDIF_CTRL, RUN, 1);									//控制器开始运行
	while(BF_RD(LCDIF_CTRL, RUN))
		{
			while(BF_RD(LCDIF_STAT, RXFIFO_EMPTY));
			*(buffer++) = BF_RD(LCDIF_DATA, DATA_ZERO);				//LCD控制器接收缓冲区非空，取出数据存入buffer
		}
}

void LCD_setxy(unsigned int x, unsigned int y)
{
//列地址
	LCD_write_cmd(0x2A, 1);
	LCD_write_dat(((x)&0xFF00)>>16, 1);
	switch(pix_format)
		{
		case PIX_FORMAT_GRAY4:
			LCD_write_dat((x/3)&0xFF, 1);
			break;
		case PIX_FORMAT_GRAY256:
			LCD_write_dat((x)&0xFF, 1);
			break;
		}

	LCD_write_dat(0x00, 1);
	LCD_write_dat(0x55, 1);

//行地址
	LCD_write_cmd(0x2B, 1);
	LCD_write_dat((y&0xFF00)>>16, 1);
	LCD_write_dat((y&0xFF)+8, 1);
	LCD_write_dat(0x00, 1);
	LCD_write_dat(0x87, 1);

//开始写LCD
	LCD_write_cmd(0x2C, 1);
}

//将显存的数据刷新到LCD
void LCD_flush_buffer(void)
{
	LCD_setxy(0, 0);
	switch(pix_format)
		{
		case PIX_FORMAT_GRAY4:
			for(int i=0; i<(255/3+1)*128; i+=4)LCD_write_dat(*((unsigned int *)(screen_buffer + i)), 4);
			break;
		case PIX_FORMAT_GRAY256:
			for(int i=0; i<258*128; i+=4)LCD_write_dat(*((unsigned int *)(screen_buffer + i)), 4);
			break;
		}
}

//清空显存
void LCD_clear_buffer(void)
{

	for(int i=0; i<256*128; i++)screen_buffer[i]=0;

}

//往显存写入一个像素点，颜色范围0~3
void LCD_write_pix(unsigned int x, unsigned int y, unsigned char color)
{
	unsigned char tmp;
	switch(pix_format)
		{
		case PIX_FORMAT_GRAY4:
			tmp = screen_buffer[ (x/3)+y*(255/3+1) ];
			switch(x%3)
				{
				case 2:
					tmp &= 0xFC;
					tmp |= ((color & 0x3));
					break;
				case 1:
					tmp &= 0xE7;
					tmp |= ((color & 0x3) << 3);
					break;
				case 0:
					tmp &= 0x3F;
					tmp |= ((color & 0x3) << 6);
					break;
				default:
					break;
				}
			screen_buffer[ (x/3)+y*(255/3+1) ] = tmp | 0x24;
			break;
		case PIX_FORMAT_GRAY256:
			screen_buffer[x+258*y]=color;
			break;
		}

}


//在指定位置显示一个字符
//x, y:起始坐标
//num:要显示的字符:" "--->"~"
//size:字体大小 12/16/24
//mode:叠加方式(1)还是非叠加方式(0)
void LCD_show_char(uint16_t x, uint16_t y, uint8_t num, uint8_t size, uint8_t color)
{  							  
    uint8_t temp, t1, t;
	uint16_t y0=y;
	uint8_t csize=(size/8+((size%8)?1:0))*(size/2);		//得到字体一个字符对应点阵集所占的字节数	
 	num=num-' ';//得到偏移后的值（ASCII字库是从空格开始取模，所以-' '就是对应字符的字库）
	for(t=0;t<csize;t++)
	{   
		if(size==12)	 temp=asc2_1206[num][t]; 	//调用1206字体
		else if(size==16)temp=asc2_1608[num][t];	//调用1608字体
		else if(size==24)temp=asc2_2412[num][t];	//调用2412字体
		else return;								//没有的字库
		for(t1=0;t1<8;t1++)
		{			    
			if(temp&0x80)LCD_write_pix(x, y, color);
			//else if(mode==0)LCD_point(x, y, 1);
			temp<<=1;
			y++;
			if(y>=128)return;		//超区域了
			if((y-y0)==size)
			{
				y=y0;
				x++;
				if(x>=256)return;	//超区域了
				break;
			}
		}  	 
	}  	    	   	 	  
}   

//显示字符串
//x, y:起点坐标
//width, height:区域大小  
//size:字体大小
//*p:字符串起始地址		  
void LCD_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, uint8_t color, uint8_t *p)
{         
	uint8_t x0=x;
	width+=x;
	height+=y;
    while((*p<='~')&&(*p>=' '))//判断是不是非法字符!
    {       
        if(x>=width){x=x0;y+=size;}
        if(y>=height)break;	//退出
        LCD_show_char(x, y, *p, size, color);
        x+=size/2;
        p++;
    }  
}

void LCD_dma_irq_handle(){
	BF_CS1(APBH_CTRL1, CH0_CMDCMPLT_IRQ, 0);		
	irq_set_enable(VECTOR_IRQ_LCDIF_DMA, 1);
	
	while(BF_RD(LCDIF_CTRL, RUN));
	BF_WRn(APBH_CHn_NXTCMDAR, 0, CMD_ADDR, (reg32_t)&screen_buffer_dma_desc);
	BW_APBH_CHn_SEMA_INCREMENT_SEMA(0, 1);
}

void LCD_dma_channel_reset(void)
{
	BF_CS2(APBH_CTRL0, SFTRST, 0, CLKGATE, 0);			//重置LCD DMA通道，使能通道时钟
	BF_CS1(APBH_CTRL0, RESET_CHANNEL, (BF_RD(APBH_CTRL0, RESET_CHANNEL)|0x01));
	while((BF_RD(APBH_CTRL0, RESET_CHANNEL)&0x01));		//等待DMA通道重置完成

	BF_CS1(APBH_CTRL1, CH0_CMDCMPLT_IRQ_EN, 1);											//打开LCD DMA控制器完成中断请求
	irq_set_enable(VECTOR_IRQ_LCDIF_DMA, 1);											//打开中断控制器中关于LCD DMA的中断请求
	irq_install_serveice(VECTOR_IRQ_LCDIF_DMA, (unsigned int *)LCD_dma_irq_handle);		//注册DMA操作完成中断的处理函数
	
	
	//设置LCD DMA通道命令描述符
	screen_buffer_dma_desc.p_Next = 0;
	screen_buffer_dma_desc.Semaphore = 1;
	screen_buffer_dma_desc.Command = LCDIF_DMA_READ;
	screen_buffer_dma_desc.Chain = 0;
	screen_buffer_dma_desc.IRQOnCompletion = 1;
	screen_buffer_dma_desc.WaitForEndCommand = 0;
	screen_buffer_dma_desc.p_DMABuffer = screen_buffer;
	screen_buffer_dma_desc.PIOWords = 1;

	screen_buffer_dma_desc.PioWord.B.WORD_LENGTH=1;
	screen_buffer_dma_desc.PioWord.B.DATA_SELECT=1;
	screen_buffer_dma_desc.PioWord.B.RUN=1;
	screen_buffer_dma_desc.PioWord.B.BYPASS_COUNT=0;
	screen_buffer_dma_desc.PioWord.B.READ_WRITEB=0;

}


void LCD_dma_flush_buffer()
{
	LCD_setxy(0, 0);

	switch(pix_format)
		{
		case PIX_FORMAT_GRAY4:
			screen_buffer_dma_desc.DMABytes = (255/3+1)*128;
			screen_buffer_dma_desc.PioWord.B.COUNT= (255/3+1)*128;
			break;
		case PIX_FORMAT_GRAY256:
			screen_buffer_dma_desc.DMABytes = 258*128;
			screen_buffer_dma_desc.PioWord.B.COUNT= 258*128;
			break;
		}


	while(LCD_is_busy());

	BF_WRn(APBH_CHn_NXTCMDAR, 0, CMD_ADDR, (reg32_t)&screen_buffer_dma_desc);
	BW_APBH_CHn_SEMA_INCREMENT_SEMA(0, 1);

}


void LCD_dma_flush_auto_buffer_start(){
	screen_buffer_dma_desc.IRQOnCompletion = 1;
	LCD_dma_flush_buffer();
}

void LCD_dma_flush_auto_buffer_stop(){
	screen_buffer_dma_desc.IRQOnCompletion = 0;
}

//设置对比度 范围 0-127
void LCD_set_contrast(unsigned char contrast)
{
	LCD_write_cmd(0x25, 1);
	LCD_write_dat(contrast & 0x7F, 1);
}

void LCD_set_pix_format(lcd_pix_format _pix_format)
{
	pix_format = _pix_format;
	LCD_write_cmd(0x3A, 1);
	LCD_write_dat(pix_format, 1);
}

void LCD_init(void)
{
	BF_CS8 (
		PINCTRL_MUXSEL2, 
	    BANK1_PIN07, 0, 
	    BANK1_PIN06, 0, 
	    BANK1_PIN05, 0, 
	    BANK1_PIN04, 0, 
	    BANK1_PIN03, 0, 
	    BANK1_PIN02, 0, 
	    BANK1_PIN01, 0, 
	    BANK1_PIN00, 0
	);
	//设置引脚复用寄存器，连接至SoC内部的LCD控制器
	BF_CS5 (
	    PINCTRL_MUXSEL3, 
	    BANK1_PIN20, 0, 
	    BANK1_PIN19, 0, 
	    BANK1_PIN18, 0, 
	    BANK1_PIN17, 0, 
	    BANK1_PIN16, 0
	);
	//设置引脚复用寄存器，连接至SoC内部的LCD控制器

	BF_CS2(CLKCTRL_PIX, CLKGATE, 0, DIV, 1);								//设置主时钟分频
	HW_LCDIF_CTRL_CLR(BM_LCDIF_CTRL_CLKGATE);						
	HW_LCDIF_CTRL_SET(BM_LCDIF_CTRL_SFTRST);							//重置LCD控制器
	delay_us(100);						
	HW_LCDIF_CTRL_CLR(BM_LCDIF_CTRL_SFTRST | BM_LCDIF_CTRL_CLKGATE);	//将LCD控制器唤醒

	HW_LCDIF_CTRL1_CLR(BM_LCDIF_CTRL1_MODE86);
	HW_LCDIF_CTRL1_CLR(BM_LCDIF_CTRL1_LCD_CS_CTRL);						//设置LCD控制器使用8080总线模式进行通信
				
	HW_LCDIF_CTRL1_SET(BM_LCDIF_CTRL1_RESET);							//拉高RESET线
				
	HW_LCDIF_VDCTRL0_SET(BM_LCDIF_VDCTRL0_VSYNC_OEB);					//VSYNC引脚作为输入（实际上不使用）
				
	BF_CS2 (LCDIF_CTRL, VSYNC_MODE, 0, WORD_LENGTH, 1);					//不使用VSYNC同步，设置总线带宽为8bit
	BF_CS1 (LCDIF_CTRL, BYPASS_COUNT, 0);

	BF_CS4 (LCDIF_TIMING, DATA_SETUP, 1, DATA_HOLD, 1, CMD_SETUP, 1, CMD_HOLD, 1);	//设置各个信号线的建立、保持时间

	delay_us(500);
	HW_LCDIF_CTRL1_CLR(BM_LCDIF_CTRL1_LCD_CS_CTRL);						//CS拉低，选中屏幕
	HW_LCDIF_CTRL1_CLR(BM_LCDIF_CTRL1_RESET);							//RESET拉低10ms，硬件复位
	delay_us(100000);
	HW_LCDIF_CTRL1_SET(BM_LCDIF_CTRL1_RESET);
	delay_us(100000);

	pix_format = PIX_FORMAT_GRAY256;	//默认使用256级灰度

	//硬件复位完成，开始发送LCD初始化序列
	LCD_write_cmd(0xD7, 1);	// Auto Load Set
	LCD_write_dat(0x9F, 1);
	LCD_write_cmd(0xE0, 1);	// EE Read/write mode
	LCD_write_dat(0x00, 1);	// Set read mode
	delay_us(100000);
	LCD_write_cmd(0xE3, 1);	// Read active
	delay_us(100000);
	LCD_write_cmd(0xE1, 1);	// Cancel control

	LCD_write_cmd(0x11, 1);	// sleep out

	delay_us(500000);

	LCD_write_cmd(0x28, 1);	// Display off
	LCD_write_cmd(0xC0, 1);	// Set Vop by initial Module
	LCD_write_dat(0x01, 1);
	LCD_write_dat(0x01, 1);	// base on Module

	LCD_write_cmd(0xF0, 1);	// Set Frame Rate
	LCD_write_dat(0x0D0D0D0D, 4);

	LCD_write_cmd(0xC3, 1);	// Bias select
	LCD_write_dat(0x02, 1);

	LCD_write_cmd(0xC4, 1);	// Setting Booster times
	LCD_write_dat(0x07, 1);

	LCD_write_cmd(0xD0, 1);	// Analog circuit setting
	LCD_write_dat(0x1D, 1);

	LCD_write_cmd(0xB5, 1);	// N-Line
	LCD_write_dat(0x8C, 1);
	LCD_write_dat(0x00, 1);
	LCD_write_cmd(0x38, 1);	// Idle mode off
	LCD_write_cmd(0x3A, 1);	// pix format
	LCD_write_dat(pix_format, 1);

	LCD_write_cmd(0x36, 1);	// Memory Access Control
	LCD_write_dat(0x48, 1);
	LCD_write_cmd(0xB0, 1);	// Set Duty
	LCD_write_dat(0x87, 1);
	LCD_write_cmd(0xB4, 1);
	LCD_write_dat(0xA0, 1);

	LCD_write_cmd(0x29, 1);	//Display on

	LCD_dma_channel_reset();
}



