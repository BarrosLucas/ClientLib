#include "ConnectionNetwork.h"


WiFiClient client;
Thread *thread;

//Utilizada no callback, para simular uma thread
long lastTimeOnData = -1; 


ConnectionNetwork::ConnectionNetwork(){
	
}


/*
PUBLIC 
CONECTA O ARDUINO � REDE WIFI DISPONIVEL
PARAMETROS:
const char* ssid       -> nome da rede
const char* password   -> senha da rede
*/
void ConnectionNetwork::networkConnect(const char* ssid, const char* password){
  WiFi.mode(WIFI_STA); 							//TIPO DE CONEX�O
  WiFi.begin(ssid, password); 					//INICIA A CONEX�O POR WIFI
  while (WiFi.status() != WL_CONNECTED) {		//EXECUTA AT� QUE A CONEXAO SEJA ESTABELECIDA
    delay(500);
  }
}


/*
PUBLIC
CONECTA O ARDUINO A UM HOST
PARAMETROS:
const char* host    -> IP onde se conecta
const int port      -> Porta de conex�o
void (*onConnect)   -> Callback que � executado ao ser estabelecida a conex�o
*/
bool ConnectionNetwork::establishConnection(const char* host, const int port, void (*onConnect)()){
  if (!client.connect(host, port)) {			//CASO N�O CONSIGA CONECTAR...
    Serial.println("connection failed");
    delay(5000);
    return false;
  }
  (*onConnect)();								//QUANDO CONECTAR...
  return true;
}


/*
PUBLIC
CHAMA UM M�TODO QUANDO RECEBER UM DADO
PARAMETRO:
void (*onData)    -> Fun��oo a ser executada
*/
void ConnectionNetwork::onDataCallback(void (*onData)(String data)){
  this->onData = onData;
  thread = new Thread();				//CRIA UMA THREAD
  thread->setInterval(1000);				//DEFINE QUE SER� EXECUTADA A CADA 100 MS
  thread->onRun(readingData); 			//QUANDO EXECUTAR, CHAMAR A FUN��O...
  readingDataReceived(this->onData);	//INICIA UMA THREAD
}

/*
PUBLIC
ENVIA OS DADOS PARA O HOST NO QUAL EST� CONECTADO
PARAMETRO:
const char[] data    -> Dado que ser� enviado
*/
bool ConnectionNetwork::sendData(char * data){
	if (client.connected()) {
    	client.println(data);
    	return true; 					//ENVIA E RETURNA UM TRUE INDICANDO QUE O ENVIO TEVE SUCESSO
  	}
  	return false;						//RETORNA FALSE INDICANDO QUE O ENVIO N�O TEVE SUCESSO
}


/*
PRIVATE
� CHAMADO PELO OnDataCallback PARA EXECUTAR A THREAD DE LEITURA
DA CONEX�O, VERIFICANDO SE POSSUI ALGUM DADO A SER LIDO...
�, BASICAMETNE, UMA THREAD QUE EXECUTA OUTRA THREAD
PARAMETRO:
void (*onData)()    -> Fun��o que � executada quando recebe algum dado
*/
void ConnectionNetwork::readingDataReceived(void (*onData)(String data)){
	String data = "";
	while(true){
		if(lastTimeOnData == -1){
			if (thread->shouldRun()){
				data = thread->run();
				if(data != ""){
					onData(data);	
				}
			}
			lastTimeOnData = millis();
			
		}else{
			if((millis() - lastTimeOnData)>=1000){
				if (thread->shouldRun()){
					data = thread->run();
					if(data != ""){
						onData(data);
					}
				}
				lastTimeOnData = millis();
			}
		}
		data = "";
	}
}

/*
PRIVATE
FAZ A LEITURA
N�O NECESSITA DE PARAMETRO PARA SER LIDO
*/
String ConnectionNetwork::readingData(){
  String data = "";
  while(client.available()){
	  char ch = static_cast<char>(client.read());
      data += ch;
  }
  return data;
}

ConnectionNetwork::~ConnectionNetwork() {
	// TODO Auto-generated destructor stub
	delete thread;
}
