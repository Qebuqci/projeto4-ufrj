// Bibliotecas
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
// Constantes //
// Configuração Wifi
#define WIFI_USUARIO "RicardoWifi"
#define WIFI_SENHA "2983084000"
// Configuração conexão MQTT
#define MQTT_HOST "XXX.messaging.internetofthings.ibmcloud.com"
#define MQTT_PORT 1883
#define MQTT_DEVICEID "d:XXX:YYY:ZZZ"
#define MQTT_USER "use-token-auth"
#define MQTT_TOKEN "PPP"
#define MQTT_TOPIC_MSG "iot-2/evt/status/fmt/json"
#define MQTT_TOPIC_INTERVAL "iot-2/cmd/interval/fmt/json"
// Configuração dos sensores
#define DHT_PIN D5
#define DHTTYPE DHT11

// Criação dos objetos //
// Objetos de conexão (Wifi, MQTT)
WifiClient wifiClient;
PubSubClient mqtt(MQTT_HOST, MQTT_PORT, eventoMQTT, wifiClient);
// Objetos sensores
DHT dht(DHT_PIN, DHTTYPE);
// Objetos json
// "Template" json para mensagens enviadas
StaticJsonDocument<100> jsonMsgEnv;
// "Template json para mensagens recebidas
StaticJsonDocument<100> jsonMsgRec;

JsonObject jsonConteudo = jsonMsgEnv.to<JsonObject>();
JsonObject jsonChave = jsonConteudo.createNestedObject("d");

// Variáveis globais //
int32_t IntervaloNotificacao = 10;

// Escopo Funções //
void inTermSerial;
void inWifi();
void inConMQTT();
void sincMQTT();
char *sensorizaAmbiente();
void enviaMQTT(char *topico, char *mensagem);
void eventoMQTT(char *topico, byte *conteudo, unsigned int tamanho);

// Inicia terminal (monitor) serial
void intTermSerial()
{
	Serial.begin(115200);
	Serial.setTimeout(2000);
    	while (!Serial) 
	{}
    	Serial.println();
    	Serial.println("Terminal serial ESP8266");
}
// Inicia conexão Wifi
void inWifi(char *user, char *pass)
{
	// Modo estação (cliente)
	WiFi.mode(WIFI_STA);
    	WiFi.begin(user, pass);
    	while (WiFi.status() != WL_CONNECTED)
    	{
	    delay(500);
	    Serial.print(".");
	}
	Serial.println("");
	Serial.println("Wifi conectado!");
}
// Inicia conexão MQTT
void iniciaConMQTT()
{
	while( !mqtt.connected() )
	{
		if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) 
		{
    			Serial.println("MQTT conectado");
		}
 		else
		{
      			Serial.println("MQTT falou ao se conectar! ... tentando novamente");
      			delay(500);
		}
    	}
}
// Sincroniza MQTT
void sincMQTT()
{
	mqtt.loop();
}
// Retorna uma string em json com os dados dos sensores
char *sensorizaAmbiente()
{
	float h, t, p = (0.0, 0.0, 0.0);
	static char msg[50];
	// Verifica falha na leitura dos sensores
	if (isnan(h) || isnan(t))
	{
    		Serial.println("Falhou ao ler o sensor!");
  	}
	// Caso não tenham 
	else
	{
		h = dht.readHumidity();
		t = dht.readTemperature();
		// FALTA * sensor de pressão
		jsonChave["temp"] = t;
		jsonChave["humidity"] = h;
		// FALTA * chave de pressao
		serializeJson(jsonConteudo, msg, 50);
		Serial.println(msg);
	}
	return msg;
}
void enviaMQTT(char *topico, char *mensagem)
{
	if ( !mqtt.publish(topico, mensagem) )
	{
		Serial.println("Publicação da mensagem falhou !");	
	}
}
// Lida com eventos assim que uma mensagem MQTT chega (num topico assinado)
void eventoMQTT(char *topico, byte *conteudo, unsigned int tamanho)
{
	Serial.print("Mensagem chegou: Tópico -> [");
	Serial.print(topico);
	Serial.print("] : ");
	
	conteudo[tamanho] = 0; 
	Serial.println( (char *) conteudo);
	DeserializationError err = deserializeJson(jsonMsgRec, (char *) conteudo);
	if (err)
	{
    		Serial.print(F("deserializeJson() falhou ")); 
    		Serial.println(err.c_str());
	}
	else
	{
		JsonObject cmdData = jsonMsgRec.as<JsonObject>();
		if (0 == strcmp(topico, MQTT_TOPIC_INTERVAL))
		{
      			IntervaloNotificacao = cmdData["Intervalo"].as<int32_t>();
      			Serial.print("Intervalo de notificação alterado para: ");
      			Serial.println(IntervaloNotificacao);
      			jsonMsgRec.clear();
    		}
		else
		{
			Serial.println("Comando desconhecido recebido");
    		}
  	}

}
void setup()
{
	inTermSerial();
	inWifi();
	inConMQTT();
	
	// Inicia sensores
	dht.begin();
}
void loop()
{
	// Sincroniza MQTT -> lida com msgs que devem ser enviadas e recebidas
	sincMQTT();
	// (Re)Conexão MQTT
	inConMQTT();
	// Assinando um topico para receber mensagens
	mqtt.subscribe();
	sincMQTT();
	enviaMQTT(MQTT_TOPIC_MSG, sensorizaAmbiente());
	
	Serial.print("Intervalo de Notificação:");
  	Serial.print(IntervaloNotificacao);
  	Serial.println();

  	// Sincronização com MQTT apos intervalo definido pela aplicação
  	for (int32_t i = 0; i < IntervaloNotificacao; i++)
	{
    		sincMQTT();
   		delay(1000);
  	}
}
