-- 
-- -- Simple test code for softuart
-- -- Pipe data from uart1 to softuart and display it.
-- -- Connect a cable from gpio5 to gipio2/txd1 to receive the data via softuart.
--

softuart.setup(9600, 4, 5)
softuart.on("data", 1, function(data) print("lua handler called"); print(data) end)
uart.setup(1, 9600, 8, uart.PARITY_NONE, uart.STOPBITS_1)
uart.write(1, "1234567890\n01234567890")
softuart.write("test")
