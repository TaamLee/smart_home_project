# smart_home_project

Các bước chạy code:
1. Tải code 
2. Tải SDK từ ESP-IDF v5.0.1 tại đây https://docs.espressif.com/projects/esp-idf/en/v5.0.1/esp32/get-started/windows-setup.html
   Sau khi tải xong sẽ có terminal ESP_IDF 5.0 powershell
3. Vào ESP_IDF 5.0 powershell và di chuyển tới thư mục cần nạp code (ví dụ như Device1_coap, Device2_mqtt, Homecenter)
   ex: cd C:\Users\Admin\Documents\smarthome\Device1_coap
4. chạy: idf.py fullclean
   sau đó build chương trình: idf.py build
   rồi nạp code xuống kit bằng lệnh: idf.py -p COMx flash monitor
   COMx: cổng kết nối với ESP32


