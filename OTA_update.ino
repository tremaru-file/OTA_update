#include <LiquidCrystal_I2C.h> // 1.1.4
#include <Button2.h> // 2.2.4

#define FW_LED LED_BUILTIN
//#define FW_LED 18
//#define FW_LED 32

// макрос возможных прошивок (ИНДЕКС, СТРОКА ДЛЯ ЭКРАНА, НАЗВАНИЕ ФАЙЛА НА СЕРВЕРЕ)
#define MENU\
	X(FW1, "FW 1", "led_red.bin")\
	X(FW2, "FW 2", "led_green.bin")\
	X(FW3, "FW 3", "led_blue.bin")\

// вывод левой кнопки
constexpr unsigned BUTTON_LEFT = 13;
// вывод правой кнопки
constexpr unsigned BUTTON_RIGHT = 33;

// состояние меню
typedef enum {
#define X(INDEX, STR, FILENAME) INDEX,
	MENU
#undef X
	NMENUSTATES
} state_t;

state_t& operator++(state_t& s)
{
	switch (s) {
#define X(INDEX, STR, FILENAME) \
		case INDEX: s = static_cast<state_t>(INDEX + 1); s == NMENUSTATES ? s = FW1: 0; return s;
			    MENU
#undef X
	}
}

state_t menu_state = FW1;

// строки для вывода на дисплей
const char* disp_strings[] {
#define X(INDEX, STR) STR,
	MENU
#undef X
};

LiquidCrystal_I2C disp(0x27, 20, 4);

void setup()
{
	initDisplay();
	initButtons();
}

void loop()
{
	handleInput();
	handleDisplay();
	handleMenu();
	handleLED();
}

void handleLED()
{
	static unsigned long blink_millis = 0;
	static constexpr unsigned long blink_interval = 500;
	if (millis() - blink_millis < blink_interval)
		return;

	blink_millis = millis();
	static bool blink = false;
	digitalWrite(FW_LED, blink);
	blink = !blink;
}

// инициализация дисплея
void initDisplay()
{
	disp.init();
	disp.backlight();
	disp.noBlink();
	disp.print("CTAPTyEM....");
	delay(500);
}

// инициализация кнопок
void initButtons()
{
	leftButton.begin(BUTTON_LEFT, INPUT, false);
	rightButton.begin(BUTTON_RIGHT, INPUT, false);

	leftButton.setReleasedHandler(released);
	rightButton.setReleasedHandler(released);
}

// обработка ввода
void handleInput()
{
	leftButton.loop();
	rightButton.loop();
}

void released(Button2& btn)
{
	switch (btn.getPin()) {
		default: break;
		case BUTTON_LEFT: menu_state++; break;
		case BUTTON_RIGHT: update(); break;
	}
}

void update()
{
}
