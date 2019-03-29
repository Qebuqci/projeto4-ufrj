// Bibliotecas
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
// Constantes //
// Configuração Wifi
#define WIFI_USUARIO "RICARDO WIFI"
#define WIFI_SENHA "2983084000"
// Configuração conexão MQTT
#define MQTT_HOST "6zqn82.messaging.internetofthings.ibmcloud.com"
#define MQTT_PORT 1883
#define MQTT_DEVICEID "d:6zqn82:ESP8266:dev01"
#define MQTT_USER "use-token-auth"
#define MQTT_TOKEN "projeto4-123"
#define MQTT_TOPIC_MSG "iot-2/evt/status/fmt/json"
#define MQTT_TOPIC_INTERVAL "iot-2/cmd/interval/fmt/json"
// Configuração dos sensores //
// DHT11
#define DHT_PIN D6
#define DHTTYPE DHT11
// BMP280 - SPI
#define BMP_SCK D0
#define BMP_MISO D2
#define BMP_MOSI D1
#define BMP_CS D3

// Escopo Funções //
void inTermSerial();
void inWifi(char *user, char *pass);
void inConMQTT();
void sincMQTT();
char *sensorizaAmbiente();
void enviaMQTT(char *topico, char *mensagem);
void eventoMQTT(char *topico, byte *conteudo, unsigned int tamanho);

// Criação dos objetos //
// Objetos de conexão (Wifi, MQTT)
WiFiClient clienteWifi;
PubSubClient mqtt(MQTT_HOST, MQTT_PORT, eventoMQTT, clienteWifi);
// Objetos sensores
DHT dht(DHT_PIN, DHTTYPE);
Adafruit_BMP280 bmp(BMP_CS, BMP_MOSI, BMP_MISO,  BMP_SCK);

// Objetos json
// "Template" json para mensagens enviadas
StaticJsonDocument<100> jsonMsgEnv;
// "Template json para mensagens recebidas
StaticJsonDocument<100> jsonMsgRec;

JsonObject jsonConteudo = jsonMsgEnv.to<JsonObject>();
JsonObject jsonChave = jsonConteudo.createNestedObject("d");

// Variáveis globais //
int32_t IntervaloNotificacao = 10;

// Inicia terminal (monitor) serial
void inTermSerial()
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
void inConMQTT()
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
  float u, t, p, a = (0.0, 0.0, 0.0, 0.0);
  static char msg[200];
  // Verifica falha na leitura dos sensores
  if (isnan(u) || isnan(t))
  {
        Serial.println("Falhou ao ler o sensor!");
    }
  // Caso não tenham 
  else
  {
    u = dht.readHumidity();
    t = bmp.readTemperature();
    p = bmp.readPressure();
    a = bmp.readAltitude(); // Parâmetro ajustado para o local
    jsonChave["temp"] = t;
    //jsonChave["Umidade"] = u;
    jsonChave["Pressão"] = p;
    jsonChave["Altitude"] = a;
 
    serializeJson(jsonConteudo, msg, 200);
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
  inWifi(WIFI_USUARIO, WIFI_SENHA);
  inConMQTT();
  
  // Inicia sensores
  bmp.begin();
  dht.begin();
  
}
void loop()
{
  // Sincroniza MQTT -> lida com msgs que devem ser enviadas e recebidas
  sincMQTT();
  // (Re)Conexão MQTT
  inConMQTT();
  // Assinando um topico para receber mensagens
  mqtt.subscribe(MQTT_TOPIC_INTERVAL);
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
