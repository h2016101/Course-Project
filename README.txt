# Course-Project
> Download the Kernel Level code “i2c.c”, user space code “user.c” and Makefile .
> Go to the Directory where these files are downloaded.
> Run $ make all on terminal
> This should generate i2c.ko file successfully
> Compile the user code using command $ gcc user.c
> This should generate a.out executable file
> Insert the module using $ sudo insmod i2c.ko
> On successful insertion of the module run user code
> On terminal run $ ./a.out
> If everything goes right you should see the proximity value on the Terminal.
> To read the Status of Interrupt and other information run $dmesg
> If you bring something close to the sensor the Pin 11 on Raspberry Pi should go high, connect an Led to this Pin.
> To Remove the module run $rmmod i2c.ko  

