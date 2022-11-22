# ION-DTN-Uart-LoRa
This ION DTN repository inlcudes an Uart and LoRa Convergence Layer for ION-DTN

# How to confiugure UART Convergence Layer in ION

Using bpadmin command:
// Add an UART protocol to the ION
a protocol uart 1400 100
// Configure UART outduct with the link to the serial tty file descriptor and baud rate
a outduct uart /dev/ttyACM0,115200 uartclo
// Configure UART induct with the link to the serial tty file descriptor and baud rate
a induct uart /dev/ttyACM0,115200 uartcli

Using ipnadmin command (add plan):
a plan NODENR uart//dev/ttyACM0,115200
