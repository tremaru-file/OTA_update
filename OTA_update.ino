#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <SPIFFS.h>
#include <LiquidCrystal_I2C.h> // 1.1.4
#include <Button2.h> // 2.2.4

// данные для подключения
String ssid = "";
String password = "";

// адрес сервера 
// const char* server = "адрес сервера для хранения файлов (слеш в конце строки обязателен)/";
// для проверки можно использовать наш репозиторий с уже загруженными прошивками:
const char* server = "https://tremaru-file.github.io/OTA_update/";

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

// название файла настроек
const char* settings_file = "/wifi.cfg";

// объект файла настроек
fs::File g_file;

// объект дисплея
LiquidCrystal_I2C disp(0x27, 20, 4);

// объекты кнопок
Button2 leftButton;
Button2 rightButton;

void setup()
{
	Serial.begin(115200);
	initDisplay();
	if (!SPIFFS.begin()) {
		disp.print("E 02");
		while(1);
	}
	loadWiFiCred();
	initWiFi();
	initFlashMemory();
	//saveWiFiCred();
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

// инициализация памяти
void initFlashMemory()
{
	if (!SPIFFS.begin()) {
		Serial.println("Форматируем память, нужно подождать...");
		disp.print("F   ");
		SPIFFS.format();
	}

	if (!SPIFFS.begin()) {
		Serial.println("Не получилось отформатировать, остановка.");
		disp.print("E 01");
		while(true)
			;
	}
}

// сохранение настроек WiFi (необходимо, если используется наш репозиторий)
bool saveWiFiCred()
{
	try {
		g_file = SPIFFS.open(settings_file, "w");

		if (!g_file) {
			Serial.println("Ошибка открытия файла");
			return false;
		}

		g_file.print(ssid);
		g_file.print('\n');
		g_file.print(password);
		g_file.print('\n');

		g_file.flush();
		g_file.close();

		Serial.println("Настройки сохранены");

		return true;
	}
	catch (...) {
		Serial.println("Исключение при сохранении файла");
		return false;
	}
}

void parseFile(fs::File&);

// обработка загрузки файла
void loadWiFiCred() try
{
	if (!SPIFFS.exists(settings_file)) {
		Serial.println("Нет файла настроек");
		return;
	}

	g_file = SPIFFS.open(settings_file, "r");

	if (!g_file) {
		Serial.println("Не получилось открыть файл");
		return;
	}

	parseFile(g_file);

	g_file.close();
}
catch (...)
{
	g_file.close();
	Serial.println("Проблемы парсинга файла");
}

// парсинг файла настроек WiFi
void parseFile(fs::File& file)
{
	static constexpr size_t bufsiz = 256;
	int b = 0;
	char buf[bufsiz];
	String loaded_setting = "";

	while (b != EOF) {
		static int i = 0;
		static bool on_password = false;
		b = file.read();
		if (b == '\n') {
			loaded_setting = String(buf);
			if (on_password)
				password = loaded_setting;
			else
				ssid = loaded_setting;

			Serial.println(loaded_setting);
			i = 0;
			memset(buf, '\0', bufsiz);
			continue;
		}
		buf[i++] = (byte)b;
	}
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
