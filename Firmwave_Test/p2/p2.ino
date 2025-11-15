

void setup() {
  Serial.begin(115200);
}

//-------------------------- Form hệ thống yêu cầu------------------
unsigned int count_runs = 6; // Chọn số lần vòng loop
bool Start_System = false;
void Script_System( const char* user_name ){
  count_runs--;

  if(Start_System == 0) {
     Start_System = true;
     Serial.println("");
     Serial.println("____Start run partition_" + String(user_name)+"____");
  }

  if(count_runs == 0 ){
    Serial.println("");
    Serial.println("____End run partition_"+ String(user_name)+"____");
    ESP.restart();
    }
}
//-------------------------- Form hệ thống yêu cầu------------------


void loop() {
  // Hàm yêu cầu phải có.
  Script_System("hs2");
  
  // Chương trình ứng dụng viết trong đây
  Serial.println("p2");
  delay(1000);

}
