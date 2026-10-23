#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

#define DHT_PIN 15
#define LDR_PIN 34

const char* ssid = "Wokwi-GUEST";
const char* broker = "broker.hivemq.com";
const int porta = 1883;
const char* clientId = "agnello-esp32-rfeb";
const char* topicoDados = "fiap/agnello/rfeb/dados";
const char* topicoComando = "fiap/agnello/rfeb/comando";

const unsigned long intervaloEnvio = 5000;
unsigned long ultimoEnvio = 0;

DHT dht(DHT_PIN, DHT22);
WiFiClient espClient;
PubSubClient client(espClient);

// conecta no wifi do wokwi
void conectaWifi() {
  WiFi.begin(ssid, "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nwifi conectado");
}

// sincroniza a hora via ntp no fuso de brasilia
void sincronizaHora() {
  configTime(-3 * 3600, 0, "pool.ntp.org");
  struct tm info;
  while (!getLocalTime(&info)) {
    delay(500);
  }
  Serial.println("hora sincronizada");
}

// devolve a hora atual de brasilia em iso 8601
String pegaTimestamp() {
  struct tm info;
  getLocalTime(&info);
  char buf[30];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S-03:00", &info);
  return String(buf);
}

// recebe as mensagens publicadas pelo mymqtt no topico de comando
void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  Serial.print("recebido em ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(msg);
}

// reconecta no broker e assina o topico de comando
void reconecta() {
  while (!client.connected()) {
    Serial.print("conectando no broker...");
    if (client.connect(clientId)) {
      Serial.println("ok");
      client.subscribe(topicoComando);
    } else {
      Serial.print("falhou, rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  conectaWifi();
  sincronizaHora();
  client.setServer(broker, porta);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconecta();
  }
  client.loop();

  if (millis() - ultimoEnvio >= intervaloEnvio) {
    ultimoEnvio = millis();

    float temperatura = dht.readTemperature();
    float umidade = dht.readHumidity();
    int luminosidade = analogRead(LDR_PIN);

    if (isnan(temperatura) || isnan(umidade)) {
      Serial.println("falha na leitura do dht22");
      return;
    }

    char msg[200];
    snprintf(msg, sizeof(msg),
             "{\"temperatura\":%.1f,\"umidade\":%.1f,\"luminosidade\":%d,\"timestamp\":\"%s\"}",
             temperatura, umidade, luminosidade, pegaTimestamp().c_str());

    client.publish(topicoDados, msg);
    Serial.println(msg);
  }
}