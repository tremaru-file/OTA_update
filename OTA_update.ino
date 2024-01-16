#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <LiquidCrystal_I2C.h> // 1.1.4
#include <Button2.h> // 2.2.4

// данные для подключения
const char* ssid = "имя точки доступа";
const char* password = "пароль точки доступа";

// адрес сервера 
const char* server = "адрес сервера для хранения файлов (слеш в конце строки обязателен)/";
// для проверки можно использовать наш репозиторий с уже загруженными прошивками:
//const char* server = "https://tremaru-file.github.io/OTA_update/";

// выводы светодиодов (разкомментировать только один для каждой версии прошивки)
#define FW_LED LED_BUILTIN // BLUE
//#define FW_LED 18 // GREEN
//#define FW_LED 32 // RED

// макрос возможных прошивок (ИНДЕКС, СТРОКА ДЛЯ ЭКРАНА, НАЗВАНИЕ ФАЙЛА НА СЕРВЕРЕ)
#define MENU\
	X(FW1, "FW 1", "led_red.bin")\
	X(FW2, "FW 2", "led_green.bin")\
	X(FW3, "FW 3", "led_blue.bin")\

// вывод левой кнопки
constexpr unsigned BUTTON_LEFT = 13;
// вывод правой кнопки
constexpr unsigned BUTTON_RIGHT = 33;

// состояние меню (cм. макрос MENU)
typedef enum {
#define X(INDEX, STR, FILENAME) INDEX,
	MENU
#undef X
	NMENUSTATES
} state_t;

// переопределение оператора приращивания для смены текущего состояния меню
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

// строки для вывода на дисплей (см. макрос MENU)
const char* disp_strings[] {
#define X(INDEX, STR, FILENAME) STR,
	MENU
#undef X
};

// строки названия файлов (см. макрос MENU)
const char* filename_strings[] {
#define X(INDEX, STR, FILENAME) FILENAME,
	MENU
#undef X
};

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
	disp.setCursor(0,0);
	disp.print(disp_strings[menu_state]);
	disp.setCursor(0,1);
	disp.print(filename_strings[menu_state]);
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
	disp.clear();
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
		case BUTTON_RIGHT: ++menu_state; disp.clear(); break;
		case BUTTON_LEFT: update(); break;
	}
}

// обновление прошивки
void update()
{
	// запрос на сервер
	HTTPClient client;
	client.begin((String)server + filename_strings[menu_state]);

	// если запрос удачный - обновляем раздел новым кодом
	int httpcode = client.GET();
	if (httpcode == HTTP_CODE_OK) {
		disp.clear();
		disp.print("FW UPDATE..");
		updateFirmware(client);
	}
	else {
		disp.clear();
		disp.printf("HTTP: %d", httpcode);
		Serial.printf("HTTP: %d", httpcode);
		return;
	}

	client.end();

	// перезагрузка
	ESP.restart();
}

int g_full_length = 0;
int g_curr_length = 0;

// обновление раздела OTA
void updateFirmware(HTTPClient& client) {
	// хранение части полученных данных
	uint8_t buff[128]{};
	// получение информации о размере файла
	g_full_length = client.getSize();
	// размер для декрементации в цикле
	int len = g_full_length;
	// начинаем обновление
	Update.begin(UPDATE_SIZE_UNKNOWN);
	// получаем указатель на поток данных
	WiFiClient* stream = client.getStreamPtr();

	while (client.connected() && (len > 0 || len == -1)) {
		// пока данные есть - записываем поэтапно в Flash память
		size_t size = stream->available();
		if (size) {
			int bytes_read = stream->readBytes(buff, ((size > sizeof(buff))?sizeof(buff):size));
			updateFlash(buff, bytes_read);
			if (len > 0)
				len -= bytes_read;
		}
		delay(1);
	}
}


// запись из RAM в Flash (параметры: указатель на массив в RAM, длинна массива)
void updateFlash(uint8_t* data, size_t len)
{
	// Запись массива в Flash
	Update.write(data, len);
	// Обновление счётчика записсаного размера
	g_curr_length += len;

	// Вывод на дисплей точек на дисплей
	static int count = 0;
	count++;
	if (count % 1024 == 0)
		disp.print(".");

	// ранний выход из функции, если записаны не все данные
	if (g_curr_length != g_full_length)
		return;

	// завершение обновления
	Update.end(true);
}