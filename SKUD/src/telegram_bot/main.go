package telegram_bot

import (
	"fmt"
	"os"
	"sync"

	MQTT "github.com/eclipse/paho.mqtt.golang"
	"github.com/mymmrac/telego"
	th "github.com/mymmrac/telego/telegohandler"
	tu "github.com/mymmrac/telego/telegoutil"
)

// Конфигурация
type Config struct {
	MQTTBroker   string
	MQTTUsername string
	MQTTPassword string
	BotToken     string
}

// Информацию о кабинете
type Room struct {
	ID      int
	Name    string
	Command string
	Topic   string
	Message string
}

var (
	config = Config{
		MQTTBroker:   "tcp://localhost:1883",
		MQTTUsername: "skud_tg_bot",
		MQTTPassword: "1234",
		BotToken:     "8387914042:AAFksNAU3NEtwtNXVEgLHZs1MtcAkRrka0Q",
	}

	// Список доступных кабинетов
	rooms = []Room{
		{
			ID:      32,
			Name:    "Кабинет №32",
			Command: "open_lock32",
			Topic:   "new/button",
			Message: "32",
		},
		{
			ID:      33,
			Name:    "Кабинет №33",
			Command: "open_lock33",
			Topic:   "new/button",
			Message: "33",
		},
		{
			ID:      45,
			Name:    "Кабинет №45",
			Command: "open_lock45",
			Topic:   "new/button",
			Message: "45",
		},
	}

	mqttClient MQTT.Client
	clientOnce sync.Once
)

// getMQTTClient возвращает singleton клиента MQTT
func getMQTTClient() (MQTT.Client, error) {
	var err error
	clientOnce.Do(func() {
		opts := MQTT.NewClientOptions().AddBroker(config.MQTTBroker)
		opts.SetUsername(config.MQTTUsername)
		opts.SetPassword(config.MQTTPassword)
		opts.SetClientID("go_mqtt_telegram_bot")

		client := MQTT.NewClient(opts)
		if token := client.Connect(); token.Wait() && token.Error() != nil {
			err = token.Error()
			return
		}
		mqttClient = client
	})

	return mqttClient, err
}

// публикация сообщение в MQTT
func publishMQTTMessage(topic, message string) error {
	client, err := getMQTTClient()
	if err != nil {
		return fmt.Errorf("MQTT connection failed: %w", err)
	}

	token := client.Publish(topic, 0, false, message)
	token.Wait()
	if token.Error() != nil {
		return fmt.Errorf("MQTT publish failed: %w", token.Error())
	}

	return nil
}

func createKeyboard() *telego.ReplyKeyboardMarkup {
	// Количество кнопок в строке(может изменяться по мере добавления дверей)
	buttonsPerRow := 2

	// Создаем слайс для всех строк
	rows := make([][]telego.KeyboardButton, 0)

	// Создаем временную строку
	currentRow := make([]telego.KeyboardButton, 0, buttonsPerRow)

	// Добавляем кнопки для кабинетов
	for _, room := range rooms {
		// Здеся используем название кабинета вместо команды
		btn := tu.KeyboardButton(room.Name)
		currentRow = append(currentRow, btn)

		// Если строка заполнена, то делаем новую
		if len(currentRow) >= buttonsPerRow {
			rows = append(rows, currentRow)
			currentRow = make([]telego.KeyboardButton, 0, buttonsPerRow)
		}
	}

	// Добавляем оставшиеся кнопки, если они есть
	if len(currentRow) > 0 {
		rows = append(rows, currentRow)
	}

	// Добавляем кнопку помощи в отдельную строку
	rows = append(rows, tu.KeyboardRow(
		tu.KeyboardButton("О боте"),
	))

	return tu.Keyboard(rows...).
		WithResizeKeyboard(). // Автоматическое изменение размера
		WithOneTimeKeyboard() // Скрытие клавиатуры после использования
}

func main() {
	// Инициализация бота
	bot, err := telego.NewBot(config.BotToken, telego.WithDefaultDebugLogger())
	if err != nil {
		fmt.Printf("Failed to create bot: %v\n", err)
		os.Exit(1)
	}

	// Получение обновлений
	updates, err := bot.UpdatesViaLongPolling(nil)
	if err != nil {
		fmt.Printf("Failed to get updates: %v\n", err)
		os.Exit(1)
	}

	// Создание обработчика
	bh, err := th.NewBotHandler(bot, updates)
	if err != nil {
		fmt.Printf("Failed to create bot handler: %v\n", err)
		os.Exit(1)
	}

	defer func() {
		bh.Stop()
		bot.StopLongPolling()
		if mqttClient != nil && mqttClient.IsConnected() {
			mqttClient.Disconnect(250)
		}
	}()

	keyboard := createKeyboard()

	// Обработчик команды /start
	bh.Handle(func(bot *telego.Bot, update telego.Update) {
		chatID := tu.ID(update.Message.Chat.ID)
		message := tu.Message(
			chatID,
			"🚪 MQTT-панель для Системы контроля и управления доступом\n\n"+
				"Выберите кабинет, который хотите открыть:",
		).WithReplyMarkup(keyboard)
		_, _ = bot.SendMessage(message)
	}, th.CommandEqual("start"))

	// Обработчик текстовых сообщений (нажатие на кнопки с названиями кабинетов)
	bh.Handle(func(bot *telego.Bot, update telego.Update) {
		text := update.Message.Text
		chatID := tu.ID(update.Message.Chat.ID)

		// Ищем кабинет по названию
		for _, room := range rooms {
			if room.Name == text {
				// Отправляем сообщение об открытии
				_, _ = bot.SendMessage(tu.Message(
					chatID,
					fmt.Sprintf("🔓 %s открыт!", room.Name),
				).WithReplyMarkup(keyboard))

				// Публикуем MQTT сообщение
				if err := publishMQTTMessage(room.Topic, room.Message); err != nil {
					_, _ = bot.SendMessage(tu.Message(
						chatID,
						fmt.Sprintf("❌ Ошибка при открытии %s: %v", room.Name, err),
					))
					return
				}

				fmt.Printf("%s opened\n", room.Name)
				return
			}
		}

		// Обработка кнопки "Помощь"
		if text == "О боте" {
			_, _ = bot.SendMessage(tu.Message(
				chatID,
				"ℹ️ Этот бот создан для управления системой контроля доступа.\n\n"+
					"• Выберите нужный кабинет из списка\n"+
					"• Бот отправит команду на открытие двери\n\n"+
					"Создано на языке Go с использованием библиотек 'TeleGO' и 'Paho-MQTT'.",
			).WithReplyMarkup(keyboard))
		}
	}, th.AnyMessage())

	// Запуск обработчика
	bh.Start()
}
