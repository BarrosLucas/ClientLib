#include <ConnectionNetwork.h>

typedef unsigned char BYTE;

ConnectionNetwork connectionNetwork;
bool connection;
bool calledOnDataMethod = false;
void onData(String data);
void onConnect();
void string2ByteArray(char* input, BYTE* output);

void string2ByteArray(char* input, BYTE* output)
{
    int loop;
    int i;
    
    loop = 0;
    i = 0;
    
    while(input[loop] != '\0')
    {
        output[i++] = input[loop++];
    }
}

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
  int len = strlen("hello from ESP8266");
  BYTE arr[len];
  string2ByteArray("hello from ESP8266", arr);
  if(connectionNetwork.sendData(arr,len)){
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
}
