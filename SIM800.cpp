#include "SIM800.h"

SIM800::SIM800(PinName pwr, PinName st):power(pwr, 0), strt(st, 1){
    simRegistered = false;
}

void SIM800::powerON(){
    power = 1;
}

void SIM800::powerOFF(){
    power = 0;
}

void SIM800::enable(){
    strt = 0;
    wait_us(500000);
    strt = 1;
}

int SIM800::start() {
    int retries = 0, r = 0;
    while (true) {
        Watchdog::get_instance().kick();
        simRegistered = false;
        powerOFF();
        wait_us(1000000);
        powerON();
        wait_us(1000000);
        enable();
        pc.printf("Registering sim800 on network...");
        wait_us(5000000);
        int r = 0;
        while (true) {
            Watchdog::get_instance().kick();
            if(ser.sendCmdAndWaitForResp((char*)"AT+CREG?\r\n", (char*)"+CREG: 0,1", 0, 1000) == 0){
                simRegistered = true;
                break;
            }
            if(r == 20){
                r = 0;
                break;
            }
            wait_us(1000000);
            r++;
        }
        if (simRegistered) {
            pc.printf("Done\n");
            wait_us(2000000);
            return 0;
        }
        else{
            pc.printf("Failed!\n");
            retries++;
        }
        if (retries == 4) {
            pc.printf("SIM failed to start.\n");
            return -1;
        }
    }
}

int SIM800::disableEcho() {
    pc.printf("ATE0\n");
    if(ser.sendCmdAndWaitForResp((char*)"ATE0\r\n", (char*)"OK", 0, 1000) == 0){
        pc.printf("OK\n");
        return 0;
    }else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }
}

int SIM800::checkSim(){
    if(ser.sendCmdAndWaitForResp((char*)"AT+CREG?\r\n", (char*)"+CREG: 0,1", 0, 5000) == 0){
        simRegistered = true;
        return 0;
    }
    else{
        simRegistered = false;
        return -1;
    }
}

int SIM800::setGPRSSettings(){
    pc.printf("AT+CGATT?\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+CGATT?\r\n", (char*)"+CGATT: 1", 0, 1000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }

    pc.printf("AT+CIPSHUT\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+CIPSHUT\r\n", (char*)"SHUT OK", 0, 1000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }

    pc.printf("AT+SAPBR=3,1,Contype,GPRS\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+SAPBR=3,1,Contype,GPRS\r\n", (char*)"OK", 0, 1000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }

    pc.printf("AT+SAPBR=3,1,APN,www\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+SAPBR=3,1,APN,www\r\n", (char*)"OK", 0, 1000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }
    return 0;
}

int SIM800::startGPRS(){
    pc.printf("AT+SAPBR=1,1\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+SAPBR=1,1\r\n", (char*)"OK", 0, 3000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }
    return 0;
}

int SIM800::stopGPRS(){
    pc.printf("AT+SAPBR=0,1\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+SAPBR=0,1\r\n", (char*)"OK", 0, 10000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }
    return 0;
}

int SIM800::initHTTP(){
    pc.printf("AT+HTTPINIT\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+HTTPINIT\r\n", (char*)"OK", 0, 3000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }

    pc.printf("AT+HTTPPARA=CID,1\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+HTTPPARA=CID,1\r\n", (char*)"OK", 0, 5000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }

    pc.printf("AT+HTTPPARA=URL, optimems.net/Hajmi/settings.php\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+HTTPPARA=URL, optimems.net/Hajmi/settings.php\r\n", (char*)"OK", 0, 5000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }

    pc.printf("AT+HTTPACTION=0\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+HTTPACTION=0\r\n", (char*)"+HTTPACTION: 0,200,43", 0, 20000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }
    return 0;
}

int SIM800::termHTTP(){
    pc.printf("AT+HTTPTERM\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+HTTPTERM\r\n", (char*)"OK", 0, 2000) == 0){
        pc.printf("OK\n");
        return 0;
    } else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }

}

int SIM800::setFormat(){
    pc.printf("AT+CMGF=1\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+CMGF=1\r\n", (char*)"OK", 0, 1000) == 0){
        pc.printf("OK\n");
        return 0;
    }
    else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }
}

int SIM800::initFTP(){
    pc.printf("AT+FTPCID=1\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+FTPCID=1\r\n", (char*)"OK", 0, 1000) == 0){
        pc.printf("OK\n");
    }
    else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }
    pc.printf("AT+FTPSERV=\"ftp.optimems.net\"\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+FTPSERV=\"ftp.optimems.net\"\r\n", (char*)"OK", 0, 1000) == 0){
        pc.printf("OK\n");
    }
    else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }
    pc.printf("AT+FTPUN=\"rtu@optimems.net\"\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+FTPUN=\"rtu@optimems.net\"\r\n", (char*)"OK", 0, 1000) == 0){
        pc.printf("OK\n");
    }
    else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }
    pc.printf("AT+FTPPW=\"Miouch13\"\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+FTPPW=\"Miouch13\"\r\n", (char*)"OK", 0, 1000) == 0){
        pc.printf("OK\n");
    }
    else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }
    pc.printf("AT+FTPPUTPATH=\"/\"\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+FTPPUTPATH=\"/\"\r\n", (char*)"OK", 0, 1000) == 0){
        pc.printf("OK\n");
    }
    else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }
    return 0;
}

int SIM800::setFTPFileName(char* filename){
    pc.printf("AT+FTPPUTNAME=\"%s\"\n", filename);
    char str[40];
    sprintf(str, "AT+FTPPUTNAME=\"%s\"\r\n", filename);
    if(ser.sendCmdAndWaitForResp(str, (char*)"OK", 0, 1000) == 0){
        pc.printf("OK\n");
        return 0;
    }
    else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }
}

int SIM800::startFTP(){
    pc.printf("AT+FTPPUT=1\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+FTPPUT=1\r\n", (char*)"+FTPPUT: 1,1,1360", 0, 10000) == 0){
        pc.printf("OK\n");
        return 0;
    }
    else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }
}

int SIM800::FTPsend(char* data, int ftp_size){
    pc.printf("AT+FTPPUT=2,%d\n", ftp_size);
    char str[20];
    sprintf(str, "AT+FTPPUT=2,%d\r\n", ftp_size);
    char result[20];
    sprintf(result, "+FTPPUT: 2,%d", ftp_size);
    if(ser.sendCmdAndWaitForResp(str, result, 0, 2000) == 0){
        pc.printf("+FTPPUT: 2,%d\n", ftp_size);
    }
    else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }
    if(ser.sendCmdAndWaitForResp(data, (char*)"OK", 0, 2000) == 0){
        pc.printf("OK\n");
        return 0;
    }
    else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }
}

int SIM800::stopFTP(){
    pc.printf("AT+FTPPUT=2,0\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+FTPPUT=2,0\r\n", (char*)"OK", 0, 1) == 0){
        pc.printf("OK\n");
        return 0;
    }
    else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }
}

int SIM800::sendSMS(char* text, char* phoneNumber){
    pc.printf("Sending sms '%s' to '%s'\n", text, phoneNumber);
    pc.printf("AT+CSMP=17,167,0,0\n");
    if (ser.sendCmdAndWaitForResp((char*)"AT+CSMP=17,167,0,0\r\n", (char*)"OK", 0, 1000) == 0) {
        pc.printf("OK\n");
    } else {
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }
    pc.printf("AT+CMGS=\"%s\" %s <CTRL-Z>", phoneNumber, text);
    char txt[30];
    sprintf(txt, "AT+CMGS=\"%s\"\r\n", phoneNumber);
    if (ser.sendCmdAndWaitForResp(txt, (char*)">", 0, 1000) == 0) {
        sprintf(txt, "%s %c", text, 26);
        if (ser.sendCmdAndWaitForResp(txt, (char*)"+CMGS:", 0, 10000) == 0) {
            pc.printf("SMS sent\n");
            return 0;
        }
        else {
            pc.printf("Failed to send the sms\n");
            return -1;
        }
    }
    else {
        pc.printf("SIM is not ready to send sms\n");
        return -2;
    }
}

int SIM800::deleteSMS(int indx){
    pc.printf("Deleting SMS number %d\n", indx);
    pc.printf("AT+CMGD=%d\n", indx);
    char text[15];
    sprintf(text, "AT+CMGD=%d\r\n", indx);
    if(ser.sendCmdAndWaitForResp(text, (char*)"OK", 0, 1000) == 0){
        pc.printf("Done\n");
        return 0;
    }
    else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return -1;
    }
}

void SIM800::simSleep(){
    pc.printf("AT+CSCLK=2\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+CSCLK=2\r\n", (char*)"OK", 0, 10000) == 0){
        pc.printf("OK\n");
    }
    else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
    }
}

void SIM800::simWake(){
    pc.printf("AT\n");
    ser.sendCmd((char*)"AT\r\n");
    pc.printf("AT+CSCLK=0\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+CSCLK=0\r\n", (char*)"OK", 0, 1000) == 0){
        pc.printf("OK\n");
    }
    else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
    }
}

string SIM800::getSignalQuality(){
    pc.printf("AT+CSQ\n");
    if (ser.sendCmdAndWaitForResp((char*)"AT+CSQ\r\n", (char*)"+CSQ:", -1, 1000) != 0) {
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return "-1";
    }
    string output = ser.receivedData;
    int u = output.find(":");
    output = output.substr(u+2);
    u = output.find(",");
    output = output.substr(0, u);
    return output;
}

string SIM800::readSMS(int indx){
    char text[15];
    sprintf(text, "AT+CMGR=%d\n", indx);
    if(ser.sendCmdAndWaitForResp(text, (char*)"+CMGR:", -1, 1000) != 0){
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return "-1";
    }
    else{
        return ser.receivedData;
    }
}

string SIM800::readTimeStamp(){
    pc.printf("AT+HTTPREAD\n");
    if(ser.sendCmdAndWaitForResp((char*)"AT+HTTPREAD\r\n", (char*)"+HTTPREAD:", -1, 3000) == 0){
        pc.printf("OK\n");
    }else{
        pc.printf("%s - %s\n", ser.receivedOutput.c_str(), ser.receivedData.c_str());
        return "-1";
    }

    int i=ser.receivedData.find("#");
    string s = ser.receivedData.substr(i+1, 41);
    for (size_t i = 0; i < 4; i++) {
        int v = s.find("%");
        s = s.substr(v+1);
    }
    int v = s.find("%");
    s = s.substr(0, v);
    return s;
}
