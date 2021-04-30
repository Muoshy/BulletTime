![top](https://raw.githubusercontent.com/Muoshy/BulletTime/main/img/both_front.jpg)

# BulletTime
ESP32 based clock using LTP-305 led matrices.

	- NTP time retrieval
	- Brightness control
	- esp-idf
	- USB-C

  
## Schematic
WIP

## PCB
WIP

## Acrylic cover
WIP

## Software
Running esp-idf with esp32-wifi-manager 3.3.1 by tonyp7 for setting wifi credentials during runtime. 
Time set using sntp time example from esp-idf repository.
Brightness control using button interrupts.
 

## Flashing
	- Install esp-idf extension from Visual Studio Code marketplace
	- Open LTP_clock folder from VSCode
	- Select port, compile & flash.

  
## BOM

| Component                		| Quantity 	| Description 						|
|--------------------------		|----------	|------								|
| ESP32-WROOM-32D              	| 1x 		| ESP32     						|
| CP2102N-A02-GQFN24            | 1x       	| USB-UART     						|
| HT16K33			| 2x		| LED driver |
| LDL1117            			| 1x       	| Voltage regulator     			|
| SP0503BAHTG				   	| 1x       	| ESD-protection     				|
| LTP-305					   	| 6x       	| 5x7 led matrix     				|
| PCA9306					   	| 1x       	| I2C level shifter     			|
| 5.2x5.2mm SMD pushbutton 		| 4x       	| Pushbutton						|
| 14 Position DIP socket        | 6x       	|      								|
| Resistors (WIP)        		|        	|      								|
| Capacitors (WIP)        		|        	|      								|
