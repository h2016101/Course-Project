/*User Space code to generate System calls for VCNL 4010*/
#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>

void main() 
{
	
	int fileDesc;
	char *bus = "/dev/vcnl4010";
	if ((fileDesc = open(bus, O_RDWR)) < 0) 
	{
		//if no device connected 
		printf("Failed to open the bus. \n");
		
		exit(1);
	}

	//send the slave address using IOCTL call 
	ioctl(fileDesc, I2C_SLAVE, 0x13);

	
	char data[4] ={0};

	if(read(fileDesc, data, 4) != 4)
	{
		printf("Error while Reading the file \n");
	}
	else
	{
		
		
		int proximity = (data[2] * 256 + data[3]);

		
		
		printf("Proximity detect value is : %d \n", proximity);
	}

}

