#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <protocol/protocol.pb.h>
#include <publish_message.pb.h>
#include <group_membership_message.pb.h>
#include <subscribe_message.pb.h>
#include <groupcast_message.pb.h>
#include <pb_encode.h>
#include <pb_decode.h>

#define PUBSUB_CONTROL_GROUP 100
#define PUBSUB_DATA_GROUP 101

#define MSG_ENCODING_BUFFER_SIZE 512
#define MSG_HEADER_BUFFER_SIZE 64
#define MAX_ENTRIES_STRSIZE 256

typedef int dyad_Stream;

typedef struct array {
    char strings[10][51];
    int q;
} tArrayStrings;
//TODO substituir esse esquema tambem

typedef struct tPayload {
    uint8_t *data;
    size_t maxSize;
    size_t size;
} tPayload;

UUID myUuid = UUID_init_default;
int runningAsSubscriber = 1;
const char * instanceTopic;

int java_hashCode(const char *str) {
    int hash = 0, i;

    for (i = 0; i < strlen(str); i++) {
        hash = 31 * hash + str[i];
    }

    return hash;
}

void appendDigits(int64_t val, uint8_t digits, char *str) {
    int64_t hi = (int64_t)1 << (digits * (uint8_t)4);
    char format[6];
    sprintf(format, "%%0%dlx", digits);
    char tempStr[13];
    sprintf(tempStr, format, hi | (val & (hi - 1)));

    strcat(str, tempStr + 1);
}

void uuidToString(UUID *uuid, char *str){
    *str = '\0';
    appendDigits(uuid->mostSignBits >> 32, 8, str);
    strcat(str, "-");
    appendDigits(uuid->mostSignBits >> 16, 4, str);
    strcat(str, "-");
    appendDigits(uuid->mostSignBits, 4, str);
    strcat(str, "-");
    appendDigits(uuid->leastSignBits >> 48, 4, str);
    strcat(str, "-");
    appendDigits(uuid->leastSignBits, 12, str);
}

int getNumberOfEntries(const tPayload *entriesPayload){
    int q = 0, i;

    for(i = 0; i < entriesPayload->size; i++){
        if (entriesPayload->data[i] == '\0'){
            q++;
        }
    }

    return q/2;
}

const char* getValueByKey(const tPayload *entriesPayload, const char *key){
    if (entriesPayload->size <= 0){
        return NULL;
    }

    const char *pos = entriesPayload->data;
    while ((uint8_t *)pos - entriesPayload->data < entriesPayload->size){
        if (pos[0] != 'k'){
            break;
        }
        pos++;
        if (strcmp(pos, key) == 0){
            //achou a key
            pos = strchr(pos, '\0'); //fim da key
            pos++;
            if (pos[0] != 'v'){
                break; //algum erro na string
            }
            pos++;
            return pos;
        }else{
            //nao achou a key
            pos = strchr(pos, '\0'); //fim da key
            pos++; //inicio do value
            if (pos[0] != 'v'){
                break; //algum erro na string
            }
            pos = strchr(pos, '\0'); //fim do value
            pos++;
        }
    }

    return NULL;
}

const char* getValueByIndex(const tPayload *entriesPayload, const int index){
    if (entriesPayload->size <= 0){
        return NULL;
    }

    const char *pos = entriesPayload->data;
    int i = -1;
    while ((uint8_t *)pos - entriesPayload->data < entriesPayload->size){
        if (pos[0] != 'k'){
            break;
        }
        i++;
        pos++;
        if (i == index){
            //achou a key
            pos = strchr(pos, '\0'); //fim da key
            pos++;
            if (pos[0] != 'v'){
                break; //algum erro na string
            }
            pos++;
            return pos;
        }else{
            //nao achou a key
            pos = strchr(pos, '\0'); //fim da key
            pos++; //inicio do value
            if (pos[0] != 'v'){
                break; //algum erro na string
            }
            pos = strchr(pos, '\0'); //fim do value
            pos++;
        }
    }

    return NULL;
}

const char* getKeyByIndex(const tPayload *entriesPayload, const int index){
    if (entriesPayload->size <= 0){
        return NULL;
    }

    const char *pos = entriesPayload->data;
    int i = -1;
    while ((uint8_t *)pos - entriesPayload->data < entriesPayload->size){
        if (pos[0] != 'k'){
            break;
        }
        i++;
        pos++;
        if (i == index){
            //achou a key
            return pos;
        }else{
            //nao achou a key
            pos = strchr(pos, '\0'); //fim da key
            pos++; //inicio do value
            if (pos[0] != 'v'){
                break; //algum erro na string
            }
            pos = strchr(pos, '\0'); //fim do value
            pos++;
        }
    }

    return NULL;
}

bool encodeGroup(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    Group *gr = *arg;

    if (!pb_encode_tag_for_field(stream, field)){
        return false;
    }

    if (pb_encode_submessage(stream, Group_fields, gr)){
        return true;
    }else{
        return false;
    }
}

bool decodeGroup(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    Group *gr = *arg;

    return pb_decode(stream, Group_fields, gr);
}

bool encodeGroupMembership(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    uint8_t gmBuffer[MSG_ENCODING_BUFFER_SIZE];
    pb_ostream_t gmStream = pb_ostream_from_buffer(gmBuffer, MSG_ENCODING_BUFFER_SIZE);
    gmStream.max_size = MSG_ENCODING_BUFFER_SIZE;

    GroupMembership *gm = *arg;

    if (!pb_encode(&gmStream, GroupMembership_fields, gm)){
        return false;
    }

    if (!pb_encode_tag_for_field(stream, field)){
        return false;
    }

    return pb_encode_string(stream, gmBuffer, gmStream.bytes_written);
}

bool decodeGroupMembership(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    GroupMembership *gm = *arg;

    return pb_decode(stream, GroupMembership_fields, gm);
}

bool encodeGroupcast(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    uint8_t groupcastBuf[MSG_ENCODING_BUFFER_SIZE];
    pb_ostream_t groupcastStm = pb_ostream_from_buffer(groupcastBuf, MSG_ENCODING_BUFFER_SIZE);
    groupcastStm.max_size = MSG_ENCODING_BUFFER_SIZE;

    Groupcast *groupcastMsg = *arg;

    if (!pb_encode(&groupcastStm, GroupMembership_fields, groupcastMsg)){
        return false;
    }

    if (!pb_encode_tag_for_field(stream, field)){
        return false;
    }

    return pb_encode_string(stream, groupcastBuf, groupcastStm.bytes_written);
}

bool encodeSubscriptionInformation(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    uint8_t subBuf[MSG_ENCODING_BUFFER_SIZE];
    pb_ostream_t siStm = pb_ostream_from_buffer(subBuf, MSG_ENCODING_BUFFER_SIZE);
    siStm.max_size = MSG_ENCODING_BUFFER_SIZE;

    SubscribeInformation *si = *arg;

    if (!pb_encode(&siStm, SubscribeInformation_fields, si)){
        return false;
    }

    if (!pb_encode_tag_for_field(stream, field)){
        return false;
    }

    return pb_encode_string(stream, subBuf, siStm.bytes_written);
}

bool encodeClientLibMessage(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    uint8_t internalBuffer[MSG_ENCODING_BUFFER_SIZE];
    pb_ostream_t internalOutStream = pb_ostream_from_buffer(internalBuffer, MSG_ENCODING_BUFFER_SIZE);
    internalOutStream.max_size = MSG_ENCODING_BUFFER_SIZE;

    ClientLibMessage *msg = *arg;

    if (!pb_encode(&internalOutStream, ClientLibMessage_fields, msg)){
        return false;
    }

    if (!pb_encode_tag_for_field(stream, field)){
        return false;
    }

    return pb_encode_string(stream, internalBuffer, internalOutStream.bytes_written);
}

bool encodeTagList(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    //array de strings
    tArrayStrings *as = *arg;
    int i;

    for(i = 0; i < as->q; i++){
        if (!pb_encode_tag_for_field(stream, field)){
            return false;
        }
        if (!pb_encode_string(stream, (uint8_t*)as->strings[i], strlen(as->strings[i]))){
            return false;
        }else{
        }
    }

    return true;
}

bool encodeSingleString(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    //array de strings
    char *str = *arg;

    if (!pb_encode_tag_for_field(stream, field)){
        return false;
    }
    return pb_encode_string(stream, str, strlen(str));
}

bool decodeSingleString(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    tPayload *payload = *arg;
    size_t left = stream->bytes_left;

    if (stream->bytes_left > payload->maxSize - 1)
        return false;

    if (!pb_read(stream, payload->data, stream->bytes_left))
        return false;

    payload->data[left] = '\0';
    payload->size = left + 1;

    return true;
}

bool decodeAndAppendMapEntriesSingleString(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    tPayload *payload = *arg;
    size_t left = stream->bytes_left;

    if (left > (payload->maxSize - payload->size) - 3)
        return false;

    char *endStr = &payload->data[payload->size];

    if (payload->size != 0){ //se ja tem algo na string
        switch(field->tag){
            case MapEntry_key_tag:
                *endStr = '|';
                break;
            case MapEntry_value_tag:
                *endStr = '=';
                break;
        }
        endStr++;
        payload->size++;
    }

    if (!pb_read(stream, endStr, left))
        return false;

    endStr[left] = '\0';
    payload->size += left;

    return true;
}

bool decodeAndAppendMapEntriesString(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    tPayload *payload = *arg;
    size_t left = stream->bytes_left;

    if (left > (payload->maxSize - payload->size) - 3)
        return false;

    char *endStr = &payload->data[payload->size];
    if (payload->size > 0){
        endStr++;
        payload->size++;
    }
    //adicionar cabecalho do campo
    switch(field->tag){
        case MapEntry_key_tag:
            *endStr = 'k';
            break;
        case MapEntry_value_tag:
            *endStr = 'v';
            break;
    }
    endStr++;
    payload->size++;

    if (!pb_read(stream, endStr, left))
        return false;

    endStr[left] = '\0';
    payload->size += left;

    return true;
}

bool appendToMapEntryString(tPayload *entriesPayload, const char *key, const char *value){
    if(entriesPayload->maxSize < entriesPayload->size + 1 + strlen(key) + 1 + 1 + strlen(value) + 1){
        //nao ha espaco
        return false;
    }

    char *endStr = &entriesPayload->data[entriesPayload->size];
    if (entriesPayload->size > 0){
        endStr++;
        entriesPayload->size++;
    }

    //adiciona chave
    *endStr = 'k';
    endStr++;
    entriesPayload->size++;
    strcpy(endStr, key);
    endStr += strlen(key) + 1;
    entriesPayload->size += strlen(key) + 1;
    //adiciona valor
    *endStr = 'v';
    endStr++;
    entriesPayload->size++;
    strcpy(endStr, value);
    endStr += strlen(value);
    entriesPayload->size += strlen(value);

    return true;

}

bool decodeMapEntries(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    MapEntry *entry = *arg;
    return pb_decode(stream, MapEntry_fields, entry);
}

bool encodeMapEntries(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    tPayload *dataPayload = *arg;
    const char *str = dataPayload->data, *k, *v;
    //return true caso tamanho zero?

    while((uint8_t*)str - dataPayload->data < dataPayload->size){
        if ( *(k = str) != 'k' ){ //verifica se esta no iniciio de uma chave
            puts("Erro ao montar map, chave esperada");
            return false;
        }else{
            if ( *(v = k + strlen(k) + 1) != 'v' ){ //verifica se tem um valor apos chave
                puts("Erro ao montar map, valor esperado");
                return false;
            }else{
                k++;
                v++;
            }
        }

        //preencher MapEntry
        MapEntry mapEntry = MapEntry_init_default;
        mapEntry.key.funcs.encode = &encodeSingleString;
        mapEntry.key.arg = k;
        mapEntry.value.funcs.encode = &encodeSingleString;
        mapEntry.value.arg = v;
        //printf("montada MapEntry: %s=%s\n", k, v);

        //inserir na stream
        if (!pb_encode_tag_for_field(stream, field)){
            puts("Erro ao codificar tag de MapEntry");
            return false;
        }
        if (!pb_encode_submessage(stream, MapEntry_fields, &mapEntry)){
            puts("Erro codificando MapEntry");
            return false;
        }

        //atualizar o str para apontar para proximos valores
        str = v + strlen(v) + 1;
    }

    return true;
}

bool decodePayloadBytes(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    tPayload *payload = *arg;
    size_t left = stream->bytes_left;

    if (stream->bytes_left > payload->maxSize)
        return false;

    if (!pb_read(stream, payload->data, stream->bytes_left))
        return false;

    payload->size = left;

    return true;
}

bool encodePublishInformation(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    uint8_t gmBuffer[MSG_ENCODING_BUFFER_SIZE];
    pb_ostream_t gmStream = pb_ostream_from_buffer(gmBuffer, MSG_ENCODING_BUFFER_SIZE);
    gmStream.max_size = MSG_ENCODING_BUFFER_SIZE;

    PublishInformation *pse = *arg;

    if (!pb_encode(&gmStream, PublishInformation_fields, pse)){
        return false;
    }

    if (!pb_encode_tag_for_field(stream, field)){
        return false;
    }

    return pb_encode_string(stream, gmBuffer, gmStream.bytes_written);
}

static void decodePubSubEvent(char *data, int size){
    PublishInformation pubmsg = PublishInformation_init_zero;

    //string informationClass
    char informationClass[51];
    tPayload strPayload = {informationClass, 51, 0};
    pubmsg.informationClass.funcs.decode = &decodeSingleString;
    pubmsg.informationClass.arg = &strPayload;

    //informacoes
    MapEntry informationMapEntries = MapEntry_init_zero;
    char allInformation[size+size/2];
    tPayload allInformationPayload = {allInformation, size, 0};
    informationMapEntries.key.funcs.decode = &decodeAndAppendMapEntriesString;
    informationMapEntries.key.arg = &allInformationPayload;
    informationMapEntries.value.funcs.decode = &decodeAndAppendMapEntriesString;
    informationMapEntries.value.arg = &allInformationPayload;
    pubmsg.information.entries.funcs.decode = &decodeMapEntries;
    pubmsg.information.entries.arg = &informationMapEntries;

    //propriedades
    MapEntry propertiesMapEntries = MapEntry_init_zero;
    char allProperties[size+size/2];
    tPayload allPropertiesPayload = {allProperties, size, 0};
    propertiesMapEntries.key.funcs.decode = &decodeAndAppendMapEntriesString;
    propertiesMapEntries.key.arg = &allPropertiesPayload;
    propertiesMapEntries.value.funcs.decode = &decodeAndAppendMapEntriesString;
    propertiesMapEntries.value.arg = &allPropertiesPayload;
    pubmsg.properties.entries.funcs.decode = &decodeMapEntries;
    pubmsg.properties.entries.arg = &propertiesMapEntries;


    pb_istream_t inStream = pb_istream_from_buffer(data, size);
    if (!pb_decode(&inStream, PublishInformation_fields, &pubmsg)){
        printf("Erro decodificando PublishInformation: %s\n", inStream.errmsg);
        return;
    }

    printf("--------InformationData--------\n");
    printf("Information class: %s\n", informationClass);
    int infoHash = abs(java_hashCode(informationClass));
    printf("Information class hashcode (sddl group): %d\n", infoHash);
    char hash[20];
    sprintf(hash, "%d", infoHash);
    infoHash = abs(java_hashCode(hash));
    printf("Information hashcode: %d\n", infoHash);
    if (pubmsg.has_originUuid){
        printf("Origin UUID: %lx%lx\n", pubmsg.originUuid.mostSignBits, pubmsg.originUuid.leastSignBits);
    }

    if (pubmsg.has_information){
        const char *k;
        const char *v;
        int i = 0;
        printf("Informations:\n");
        while(1){
            k = getKeyByIndex(&allInformationPayload, i);
            v = getValueByIndex(&allInformationPayload, i);
            if (!k || !v){
                break;
            }
            printf("\t\"%s\" = \"%s\"\n", k, v);
            i++;
        }
    }

    if (pubmsg.has_properties){
        const char *k;
        const char *v;
        int i = 0;
        printf("Properties:\n");
        while(1){
            k = getKeyByIndex(&allPropertiesPayload, i);
            v = getValueByIndex(&allPropertiesPayload, i);
            if (!k || !v){
                break;
            }
            printf("\t\"%s\" = \"%s\"\n", k, v);
            i++;
        }
    }
    printf("--------------------------------\n");

}

static void decodeSubscriptionMessage(char *data, int size)
{
    SubscribeInformation subscription = SubscribeInformation_init_zero;
    char topic[21];
    tPayload topicPayload = { topic, 21, 0};
    subscription.topic.funcs.decode = &decodeSingleString;
    subscription.topic.arg = &topicPayload;

    pb_istream_t inStream = pb_istream_from_buffer(data, size);
    if (!pb_decode(&inStream, SubscribeInformation_fields, &subscription)){
        printf("Erro decodificando subscription: %s\n", inStream.errmsg);
        return;
    }

    printf("new subscription to topic \"%s\".\n", topic);
}

static void decodeGroupMembershipMessage(char *data, int size)
{
    GroupMembership groupMembership = GroupMembership_init_zero;
    Group joinedGroup = Group_init_zero;
    Group leftGroup = Group_init_zero;
    groupMembership.groupsJoined.funcs.decode = &decodeGroup;
    groupMembership.groupsJoined.arg = &joinedGroup;
    groupMembership.groupsLeft.funcs.decode = &decodeGroup;
    groupMembership.groupsLeft.arg = &leftGroup;
    pb_istream_t inStream = pb_istream_from_buffer(data, size);
    if (!pb_decode(&inStream, GroupMembership_fields, &groupMembership)){
        printf("Erro decodificando groupmembership: %s\n", inStream.errmsg);
        return;
    }

    if (joinedGroup.groupType != 0){
        printf("entrou no grupo [%d/%d]\n", joinedGroup.groupID, joinedGroup.groupType);
    }
    if (leftGroup.groupType != 0){
        printf("saiu do grupo [%d/%d]\n", leftGroup.groupID, leftGroup.groupType);
    }
}

static void decodeProtocolMessage(char *data, int size)
{
    printf("New message:\n");

    ClientLibMessage msg = ClientLibMessage_init_zero;
    uint8_t payloadBuffer[size];
    tPayload payload = {payloadBuffer, size, 0};
    msg.contentPayload.funcs.decode = &decodePayloadBytes;
    msg.contentPayload.arg = &payload;

    pb_istream_t inStream = pb_istream_from_buffer(data, size);
    if (!pb_decode(&inStream, ClientLibMessage_fields, &msg)){
        printf("Erro decodificando mensagem recebida: %s\n", inStream.errmsg);
        return;
    }
    char uuid[41];
    uuidToString(&msg.senderID, uuid);
    printf("Sender UUID: %s (formatted)\n", uuid);

    printf("Sender UUID: %016lx%016lx\n", msg.senderID.mostSignBits, msg.senderID.leastSignBits);
    printf("MSGType: %d\n", msg.msgType);
    printf("Payload has %d bytes\n", payload.size);

    switch (msg.msgType){
        case MSGType_PUBSUBEVENT:
            puts("type is pubsub event");
            decodePubSubEvent(payload.data, payload.size);
            break;
        case MSGType_SUBSCRIPTION:
            puts("type is subscription event");
            decodeSubscriptionMessage(payload.data, payload.size);
            break;
        case MSGType_GROUPMEMBERSHIP:
            puts("type is groupmembership");
            decodeGroupMembershipMessage(payload.data, payload.size);
            break;
        default:
            printf("type of message not compatible\n");
    }

}

static void publishInformation(const dyad_Stream *s, const char *topic, const char *mainValue) {
    time_t creationTime = time(NULL);
    //criar a publishinformation
    PublishInformation info = PublishInformation_init_zero;

    //informacoes basicas
    info.informationClass.funcs.encode = &encodeSingleString;
    info.informationClass.arg = topic;
    info.has_originUuid = true;
    info.originUuid.leastSignBits = myUuid.leastSignBits;
    info.originUuid.mostSignBits = myUuid.mostSignBits;

    //informacoes
    info.has_information = true;
    char allInformation[MAX_ENTRIES_STRSIZE];
    tPayload allInformationPayload = {allInformation, MAX_ENTRIES_STRSIZE, 0};
    if (!appendToMapEntryString(&allInformationPayload, "main", mainValue)){
        printf("Error appending main value\n");
    }
    if (!appendToMapEntryString(&allInformationPayload, "bairro", "portal do sol")){
        printf("Error appending bairro\n");
    }
    info.information.entries.funcs.encode = &encodeMapEntries;
    info.information.entries.arg = &allInformationPayload;

    int i;

    //propriedades
    info.has_properties = true;
    char allProperties[MAX_ENTRIES_STRSIZE];
    tPayload allPropertiesPayload = {allProperties, MAX_ENTRIES_STRSIZE, 0};
    char creationTimeStr[21];
    sprintf(creationTimeStr, "%d", creationTime);
    if (!appendToMapEntryString(&allPropertiesPayload, "creationTime", creationTimeStr)){
        printf("Error appending creationTime\n");
    }
    if (!appendToMapEntryString(&allPropertiesPayload, "publicationTime", creationTimeStr)){
        printf("Error appending publicationTime\n");
    }
    char originUUID[41];
    uuidToString(&myUuid, originUUID);
    if (!appendToMapEntryString(&allPropertiesPayload, "originUuid", originUUID)){
        printf("Error appending originUuid\n");
    }
    if (!appendToMapEntryString(&allPropertiesPayload, "senderUuid", originUUID)){
        printf("Error appending senderUuid\n");
    }
    info.properties.entries.funcs.encode = &encodeMapEntries;
    info.properties.entries.arg = &allPropertiesPayload;


    //encapsular pubsub em um clientlib
    ClientLibMessage pubsubMsg = ClientLibMessage_init_default;
    pubsubMsg.msgType = MSGType_PUBSUBEVENT;

    pubsubMsg.contentPayload.funcs.encode = &encodePublishInformation; //talvez tenha que ser uma completa e nao como campo
    pubsubMsg.contentPayload.arg = &info;

    pubsubMsg.has_contentType = true;
    pubsubMsg.contentType = PayloadSerialization_PROTOCOLBUFFER;
    pubsubMsg.has_senderID = true;
    pubsubMsg.senderID.leastSignBits = myUuid.leastSignBits;
    pubsubMsg.senderID.mostSignBits = myUuid.mostSignBits;

    tArrayStrings pubsubTags = {{"_pubsubAPI"}, 1};
    pubsubMsg.tagList.funcs.encode = &encodeTagList;
    pubsubMsg.tagList.arg = &pubsubTags;


    //preparar e colocar em um groupcast
    Groupcast groupcastMessage = Groupcast_init_default;

    Group gr = Group_init_default;
    gr.groupID = abs(java_hashCode(topic)); //id do grupo eh o hash do topico
    gr.groupType = PUBSUB_DATA_GROUP; //controles sao enviadas nesse tipo
    groupcastMessage.groupsToSend.funcs.encode = &encodeGroup;
    groupcastMessage.groupsToSend.arg = &gr;

    groupcastMessage.content.funcs.encode = &encodeClientLibMessage;
    groupcastMessage.content.arg = &pubsubMsg;


    //encapsular clientlib final
    ClientLibMessage msg = ClientLibMessage_init_default;
    msg.msgType = MSGType_GROUPCAST;

    msg.contentPayload.funcs.encode = &encodeGroupcast;
    msg.contentPayload.arg = &groupcastMessage;

    msg.has_contentType = true;
    msg.contentType = PayloadSerialization_PROTOCOLBUFFER;
    msg.has_senderID = true;
    msg.senderID.leastSignBits = myUuid.leastSignBits;
    msg.senderID.mostSignBits = myUuid.mostSignBits;

    tArrayStrings groupcastTags = {{ "_default", "_groupAPI"}, 2};
    msg.tagList.funcs.encode = &encodeTagList;
    msg.tagList.arg = &groupcastTags;


    uint8_t msgBuffer[MSG_ENCODING_BUFFER_SIZE];
    pb_ostream_t stream;
    stream.max_size = MSG_ENCODING_BUFFER_SIZE;

    stream = pb_ostream_from_buffer(msgBuffer, MSG_ENCODING_BUFFER_SIZE);
    if (!pb_encode(&stream, ClientLibMessage_fields, &msg)){
        puts("erro no encode do ClientLibMsg");
        exit(1);
    }
    //enviar
    printf("enviar %d bytes\n", stream.bytes_written);
    char iniMsg[MSG_HEADER_BUFFER_SIZE];
    sprintf(iniMsg, "%d|%d|%s", 4, stream.bytes_written, "NULL");
    //dyad_write(s, iniMsg, strlen(iniMsg));
    //dyad_write(s, msgBuffer, stream.bytes_written);
    puts("publicado");

}

static void registerPublisherOf(const dyad_Stream *s, const char *topic) {
    printf("will register as publisher of: %s\n", topic);

    GroupMembership sub = GroupMembership_init_default; //declara e inicia com a macro que zera os campos
    Group gr = Group_init_default; //idem
    gr.groupID = abs(java_hashCode(topic)); //id do grupo eh o hash do topico
    gr.groupType = PUBSUB_CONTROL_GROUP; //msg de controle sao enviadas nesse tipo

    sub.groupsJoined.funcs.encode = &encodeGroup;
    sub.groupsJoined.arg = &gr;
    //sub.groupsLeft.funcs.encode = &encodeGroup;
    //sub.groupsLeft.arg = NULL;

    ClientLibMessage msg = ClientLibMessage_init_default;
    msg.msgType = MSGType_GROUPMEMBERSHIP;

    msg.contentPayload.funcs.encode = &encodeGroupMembership; //talvez tenha que ser uma completa e nao como campo
    msg.contentPayload.arg = &sub;

    msg.has_contentType = true;
    msg.contentType = PayloadSerialization_PROTOCOLBUFFER;
    msg.has_senderID = true;
    msg.senderID.leastSignBits = myUuid.leastSignBits;
    msg.senderID.mostSignBits = myUuid.mostSignBits;

    tArrayStrings arrayString = {{ "_default", "_groupAPI" }, 2};
    msg.tagList.funcs.encode = &encodeTagList;
    msg.tagList.arg = &arrayString;


    uint8_t msgBuffer[MSG_ENCODING_BUFFER_SIZE];
    pb_ostream_t stream;
    stream.max_size = MSG_ENCODING_BUFFER_SIZE;

    stream = pb_ostream_from_buffer(msgBuffer, MSG_ENCODING_BUFFER_SIZE);
    if (!pb_encode(&stream, ClientLibMessage_fields, &msg)){
        puts("erro no encode do ClientLibMsg");
        exit(1);
    }
    //enviar
    printf("enviar %d bytes\n", stream.bytes_written);
    char iniMsg[MSG_HEADER_BUFFER_SIZE];
    sprintf(iniMsg, "%d|%d|%s", 4, stream.bytes_written, "NULL");
    //dyad_write(s, iniMsg, strlen(iniMsg));
    //dyad_write(s, msgBuffer, stream.bytes_written);
    puts("enviado");
}

static void sendSubscriptionNotification(const dyad_Stream *s,  const char *topic){
    //criar a mensagem de subinscricao q sera enviada aos publicadores
    SubscribeInformation subInfo = SubscribeInformation_init_default;

    subInfo.topic.funcs.encode = &encodeSingleString;
    subInfo.topic.arg = topic;

    //encapsular numa clientlib message
    ClientLibMessage pubsubMsg = ClientLibMessage_init_default;
    pubsubMsg.msgType = MSGType_SUBSCRIPTION;

    pubsubMsg.contentPayload.funcs.encode = &encodeSubscriptionInformation; //talvez tenha que ser uma completa e nao como campo
    pubsubMsg.contentPayload.arg = &subInfo;

    pubsubMsg.has_contentType = true;
    pubsubMsg.contentType = PayloadSerialization_PROTOCOLBUFFER;
    pubsubMsg.has_senderID = true;
    pubsubMsg.senderID.leastSignBits = myUuid.leastSignBits;
    pubsubMsg.senderID.mostSignBits = myUuid.mostSignBits;

    tArrayStrings pubsubTags = {{"_pubsubAPI"}, 1};
    pubsubMsg.tagList.funcs.encode = &encodeTagList;
    pubsubMsg.tagList.arg = &pubsubTags;

    //encapcular em um groucast para grupo dos publicadodes
    Groupcast groupcastMessage = Groupcast_init_default;

    Group gr = Group_init_default;
    gr.groupID = abs(java_hashCode(topic)); //id do grupo eh o hash do topico
    gr.groupType = PUBSUB_CONTROL_GROUP; //controles sao enviadas nesse tipo
    groupcastMessage.groupsToSend.funcs.encode = &encodeGroup;
    groupcastMessage.groupsToSend.arg = &gr;

    groupcastMessage.content.funcs.encode = &encodeClientLibMessage;
    groupcastMessage.content.arg = &pubsubMsg;

    //encapcular groupcast no clientlib
    ClientLibMessage msg = ClientLibMessage_init_default;
    msg.msgType = MSGType_GROUPCAST;

    msg.contentPayload.funcs.encode = &encodeGroupcast;
    msg.contentPayload.arg = &groupcastMessage;

    msg.has_contentType = true;
    msg.contentType = PayloadSerialization_PROTOCOLBUFFER;
    msg.has_senderID = true;
    msg.senderID.leastSignBits = myUuid.leastSignBits;
    msg.senderID.mostSignBits = myUuid.mostSignBits;

    tArrayStrings groupcastTags = {{ "_default", "_groupAPI"}, 2};
    msg.tagList.funcs.encode = &encodeTagList;
    msg.tagList.arg = &groupcastTags;


    uint8_t msgBuffer[MSG_ENCODING_BUFFER_SIZE];
    pb_ostream_t stream;
    stream.max_size = MSG_ENCODING_BUFFER_SIZE;

    stream = pb_ostream_from_buffer(msgBuffer, MSG_ENCODING_BUFFER_SIZE);
    if (!pb_encode(&stream, ClientLibMessage_fields, &msg)){
        puts("erro no encode do ClientLibMsg");
        exit(1);
    }
    //enviar
    printf("enviar %d bytes\n", stream.bytes_written);
    char iniMsg[MSG_HEADER_BUFFER_SIZE];
    sprintf(iniMsg, "%d|%d|%s", 4, stream.bytes_written, "NULL");
    //dyad_write(s, iniMsg, strlen(iniMsg));
    //dyad_write(s, msgBuffer, stream.bytes_written);
    puts("enviado");
}

static void subscribeTo(const dyad_Stream *s,  const char *topic) {
    printf("will subscribe to: %s\n", topic);

    GroupMembership sub = GroupMembership_init_default; //declara e inicia com a macro que zera os campos
    Group gr = Group_init_default; //idem
    gr.groupID = abs(java_hashCode(topic)); //id do grupo eh o hash do topico
    gr.groupType = PUBSUB_DATA_GROUP; //notificacoes sao enviadas nesse tipo

    sub.groupsJoined.funcs.encode = &encodeGroup;
    sub.groupsJoined.arg = &gr;
    //sub.groupsLeft.funcs.encode = &encodeGroup;
    //sub.groupsLeft.arg = NULL;

    ClientLibMessage msg = ClientLibMessage_init_default;
    msg.msgType = MSGType_GROUPMEMBERSHIP;

    msg.contentPayload.funcs.encode = &encodeGroupMembership; //talvez tenha que ser uma completa e nao como campo
    msg.contentPayload.arg = &sub;

    msg.has_contentType = true;
    msg.contentType = PayloadSerialization_PROTOCOLBUFFER;
    msg.has_senderID = true;
    msg.senderID.leastSignBits = myUuid.leastSignBits;
    msg.senderID.mostSignBits = myUuid.mostSignBits;

    tArrayStrings arrayString = {{ "_default", "_groupAPI" }, 2};
    msg.tagList.funcs.encode = &encodeTagList;
    msg.tagList.arg = &arrayString;


    uint8_t msgBuffer[MSG_ENCODING_BUFFER_SIZE];
    pb_ostream_t stream;
    stream.max_size = MSG_ENCODING_BUFFER_SIZE;

    stream = pb_ostream_from_buffer(msgBuffer, MSG_ENCODING_BUFFER_SIZE);
    if (!pb_encode(&stream, ClientLibMessage_fields, &msg)){
        puts("erro no encode do ClientLibMsg");
        exit(1);
    }
    //enviar
    printf("enviar %d bytes\n", stream.bytes_written);
    char iniMsg[MSG_HEADER_BUFFER_SIZE];
    sprintf(iniMsg, "%d|%d|%s", 4, stream.bytes_written, "NULL");
    //dyad_write(s, iniMsg, strlen(iniMsg));
    //dyad_write(s, msgBuffer, stream.bytes_written);
    puts("enviado");

    sendSubscriptionNotification(s, topic);
}

void setup(){
}

void loop(){
}
