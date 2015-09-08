#include "SimpleTimer.h"
#include <Servo.h>

SimpleTimer timer;
Servo lock;

byte timerDataPin = 5;    // green
byte timerLatchPin = 6;   // blue 
byte timerClockPin = 7;   // yellow

byte buttonPin = 2;
byte servoPin = 9;

byte timerReg1;
byte timerReg2;
byte timerReg3;
byte responseLeds = 0; // all off

const int INVALID = 9;
const int NUM_COLORS = 6;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            
const int CODE_SIZE = 4;

int numRightSlot;
int numRightColor;

volatile int guesses[CODE_SIZE];    // may be changed during interrupt
int code[CODE_SIZE] = {};   // generated "code" (4 random colors)

long debouncing_time = 50; // debounce time in ms
volatile unsigned long last_micros;
static unsigned long last_interrupt_time = 0;

const byte digits[10] = {
	0b00111111,        // 0            
	0b00000110,        // 1                 b  0 0 0 0 0 0 0 0
	0b01011011,        // 2                   DP G F E D C B A      
	0b01001111,        // 3
	0b01100110,        // 4                          A
	0b01101101,        // 5                         ---
	0b01111101,        // 6                     F / G / B
	0b00000111,        // 7                       ---
	0b01111111,        // 8                   E /   / C
	0b01100111         // 9                     ---  * DP
					   //                        D  
};

int mins = 3;      // starting time
int secTens = 0;
int secOnes = 0;
int count = 0;
						//  0      1       2        3         4       5
char* colorStrings[] = { "GREY", "RED", "WHITE", "PURPLE", "BLUE", "GREEN" };    // for serial printing
int colors[] = { 0, 1, 2, 3, 4, 5 };

boolean gameOver = false;
boolean locked = true;

int servoPosition = 0;

void setup() {

	lock.attach(servoPin);
	lock.write(0);

	Serial.begin(9600);
	//Serial.println("setUp");

	pinMode(timerDataPin, OUTPUT);
	pinMode(timerClockPin, OUTPUT);
	pinMode(timerLatchPin, OUTPUT);

	shiftOut(timerDataPin, timerClockPin, 0);
	shiftOut(timerDataPin, timerClockPin, 0);
	shiftOut(timerDataPin, timerClockPin, 0);
	shiftOut(timerDataPin, timerClockPin, 0);

	pinMode(A0, INPUT);
	pinMode(A1, INPUT);
	pinMode(A2, INPUT);
	pinMode(A3, INPUT);
	pinMode(A5, INPUT);

	digitalWrite(A0, HIGH);
	digitalWrite(A1, HIGH);
	digitalWrite(A2, HIGH);
	digitalWrite(A3, HIGH);

	pinMode(buttonPin, INPUT_PULLUP);
	attachInterrupt(0, debounceInterrupt, FALLING);

	generateCode();
	timer.setInterval(1000, tick);
}

void loop() {
	timer.run();
	showTime();
}

void tick() {
	secOnes--;
	if (secOnes < 0) {
		secOnes = 9;
		secTens--;
		if (secTens < 0) {
			secTens = 5;
			mins--;
			if (mins < 0) {
				mins = 0;
				secTens = 0;
				secOnes = 0;
				if (!gameOver) {
					lose();
				}
			}
		}
	}

}

void showTime() {
	for (int i = 0; i < 8; i++) {
		timerReg1 = digits[secOnes] & (B10000000 >> i);
		timerReg2 = digits[secTens] & (B10000000 >> i);
		if (mins <= 0) {
			timerReg3 = 0;
		}
		else {
			byte   dp = digits[mins] + 128;
			timerReg3 = dp & (0b10000000 >> i);
		}
		if (gameOver) {
			timerReg1 = 0;
			timerReg2 = 0;
			timerReg3 = 0;
		}
		digitalWrite(timerLatchPin, LOW);
		shiftOut(timerDataPin, timerClockPin, responseLeds);   // response lights
		shiftOut(timerDataPin, timerClockPin, timerReg1);      // seconds digit
		shiftOut(timerDataPin, timerClockPin, timerReg2);      // tens 
		shiftOut(timerDataPin, timerClockPin, timerReg3);      // mins
		digitalWrite(timerLatchPin, HIGH);
	}
}

void shiftOut(int dataPin, int clockPin, byte dataOut) {
	int i = 0;
	int pinState;
	for (i = 7; i >= 0; i--) {
		digitalWrite(clockPin, LOW);
		if (dataOut & (1 << i)) {
			pinState = 1;
		}
		else {
			pinState = 0;
		}
		digitalWrite(dataPin, pinState);   // write the bit
		digitalWrite(clockPin, HIGH);      // shift bits on rising clock pin 
		digitalWrite(dataPin, LOW);        // bring data pin low to prevent bleed through
	}
	//stop shifting
	digitalWrite(clockPin, LOW);
}

void generateCode() {
	/* the code is a permutation of 4 random colors out of 6 total 
	   it is generated by removing two random colors and 
	   shuffling the 4 colors left */
	
	randomSeed(analogRead(5));           // read from an analog port with nothing connected

	int a = random(0, NUM_COLORS);		 // random selections to discard
	int b = random(0, NUM_COLORS);
	
	while (a == b) {
		a = random(0, NUM_COLORS);
	}

	int numSet = 0;

	for (int i = 0; i < NUM_COLORS; i++) {
		if (i != a && i != b) {
			code[numSet] = colors[i];
			numSet++;
		}
	}	
	shuffleCode();
}

void shuffleCode() {
	for (int i = 0; i < 10; i++) {

		int a = random(0, CODE_SIZE);		 // random indexes to swap
		int b = random(0, CODE_SIZE);
		
		while (a == b) {
			a = random(0, CODE_SIZE);
		}

		int tmp = code[a];
		code[a] = code[b];
		code[b] = tmp;
	}
}

void getGuess() {
	int pinValue;
	int guess;
	for (int i = 0; i < CODE_SIZE; i++) {
		pinValue = analogRead(i);
		guess = map(pinValue, 0, 1023, 0, 6);
		guesses[i] = guess;
	}
}

void checkGuess() {
	numRightSlot = 0;
	numRightColor = 0;
	int codeCopy[CODE_SIZE];
	int guessesCopy[CODE_SIZE];

	// check for right color/slot and record matches (INVALID)
	for (int i = 0; i < CODE_SIZE; i++) {
		if (guesses[i] == code[i]) {
			numRightSlot += 1;
			codeCopy[i] = INVALID;
			guessesCopy[i] = INVALID;
		}
		else {
			codeCopy[i] = code[i];
			guessesCopy[i] = guesses[i];
		}
	}
	// check for right color and wrong slot, ignoring matches
	for (int i = 0; i < CODE_SIZE; i++) {
		if (guessesCopy[i] != INVALID) {
			for (int j = 0; j < CODE_SIZE; j++) {
				if (guessesCopy[i] == codeCopy[j]) {
					numRightColor += 1;
					guessesCopy[i] = INVALID;
					codeCopy[j] = INVALID;  // ignore next time
					break;
				}
			}
		}
	}
	setLights();
}

void setLights() {
	responseLeds = 0;
	if (numRightSlot > 0) {
		for (int i = 0; i < numRightSlot; i++) {
			bitSet(responseLeds, i);
		}
	}
	if (numRightColor > 0) {
		for (int i = 4; i < (numRightColor + 4); i++) {
			bitSet(responseLeds, i);
		}
	}
	if (numRightSlot == 4) {
		win();
	}
}

void win() {
	gameOver = true;
	locked = false;
	setLock();
}

void lose() {
	softReset();
}

void setLock() {
	if (locked) {
		lock.write(0);
	} else {
		lock.write(180);
	}
}

void debounceInterrupt() {
	if ((long)(micros() - last_micros) >= debouncing_time * 1000) {
		if (gameOver) {
			locked = true;
			setLock();
			//timer.setTimeout(5000, softReset);
		}
		getGuess();
		checkGuess();
		last_micros = micros();
	}
	count += 1;
}

void softReset() {
	asm volatile (" jmp 0");
}
