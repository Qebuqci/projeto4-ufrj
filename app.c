// Bibliotecas
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

// Constantes //

// Wifi
#define WIFI_USUARIO "RicardoWifi"
#define WIFI_SENHA "2983084000"

// MQTT
#define MQTT_HOST "XXX.messaging.internetofthings.ibmcloud.com"
#define MQTT_PORT 1883
#define MQTT_DEVICEID "d:XXX:YYY:ZZZ"
#define MQTT_USER "use-token-auth"
#define MQTT_TOKEN "PPP"
#define MQTT_TOPIC_MSG "iot-2/evt/status/fmt/json"
#define MQTT_TOPIC_INTERVAL "iot-2/cmd/interval/fmt/json"

// Sensores
// Temperatura e umidade
#define DHT_PIN D5
#define DHTTYPE DHT11

// Objetos MQTT
void evento(char *topico, byte *conteudo, unsigned int tamanho);
WiFiClient wifiClient;
PubSubClient mqtt(MQTT_HOST, MQTT_PORT, evento, wifiClient);

// Variáveis //

// Wifi
char user[] = WIFI_USUARIO;
char pass[] = WIFI_SENHA;

// Dados json enviados via MQTT
float h = 0.0;
float t = 0.0;
//float p = 0.0;
static char msg[50];
int32_t IntervaloNotificacao = 10;

// Objetos Sensores
DHT dht(DHT_PIN, DHTTYPE);

// Objetos json
StaticJsonDocument<100> jsonMsgEnviada;
StaticJsonDocument<100> jsonMsgRecebida;
JsonObject jConteudo = jsonMsgEnviada.to<JsonObject>();
JsonObject jStatus = jConteudo.createNestedObject("d");

void evento(char *topico, byte *conteudo, unsigned int tamanho)
{
// Lida com eventos assim que uma mensagem chega em determinado topico
	Serial.print("Mensagem chegou: Tópico -> [");
	Serial.print(topico);
	Serial.print("] : ");
  
	conteudo[tamanho] = 0; 
	Serial.println( (char *) conteudo);
	DeserializationError err = deserializeJson(jsonMsgRecebida, (char *) conteudo);
	if (err) 
	{
    	Serial.print(F("deserializeJson() falhou ")); 
    	Serial.println(err.c_str());
	} 
	else 
	{
    	JsonObject cmdData = jsonMsgRecebida.as<JsonObject>();
    	if (0 == strcmp(topico, MQTT_TOPIC_INTERVAL))
		{
      		IntervaloNotificacao = cmdData["Intervalo"].as<int32_t>();
      		Serial.print("Intervalo de notificação alterado para: ");
      		Serial.println(IntervaloNotificacao);
      		jsonMsgRecebida.clear();
    	}
		else
		{
			Serial.println("Comando desconhecido recebido");
    	}
  	}

}

void setup()
{
	// Iniciando terminal serial 
	Serial.begin(115200);
    Serial.setTimeout(2000);
    while (!Serial) 
	{}
    Serial.println();
    Serial.println("Terminal serial ESP8266");

    // Iniciando conexão Wifi
    WiFi.mode(WIFI_STA);
    WiFi.begin(user, pass);
    while (WiFi.status() != WL_CONNECTED)
    {
	    delay(500);
	    Serial.print(".");
	}
	Serial.println("");
	Serial.println("Wifi conectado!");

	// Iniciando conexao MQTT
	while(! mqtt.connected() )
	{
		if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) 
		{
    		Serial.println("MQTT conectado");
   			mqtt.subscribe(MQTT_TOPIC_INTERVAL);
		}
 		else
		{
      		Serial.println("MQTT falou ao se conectar! ... tentando novamente");
      		delay(500);
    	}

	}

	// Iniciando sensores
	dht.begin();
}
void loop()
{
	mqtt.loop(); // Sincroniza com MQTT Broker (Server) - re-conexão
	while ( !mqtt.connected() )
	{
    	Serial.print("Tentando conexão MQTT ...");
    	if ( mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN) )
		{
    		Serial.println("MQTT Conectado");
			// Assina topico para receber mensagens
    		mqtt.subscribe(MQTT_TOPIC_INTERVAL);
        	mqtt.loop();
    	} 
		else
		{
    		Serial.println("MQTT falou ao se conectar!");
    		delay(5000);
    	}
	}

	if (isnan(h) || isnan(t))
	{
    	Serial.println("Falhou ao ler o sensor!");
  	}
	else
	{
  		h = dht.readHumidity();
  		t = dht.readTemperature();
		// p;
		jStatus["temp"] = t;
    	jStatus["humidity"] = h;
    	serializeJson(jsonMsgEnviada, msg, 50);
    	Serial.println(msg);
		// Já publica verificando num topico
    	if ( !mqtt.publish(MQTT_TOPIC_MSG, msg) )
		{
	    	Serial.println("Publicação MQTT falhou");
	    }
	}
	
	Serial.print("Intervalo de Notificação:");
  	Serial.print(IntervaloNotificacao);
  	Serial.println();

  	// Sincronização com MQTT apos intervalo definido pela aplicação
  	for (int32_t i = 0; i < IntervaloNotificacao; i++)
	{
    	mqtt.loop();
   		delay(1000);
  	}
}
