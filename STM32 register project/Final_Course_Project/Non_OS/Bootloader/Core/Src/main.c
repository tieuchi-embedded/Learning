/*
 * Bootloader: 0x08000000 128Kb
	-Reset nhảy vào app1
	-Nhấn giữ nút user, reset nhảy vào app2
 */

#include "main.h"

#define RCC_BASE_ADDR     0x40023800
#define GPIOA_BASE_ADDR   0x40020000

void Button_Init()
{
	uint32_t* RCC_AHB1ENR = (uint32_t*)(RCC_BASE_ADDR + 0x30);
    *RCC_AHB1ENR |= (1 << 0);			//Enable clock GPIOA

    uint32_t* GPIOA_MODER = (uint32_t*)(GPIOA_BASE_ADDR + 0x00);
    *GPIOA_MODER &= ~(0b11 << (0 * 2)); //PA0 input

    uint32_t* GPIOA_PUPDR = (uint32_t*)(GPIOA_BASE_ADDR + 0x0C);
    *GPIOA_PUPDR &= ~(0b11 << 0);
    *GPIOA_PUPDR |=  (0b10 << 0);		//Pull-down
}

int Button_state()
{
    uint32_t* IDR = (uint32_t*)(GPIOA_BASE_ADDR + 0x10);
    uint32_t state = *IDR & (1 << 0);
    return state;
}
int state;
int main()
{
    Button_Init();

	void (*function_ptr)();
	uint32_t* ptr=0;
	uint32_t* VTOR = (uint32_t*)0xE000ED08;

	if (Button_state()== 0)
	{
		ptr= (uint32_t*)0x08040004;
		*VTOR = 0x08040000;
	}
	else if(Button_state()== 1)
	{
		ptr= (uint32_t*)0x08060004;
		*VTOR = 0x08060000;
	}
	function_ptr = (void (*)(void))(*ptr);
	function_ptr();

    while(1)
    {
    	//state = Button_state();
    }
}
