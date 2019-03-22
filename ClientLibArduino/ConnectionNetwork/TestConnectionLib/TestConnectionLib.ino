#include <ConnectionNetwork.h>

ConnectionNetwork connectionNetwork;
bool connection;
bool calledOnDataMethod = false;
void onData(String data);
void onConnect();

void onData(String data){
  Serial.println("\nRecebeu");
  if(data==""){
    Serial.println("ops");
  }else{
    Serial.println(data);
  }
  
}

void onConnect(){
  Serial.println("Conectou");
  if(connectionNetwork.sendData("hello from ESP8266")){
    Serial.println("Enviado...");
  }else{
    Serial.println("Falha no envio");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("TESTE");
  // put your setup code here, to run once:
  connectionNetwork = ConnectionNetwork();
  connectionNetwork.networkConnect("RESIDENCIA_LUCAS","lucasfreitas115");
  connection = connectionNetwork.establishConnection("djxmmx.net", 17, onConnect);
  if(!connection){
    Serial.println("Falha na conexao");
  }
}

void loop() {
  if(!calledOnDataMethod){
    calledOnDataMethod = true;
    connectionNetwork.onDataCallback(onData);
  }
  Serial.println("Passou 1s");
  delay(1000);
}
