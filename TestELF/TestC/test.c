#include <stdio.h>
#include <stdlib.h> 
#include <string.h>


extern void sleep(int ms);


int main()
{
	printf("Hello World!\n");
	*((unsigned int *)0xE0000000 ) = 0xF1;
	printf("TestFault\n");
	return 0;
}