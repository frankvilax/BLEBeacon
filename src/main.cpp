#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <WiFi.h>
#include <mbedtls/aes.h>
#include "base64.h"
#include <DHT.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <time.h>
#include <sys/time.h>
#include <EEPROM.h> // include library to read and write from flash memory

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" 
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" 

#define FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5        /* Time ESP32 will go to sleep (in seconds) */

#define DHTPIN 18 // pino de dados do DHT11
#define DHTTYPE DHT11 // define o tipo de sensor, no caso DHT11

#define EEPROM_SIZE 50 // define the number of bytes you want to access

RTC_DATA_ATTR int bootCount = 0;

mbedtls_aes_context aes;
 
DHT dht(DHTPIN, DHTTYPE); // Instanciates DHT sensor with the data pin and the model of the sensor

BLECharacteristic *pCharacteristic;

int humidity;
int temperature;

char * key = "abcdefghijklmnop";
char iv[16];
char plaintext[16];

const size_t payloadLength = strlen(plaintext);  
char cipheredText[16];

struct tm data;//Cria a estrutura que contem as informacoes da data.

int i = 0;

bool deviceConnected = false;

long receivedTimestamp;


class MyServerCallbacks: public BLEServerCallbacks {
  
  void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks 
{
      
      void onWrite(BLECharacteristic *pCharacteristic) 
      {
        std::string rxValue = pCharacteristic->getValue();
        
        if (rxValue.length() > 0) 
        {
          Serial.println("*********");
          Serial.print("Received Value: ");        
          for (int i = 0; i < rxValue.length(); i++) 
          {
            Serial.print(rxValue[i]);
          }
          Serial.println();
        //receivedTimestamp = atoi(rxValue);
        //Serial.println(receivedTimestamp);
        }
      }
};


// Encryption using AES ECB algorithm with a 128 bit key
void encrypt(char * plainText, char * key, unsigned char * outputBuffer)
{
  mbedtls_aes_context aes;
  mbedtls_aes_init( &aes );
  mbedtls_aes_setkey_enc( &aes, (const unsigned char*) key, strlen(key) * 8 );
  mbedtls_aes_crypt_ecb( &aes, MBEDTLS_AES_ENCRYPT, (const unsigned char*)plainText, outputBuffer);
  mbedtls_aes_free( &aes );

}
/*
void encrypt(char * plainText, char * key, char * iv, unsigned char * outputBuffer){
 
  mbedtls_aes_context aes;
 
  mbedtls_aes_init( &aes );
  mbedtls_aes_setkey_enc( &aes, (const unsigned char*) key, strlen(key) * 8 );
  mbedtls_aes_crypt_cbc( &aes, MBEDTLS_AES_ENCRYPT,sizeof(plainText),( unsigned char*) iv, (const unsigned char*)plainText, outputBuffer);
  mbedtls_aes_free( &aes );

}*/

void decrypt(unsigned char * chipherText, char * key, unsigned char * outputBuffer)
{
 
  mbedtls_aes_context aes;
  mbedtls_aes_init( &aes );
  mbedtls_aes_setkey_dec( &aes, (const unsigned char*) key, strlen(key) * 8 );
  mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, (const unsigned char*)chipherText, outputBuffer);
  mbedtls_aes_free( &aes );

}

void performMeasurements()
{

  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  if (isnan(temperature) || isnan(humidity)) 
  {
      Serial.println("Failed to read from DHT");
  }

}

void initializeServer()
{

  // Create the BLE Device
  BLEDevice::init("ESP32 DHT11"); // Give it a name
 
  // Configura o dispositivo como Servidor BLE
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
 
  // Cria o servico UART
  BLEService *pService = pServer->createService(SERVICE_UUID);
 
  // Cria uma Característica BLE para envio dos dados
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
                       
  pCharacteristic->addDescriptor(new BLE2902());
 
  // cria uma característica BLE para recebimento dos dados
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID_RX,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
 
  pCharacteristic->setCallbacks(new MyCallbacks());
 
  // Inicia o serviço
  pService->start();
 
  // Inicia a descoberta do ESP32
  pServer->getAdvertising()->start();
  Serial.println("Esperando um cliente se conectar...");
}

void printMeasurements()
{
    Serial.print("Temperature: ");
    Serial.println(temperature);
    Serial.print("Humidity: ");
    Serial.println(humidity);
    Serial.println("-------------");
    Serial.println("-------------");
    Serial.println("-------------");
    Serial.println("-------------");

}

void addPadding()
{

  sprintf(plaintext, "%d             ", temperature);

}

void optionalIV()
{

  strcpy(iv, "123456789101112");   // Initializing the IV for eventual future use

}

void printCipheredText()
{
  
  Serial.println("\nCiphered text:");
  
  for (int i = 0; i < 16; i++) 
  {

    char str[3];
    sprintf(str, "%02x", (int)cipheredText[i]);
    Serial.print(str);

  }
  
  Serial.print("\n");

}

void initializeUnixTimer()
{

  timeval tv;//Cria a estrutura temporaria para funcao abaixo.
	tv.tv_sec = 1559147775;//Atribui minha data atual. Voce pode usar o NTP para isso ou o site citado no artigo!
	settimeofday(&tv, NULL);//Configura o RTC para manter a data atribuida atualizada.

}

void inferCurrentDate()
{

  time_t tt = time(NULL);//Obtem o tempo atual em segundos. Utilize isso sempre que precisar obter o tempo atual
	data = *gmtime(&tt);//Converte o tempo atual e atribui na estrutura
 
	char data_formatada[64];
	strftime(data_formatada, 64, "%d/%m/%Y %H:%M:%S", &data);//Cria uma String formatada da estrutura "data"
 
	printf("\nUnix Time: %d\n", int32_t(tt));//Mostra na Serial o Unix time
	printf("Data formatada: %s\n", data_formatada);//Mostra na Serial a data formatada

}

void writeToMemory(int address, int toWrite)
{
  EEPROM.write(address, toWrite);
  EEPROM.commit();
}

int readFromMemory(int address)
{
  return EEPROM.read(address);
}

void testingFlashMemory()
{
  writeToMemory(i, temperature);
  temperature = readFromMemory(i);
}

void notifyClient()
{
    pCharacteristic->setValue(temperature);
    pCharacteristic->notify(); // Envia o valor para o aplicativo!
    Serial.print("*** Dado enviado: ");
    Serial.print(temperature);
    Serial.println(" ***");
}




void setup() 
{
  
  Serial.begin(115200);
  Serial.println("Just woke up.");
  
  dht.begin(); // Initialize sensor
  
  EEPROM.begin(EEPROM_SIZE); // initialize EEPROM with predefined size

  initializeServer();

  bootCount++;

  delay(1000); //Take some time to open up the Serial Monitor

  Serial.println("Starting BLE work!");

  initializeUnixTimer();

}

void loop() 
{
  if (deviceConnected) 
  {
    performMeasurements();
    printMeasurements();
    notifyClient();
  }
  
  delay(1000);

/*  
  Serial.print(++i);
  
  inferCurrentDate();

  performMeasurements();

 // testingFlashMemory();

  printMeasurements();
*/ 
/*
  
  addPadding();

  encrypt(plaintext, key, (unsigned char*) cipheredText);
  std::string s = "";
  
  printCipheredText();

  initializeServer(cipheredText);
  Serial.println("Characteristic defined! Now you can read it in your phone!");

  //esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * FACTOR);

  //esp_deep_sleep_start();*/

}
