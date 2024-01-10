#include <WiFi.h>
#include <LiquidCrystal_I2C.h> // 1.1.4
#include <Button2.h> // 2.2.4
#include "password.h"

// данные для подключения
//const char* ssid = "имя точки доступа";
//const char* password = "пароль точки доступа";

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
#define X(INDEX, STR, FILENAME) STR,
	MENU
#undef X
};

const char* filename_strings[] {
#define X(INDEX, STR, FILENAME) FILENAME,
	MENU
#undef X
}

LiquidCrystal_I2C disp(0x27, 20, 4);

// объекты кнопок
Button2 leftButton;
Button2 rightButton;

void setup()
{
	initWiFi();
	initDisplay();
	initButtons();
	initLED();
}

void loop()
{
	handleInput();
	handleDisplay();
	handleMenu();
	handleLED();
}

void handleDisplay()
{
}

void handleMenu()
{
}

void handleLED()
{
	static unsigned long blink_millis = 0;
	static constexpr unsigned long blink_interval = 1000;
	if (millis() - blink_millis < blink_interval)
		return;

	blink_millis = millis();
	static bool blink = false;
	digitalWrite(FW_LED, blink);
	blink = !blink;
}

void initWiFi()
{
	WiFi.begin(ssid, password);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		disp.print(".");
	}
}

void initLED()
{
	pinMode(FW_LED, OUTPUT);
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
		case BUTTON_LEFT: ++menu_state; break;
		case BUTTON_RIGHT: update(); break;
	}
}

void update()
{
	HTTPClinet client;
	client.begin(server + filename_strings[menu_state]);

	if (client.GET() == HTTP_CODE_OK) {
		g_full_length = client.getSize();
		int len = g_full_length;
		Update.begin(UPDATE_SIZE_UNKNOWN);
		uint8_t buff[128]{};
		WiFiClient* stream = client.getStreamPtr();

		while (client.connected() && (len > 0 || len == -1)) {
			size_t stream->available();
			if (size) {
				int bytes_read = stream->readBytes(buff, ((size > sizeof(buff))?sizeof(buff):size));
				updateFirmware(buff, bytes_read);
				if (len > 0)
					len -= bytes_read;
			}
			delay(1);
		}
	}
	else {
	// error	
	}
	client.end();
	ESP.restart();
}

void updateFirmware(uint8_t* data, size_t len)
{
	Update.write(data, len);
	g_curr_length += len;
	if (g_curr_length != g_full_length)
		return;

	Update.end(true);
}
