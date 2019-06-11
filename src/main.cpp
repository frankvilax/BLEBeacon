#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
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

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"    

#define FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5        /* Time ESP32 will go to sleep (in seconds) */

#define DHTPIN 18 // pino de dados do DHT11
#define DHTTYPE DHT11 // define o tipo de sensor, no caso DHT11

#define EEPROM_SIZE 50 // define the number of bytes you want to access

RTC_DATA_ATTR int bootCount = 0;

mbedtls_aes_context aes;
 
DHT dht(DHTPIN, DHTTYPE); // Instanciates DHT sensor with the data pin and the model of the sensor


int humidity;
int temperature;

char * key = "abcdefghijklmnop";
char iv[16];
char plaintext[16];

const size_t payloadLength = strlen(plaintext);  
char cipheredText[16];

struct tm data;//Cria a estrutura que contem as informacoes da data.

int i = 0;


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

void initializeServer(char *output)
{

  BLEDevice::init("ESP32");
  
  BLEServer *pServer = BLEDevice::createServer();
  
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setValue(output);

  pService->start();
  
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  
  pAdvertising->addServiceUUID(SERVICE_UUID);

  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  
  BLEDevice::startAdvertising();
}

void printMeasurements(){
    
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

void setup() 
{
  
  Serial.begin(115200);
  Serial.println("Just woke up.");
  
  dht.begin(); // Initialize sensor
  
  EEPROM.begin(EEPROM_SIZE); // initialize EEPROM with predefined size

  bootCount++;

  delay(1000); //Take some time to open up the Serial Monitor

  Serial.println("Starting BLE work!");

  initializeUnixTimer();

}

void loop() 
{
  
  delay(5000);
  
  Serial.print(++i);
  
  inferCurrentDate();

  performMeasurements();

 // testingFlashMemory();

  printMeasurements();

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
