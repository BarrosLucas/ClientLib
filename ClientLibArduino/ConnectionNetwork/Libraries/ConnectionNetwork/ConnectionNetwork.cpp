#include "ConnectionNetwork.h"


WiFiClient client;
Thread *thread;

//Utilizada no callback, para simular uma thread
long lastTimeOnData = -1; 


ConnectionNetwork::ConnectionNetwork(){
	
}


/*
PUBLIC 
CONECTA O ARDUINO À REDE WIFI DISPONIVEL
PARAMETROS:
const char* ssid       -> nome da rede
const char* password   -> senha da rede
*/
void ConnectionNetwork::networkConnect(const char* ssid, const char* password){
  WiFi.mode(WIFI_STA); 							//TIPO DE CONEXÃO
  WiFi.begin(ssid, password); 					//INICIA A CONEXÃO POR WIFI
  while (WiFi.status() != WL_CONNECTED) {		//EXECUTA ATÉ QUE A CONEXAO SEJA ESTABELECIDA
    delay(500);
  }
}


/*
PUBLIC
CONECTA O ARDUINO A UM HOST
PARAMETROS:
const char* host    -> IP onde se conecta
const int port      -> Porta de conexão
void (*onConnect)   -> Callback que é executado ao ser estabelecida a conexão
*/
bool ConnectionNetwork::establishConnection(const char* host, const int port, void (*onConnect)()){
  if (!client.connect(host, port)) {			//CASO NÃO CONSIGA CONECTAR...
    Serial.println("connection failed");
    delay(5000);
    return false;
  }
  (*onConnect)();								//QUANDO CONECTAR...
  return true;
}


/*
PUBLIC
CHAMA UM MÉTODO QUANDO RECEBER UM DADO
PARAMETRO:
void (*onData)    -> Funçãoo a ser executada
*/
void ConnectionNetwork::onDataCallback(void (*onData)(String data)){
  this->onData = onData;
  thread = new Thread();				//CRIA UMA THREAD
  thread->setInterval(1000);				//DEFINE QUE SERÁ EXECUTADA A CADA 100 MS
  thread->onRun(readingData); 			//QUANDO EXECUTAR, CHAMAR A FUNÇÃO...
  readingDataReceived(this->onData);	//INICIA UMA THREAD
}

/*
PUBLIC
FECHA A CONEXÃO 
*/
void ConnectionNetwork::closeConnection(){
	client.stop();
}

/*
PUBLIC
ENVIA OS DADOS PARA O HOST NO QUAL ESTÁ CONECTADO
PARAMETRO:
const void *data    -> Recebe o array de bytes
int size			-> Recebe o tamanho
*/
bool ConnectionNetwork::sendData(const void *data, int size){
	const char *p = (const char*) data;
	if(client.connected()){
		while (size--) {
			client.write(p);
			*p++;
		}
		return true;
	}
	return false;
}


/*
PRIVATE
É CHAMADO PELO OnDataCallback PARA EXECUTAR A THREAD DE LEITURA
DA CONEXÃO, VERIFICANDO SE POSSUI ALGUM DADO A SER LIDO...
É, BASICAMETNE, UMA THREAD QUE EXECUTA OUTRA THREAD
PARAMETRO:
void (*onData)()    -> Função que é executada quando recebe algum dado
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
NÃO NECESSITA DE PARAMETRO PARA SER LIDO
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
