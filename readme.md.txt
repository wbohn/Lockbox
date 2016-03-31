MasterMind Puzzle Box

This Arduino project is based off of the MasterMind boardgame.
https://en.wikipedia.org/wiki/Mastermind_(board_game)
 
It uses a voltage divider and different colored wires to simulate the
different colored pegs used in the original game. Instead of a two player
game, the player tries to solve a randomly generated code within a preset
time frame by plugging 4 (of 6 total) wires onto 4 Arduino pins. When the 
player presses a momentary button, the program reads the voltage at the 4 
chosen wires and illuminates LEDs to inform the player. A red LED indicates 
that a correct color wire was chosen (it does not indicate which) but that 
the wire is plugged into the wrong header. A green LED indicates that a 
correct color wire was chosen and connected to the correct pin. If the player
has solved the code, all 4 green LEDs will illuminate and the servo arm will
rotate to allow the box lid to be removed.


  
